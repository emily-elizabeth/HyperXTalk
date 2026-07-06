/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

//
// Windows ToolbarWindow32 backend for MCToolbar.
//

#ifdef _WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
// GDI+ requires OLE/COM types (MIDL_INTERFACE, PROPID, IUnknown, etc.) that
// WIN32_LEAN_AND_MEAN strips from <windows.h>.  Pull them in explicitly before
// including <gdiplus.h>, otherwise the SDK headers fail to parse.
#include <objbase.h>
#include <gdiplus.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")

#include "prefix.h"
#include "toolbar.h"

using namespace Gdiplus;

////////////////////////////////////////////////////////////////////////////////

// Maximum number of toolbar items (expand if needed)
#define W32_TOOLBAR_MAX_ITEMS 256

// WM_USER message sent from the toolbar subclass proc to route item clicks
#define WM_TOOLBAR_ITEM_CLICKED (WM_USER + 1)

// Standard icon size used when populating the image list
#define W32_ICON_SIZE 24

// Per-item data stored alongside TBBUTTON
struct W32ToolbarItemData
{
    MCNewAutoNameRef  name;
    MCAutoStringRef   label;
    MCAutoStringRef   tooltip;
    MCAutoStringRef   icon;
    bool              enabled;
    int               icon_img_idx; // index in m_image_list, or -1 if none

    W32ToolbarItemData() : enabled(true), icon_img_idx(-1) {}

    // MCAutoValueRefBase members delete the implicit copy-assignment operator.
    // Provide an explicit one so the shift-down loop in RemoveItem() can use =.
    // Use Reset() rather than operator=(T) because Reset() handles non-nil
    // destination values correctly (release old, retain new), whereas
    // operator=(T) asserts m_value==nil and is only valid for initial assignment.
    W32ToolbarItemData& operator=(const W32ToolbarItemData& rhs)
    {
        if (this != &rhs)
        {
            name   .Reset(*rhs.name);
            label  .Reset(*rhs.label);
            tooltip.Reset(*rhs.tooltip);
            icon   .Reset(*rhs.icon);
            enabled      = rhs.enabled;
            icon_img_idx = rhs.icon_img_idx;
        }
        return *this;
    }
};

////////////////////////////////////////////////////////////////////////////////
// PNG → HBITMAP helper (GDI+, scaled to p_size x p_size)

static HBITMAP _bitmapFromPNGData(const void *p_bytes, uindex_t p_length,
                                   int p_size)
{
    if (!p_bytes || p_length == 0 || p_size <= 0)
        return NULL;

    // Wrap the raw bytes in an IStream
    HGLOBAL t_mem = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)p_length);
    if (!t_mem)
        return NULL;

    void *t_ptr = GlobalLock(t_mem);
    if (!t_ptr)
    {
        GlobalFree(t_mem);
        return NULL;
    }
    memcpy(t_ptr, p_bytes, p_length);
    GlobalUnlock(t_mem);

    IStream *t_stream = NULL;
    // TRUE = IStream takes ownership of t_mem and frees it on Release
    if (FAILED(CreateStreamOnHGlobal(t_mem, TRUE, &t_stream)))
    {
        GlobalFree(t_mem);
        return NULL;
    }

    // Decode via GDI+ (initialised in Create() via GdiplusStartup)
    Bitmap *t_src = Bitmap::FromStream(t_stream);
    t_stream->Release(); // also frees t_mem

    if (!t_src || t_src->GetLastStatus() != Ok)
    {
        delete t_src;
        return NULL;
    }

    // Scale into a fresh 32-bit ARGB bitmap of the target size
    Bitmap t_scaled(p_size, p_size, PixelFormat32bppARGB);
    {
        Graphics g(&t_scaled);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        Rect t_dst(0, 0, p_size, p_size);
        g.DrawImage(t_src, t_dst);
    }
    delete t_src;

    if (t_scaled.GetLastStatus() != Ok)
        return NULL;

    HBITMAP t_hbmp = NULL;
    // Transparent background for per-pixel alpha; ILC_COLOR32 image lists on
    // Vista+ handle pre-multiplied alpha from GetHBITMAP correctly.
    t_scaled.GetHBITMAP(Color(0, 0, 0, 0), &t_hbmp);
    return t_hbmp;
}

////////////////////////////////////////////////////////////////////////////////

class MCToolbarWin32Backend : public MCToolbarBackend
{
public:
    MCToolbarWin32Backend(MCToolbar *p_owner)
        : m_owner(p_owner), m_hwnd_toolbar(NULL), m_hwnd_parent(NULL),
          m_image_list(NULL), m_item_count(0), m_visible(true),
          m_gdip_token(0)
    {
        memset(m_buttons, 0, sizeof(m_buttons));
        // Note: m_item_data elements are already default-constructed by C++
        // (all MCAutoValueRef members = nil).  The memset is redundant but
        // harmless — both paths leave every pointer at zero/null.
        memset(m_item_data, 0, sizeof(m_item_data));
    }

    ~MCToolbarWin32Backend() override {}

    void Create(void *p_window_handle) override
    {
        // On Windows the engine passes a _Drawable * (not a raw HWND).
        // Unpack the actual client HWND from the drawable struct.
        if (!p_window_handle)
            return;
        _Drawable *t_drawable = reinterpret_cast<_Drawable *>(p_window_handle);
        m_hwnd_parent = reinterpret_cast<HWND>(t_drawable->handle.window);
        if (!m_hwnd_parent)
            return;

        // Ensure Common Controls are initialised
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(icex);
        icex.dwICC  = ICC_BAR_CLASSES;
        InitCommonControlsEx(&icex);

        // Initialise GDI+ for PNG icon decoding.  GdiplusStartup must be
        // called before any Bitmap/Graphics usage; pair with GdiplusShutdown
        // in Destroy().
        if (!m_gdip_token)
        {
            Gdiplus::GdiplusStartupInput t_input;
            Gdiplus::GdiplusStartup(&m_gdip_token, &t_input, nullptr);
        }

        // Register this backend on the parent HWND so MCWindowProc can route
        // WM_COMMAND button-click notifications back to HandleCommand().
        SetPropA(m_hwnd_parent, "MCToolbar", (HANDLE)this);

        m_hwnd_toolbar = CreateWindowEx(
            0,
            TOOLBARCLASSNAME,
            NULL,
            WS_CHILD | WS_VISIBLE |
            TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NODIVIDER | CCS_NOPARENTALIGN,
            0, 0, 0, 0,
            m_hwnd_parent,
            (HMENU)1,
            GetModuleHandle(NULL),
            NULL);

        if (!m_hwnd_toolbar)
            return;

        // Required for TBBUTTON structs
        SendMessage(m_hwnd_toolbar, TB_BUTTONSTRUCTSIZE,
                    (WPARAM)sizeof(TBBUTTON), 0);

        // 32-bit ARGB image list for icons
        m_image_list = ImageList_Create(W32_ICON_SIZE, W32_ICON_SIZE,
                                        ILC_COLOR32 | ILC_MASK, 8, 8);
        SendMessage(m_hwnd_toolbar, TB_SETIMAGELIST, 0,
                    (LPARAM)m_image_list);

        // Initial sizing and positioning.  TB_AUTOSIZE computes the toolbar
        // height; _reposition() then sets x, y and width explicitly so that
        // the toolbar appears below any in-window menu bar rather than always
        // landing at y=0.
        SendMessage(m_hwnd_toolbar, TB_AUTOSIZE, 0, 0);
        _reposition();

        if (!m_visible)
            ShowWindow(m_hwnd_toolbar, SW_HIDE);
    }

    void Destroy() override
    {
        // Unregister the parent HWND property before destroying the toolbar
        // so MCWindowProc stops routing WM_COMMAND to a dead backend.
        if (m_hwnd_parent)
            RemovePropA(m_hwnd_parent, "MCToolbar");

        if (m_hwnd_toolbar)
        {
            DestroyWindow(m_hwnd_toolbar);
            m_hwnd_toolbar = NULL;
        }
        if (m_image_list)
        {
            ImageList_Destroy(m_image_list);
            m_image_list = NULL;
        }
        // Release value refs without calling the explicit destructor —
        // m_item_data is a member array so C++ will call dtors automatically
        // when the backend is destroyed.  Explicit dtor + auto dtor = double-free.
        // Assignment to a default-constructed instance properly releases old
        // values and leaves the object in a safe (all-nil) state for the later
        // auto-destructor call.
        for (int i = 0; i < m_item_count; i++)
            m_item_data[i] = W32ToolbarItemData();
        m_item_count = 0;

        if (m_gdip_token)
        {
            Gdiplus::GdiplusShutdown(m_gdip_token);
            m_gdip_token = 0;
        }
    }

    void AddItem(const MCToolbarItem *p_item) override
    {
        if (!m_hwnd_toolbar || m_item_count >= W32_TOOLBAR_MAX_ITEMS)
            return;

        int t_idx = m_item_count;

        // Populate per-item data using Reset() which properly handles
        // nil→value transitions and (for safety) any non-nil existing values.
        // Do NOT use placement-new here — the array elements are already
        // default-constructed, and the ctor's op=(T) asserts m_value==nil
        // which would fire if the slot was previously used.
        m_item_data[t_idx].name   .Reset(p_item->GetName());
        m_item_data[t_idx].label  .Reset(p_item->GetLabel());
        m_item_data[t_idx].tooltip.Reset(p_item->GetTooltip());
        m_item_data[t_idx].icon   .Reset(nil);   // icon set below via image data path
        m_item_data[t_idx].enabled      = p_item->GetEnabled();
        m_item_data[t_idx].icon_img_idx = -1;

        // Build TBBUTTON
        TBBUTTON &btn = m_buttons[t_idx];
        memset(&btn, 0, sizeof(btn));

        MCToolbarItemStyle t_style = p_item->GetStyle();
        if (t_style == kMCToolbarItemStyleSeparator)
        {
            btn.fsStyle = TBSTYLE_SEP;
            btn.iBitmap = 8; // separator width in pixels
        }
        else
        {
            btn.idCommand = t_idx + 1; // command IDs are 1-based
            btn.fsStyle   = BTNS_BUTTON | BTNS_AUTOSIZE;
            btn.fsState   = p_item->GetEnabled() ? TBSTATE_ENABLED : 0;
            btn.iBitmap   = I_IMAGENONE;

            // Icon: load from cached PNG data if available
            MCDataRef t_img_data = p_item->GetImageData();
            if (t_img_data != nil && !MCDataIsEmpty(t_img_data) && m_image_list)
            {
                HBITMAP t_hbmp = _bitmapFromPNGData(
                    MCDataGetBytePtr(t_img_data),
                    (uindex_t)MCDataGetLength(t_img_data),
                    W32_ICON_SIZE);
                if (t_hbmp)
                {
                    int t_img_idx = ImageList_Add(m_image_list, t_hbmp, NULL);
                    DeleteObject(t_hbmp);
                    if (t_img_idx >= 0)
                    {
                        btn.iBitmap = t_img_idx;
                        m_item_data[t_idx].icon_img_idx = t_img_idx;
                    }
                }
            }

        }

        SendMessage(m_hwnd_toolbar, TB_ADDBUTTONS, 1, (LPARAM)&btn);
        m_item_count++;

        // Set the button label via TB_SETBUTTONINFOW — this takes a direct
        // LPCWSTR and handles all Unicode code points correctly.  Using
        // TB_ADDSTRING + iString (the string-pool approach) has encoding
        // quirks that corrupt non-ASCII text loaded from a stack file.
        if (t_style != kMCToolbarItemStyleSeparator &&
            p_item->GetLabel() && !MCStringIsEmpty(p_item->GetLabel()))
        {
            MCAutoStringRefAsWString t_wlabel;
            if (t_wlabel.Lock(p_item->GetLabel()))
            {
                TBBUTTONINFOW tbi = {};
                tbi.cbSize  = sizeof(tbi);
                tbi.dwMask  = TBIF_TEXT;
                tbi.pszText = const_cast<LPWSTR>(*t_wlabel);
                SendMessage(m_hwnd_toolbar, TB_SETBUTTONINFOW,
                            (WPARAM)(t_idx + 1), (LPARAM)&tbi);
            }
        }

        SendMessage(m_hwnd_toolbar, TB_AUTOSIZE, 0, 0);
        _reposition();
    }

    void RemoveItem(MCNameRef p_name) override
    {
        if (!m_hwnd_toolbar)
            return;

        for (int i = 0; i < m_item_count; i++)
        {
            if (MCNameIsEqualTo(*m_item_data[i].name, p_name,
                                kMCCompareCaseless))
            {
                SendMessage(m_hwnd_toolbar, TB_DELETEBUTTON, i, 0);
                // No explicit destructor here — the copy assignment in the
                // shift loop below will properly release slot i's old values.

                // Shift remaining items down
                for (int j = i; j < m_item_count - 1; j++)
                {
                    m_buttons[j]   = m_buttons[j + 1];
                    m_item_data[j] = m_item_data[j + 1];
                    // Re-sync 1-based command IDs
                    m_buttons[j].idCommand = j + 1;
                }

                // The tail slot is now a duplicate of [m_item_count-2].
                // Release its refs via assignment to a default instance so
                // the auto-destructor later sees a clean nil-valued object.
                m_item_data[m_item_count - 1] = W32ToolbarItemData();

                m_item_count--;
                SendMessage(m_hwnd_toolbar, TB_AUTOSIZE, 0, 0);
                _reposition();
                return;
            }
        }
    }

    void UpdateItem(const MCToolbarItem *p_item) override
    {
        if (!m_hwnd_toolbar)
            return;

        for (int i = 0; i < m_item_count; i++)
        {
            if (MCNameIsEqualTo(*m_item_data[i].name, p_item->GetName(),
                                kMCCompareCaseless))
            {
                int t_cmd_id = i + 1; // 1-based command ID

                // Enabled state
                LPARAM t_state = p_item->GetEnabled() ? TBSTATE_ENABLED : 0;
                SendMessage(m_hwnd_toolbar, TB_SETSTATE,
                            (WPARAM)t_cmd_id, t_state);
                m_item_data[i].enabled = p_item->GetEnabled();

                // Label
                if (p_item->GetLabel())
                {
                    MCAutoStringRefAsWString t_wlabel;
                    if (t_wlabel.Lock(p_item->GetLabel()))
                    {
                        TBBUTTONINFOW tbi = {};
                        tbi.cbSize  = sizeof(tbi);
                        tbi.dwMask  = TBIF_TEXT;
                        tbi.pszText = const_cast<LPWSTR>(*t_wlabel);
                        SendMessage(m_hwnd_toolbar, TB_SETBUTTONINFOW,
                                    (WPARAM)t_cmd_id, (LPARAM)&tbi);
                    }
                    m_item_data[i].label.Reset(p_item->GetLabel());
                }

                // Tooltip — stored for TTN_NEEDTEXT; display requires the
                // parent window's WM_NOTIFY handler to forward the notification.
                if (p_item->GetTooltip())
                    m_item_data[i].tooltip.Reset(p_item->GetTooltip());

                // Icon: reload from cached PNG data
                MCDataRef t_img_data = p_item->GetImageData();
                if (t_img_data != nil && !MCDataIsEmpty(t_img_data) && m_image_list)
                {
                    HBITMAP t_hbmp = _bitmapFromPNGData(
                        MCDataGetBytePtr(t_img_data),
                        (uindex_t)MCDataGetLength(t_img_data),
                        W32_ICON_SIZE);
                    if (t_hbmp)
                    {
                        int t_new_idx = -1;
                        int t_existing = m_item_data[i].icon_img_idx;

                        if (t_existing >= 0)
                        {
                            // Replace existing slot in-place to preserve index
                            if (ImageList_Replace(m_image_list, t_existing,
                                                  t_hbmp, NULL))
                                t_new_idx = t_existing;
                        }
                        if (t_new_idx < 0)
                            t_new_idx = ImageList_Add(m_image_list, t_hbmp, NULL);

                        DeleteObject(t_hbmp);

                        if (t_new_idx >= 0)
                        {
                            m_item_data[i].icon_img_idx = t_new_idx;
                            TBBUTTONINFOW tbi = {};
                            tbi.cbSize  = sizeof(tbi);
                            tbi.dwMask  = TBIF_IMAGE;
                            tbi.iImage  = t_new_idx;
                            SendMessage(m_hwnd_toolbar, TB_SETBUTTONINFOW,
                                        (WPARAM)t_cmd_id, (LPARAM)&tbi);
                        }
                    }
                }
                else if (m_item_data[i].icon_img_idx >= 0)
                {
                    // Icon was cleared
                    TBBUTTONINFOW tbi = {};
                    tbi.cbSize  = sizeof(tbi);
                    tbi.dwMask  = TBIF_IMAGE;
                    tbi.iImage  = I_IMAGENONE;
                    SendMessage(m_hwnd_toolbar, TB_SETBUTTONINFOW,
                                (WPARAM)t_cmd_id, (LPARAM)&tbi);
                    m_item_data[i].icon_img_idx = -1;
                }

                return;
            }
        }
    }

    void ClearItems() override
    {
        if (!m_hwnd_toolbar)
            return;

        while (m_item_count > 0)
        {
            SendMessage(m_hwnd_toolbar, TB_DELETEBUTTON, 0, 0);
            --m_item_count;
            // Use assignment (not explicit dtor) to release value refs safely.
            m_item_data[m_item_count] = W32ToolbarItemData();
        }
        // Flush the image list so indices reset for the next batch of items
        if (m_image_list)
            ImageList_Remove(m_image_list, -1);

        SendMessage(m_hwnd_toolbar, TB_AUTOSIZE, 0, 0);
        _reposition();
    }

    void SetDisplayMode(MCToolbarDisplayMode p_mode) override
    {
        if (!m_hwnd_toolbar)
            return;

        LONG t_style = GetWindowLong(m_hwnd_toolbar, GWL_STYLE);
        // Remove existing label/image flags
        t_style &= ~(TBSTYLE_LIST);

        switch (p_mode)
        {
            case kMCToolbarDisplayModeIconOnly:
                // No labels: strip TBSTYLE_LIST, items show icon only
                break;
            case kMCToolbarDisplayModeLabelOnly:
                // Show text only — remove images
                SendMessage(m_hwnd_toolbar, TB_SETIMAGELIST, 0, (LPARAM)NULL);
                t_style |= TBSTYLE_LIST;
                break;
            case kMCToolbarDisplayModeIconAndLabel:
            case kMCToolbarDisplayModeDefault:
            default:
                SendMessage(m_hwnd_toolbar, TB_SETIMAGELIST, 0,
                            (LPARAM)m_image_list);
                t_style |= TBSTYLE_LIST;
                break;
        }
        SetWindowLong(m_hwnd_toolbar, GWL_STYLE, t_style);
        SendMessage(m_hwnd_toolbar, TB_AUTOSIZE, 0, 0);
        _reposition();
    }

    void SetVisible(bool p_visible) override
    {
        m_visible = p_visible;
        if (m_hwnd_toolbar)
            ShowWindow(m_hwnd_toolbar, p_visible ? SW_SHOW : SW_HIDE);
    }

    bool GetVisible() override
    {
        return m_visible;
    }

    // Called from the parent window's WndProc when WM_COMMAND is received
    void HandleCommand(WPARAM wParam)
    {
        int t_id = LOWORD(wParam) - 1; // back to 0-based
        if (t_id < 0 || t_id >= m_item_count)
            return;
        if (m_owner && *m_item_data[t_id].name)
            m_owner->itemClicked(*m_item_data[t_id].name);
    }

    // Called by MCWin32ToolbarHandleParentCommand — checks the sender HWND so
    // the free function never needs to touch the private m_hwnd_toolbar member.
    void HandleParentCommand(HWND p_sender, WPARAM p_wparam)
    {
        if (p_sender == m_hwnd_toolbar)
            HandleCommand(p_wparam);
    }

    // Called by MCWin32ToolbarHandleParentResize when the parent window is
    // resized (WM_SIZE), so the toolbar tracks the parent's width.
    void HandleParentResize()
    {
        _reposition();
    }

private:
    // Position and size the toolbar so that it spans the full client width,
    // placed immediately below any in-window menu bar.  Must be called after
    // every TB_AUTOSIZE because CCS_NOPARENTALIGN disables comctl32's automatic
    // layout.
    void _reposition()
    {
        if (!m_hwnd_toolbar || !m_hwnd_parent)
            return;

        // Full client width of the parent.
        RECT t_client = {};
        GetClientRect(m_hwnd_parent, &t_client);
        int t_width = t_client.right;

        // Toolbar height as computed by the last TB_AUTOSIZE.
        RECT t_tb = {};
        GetWindowRect(m_hwnd_toolbar, &t_tb);
        int t_height = t_tb.bottom - t_tb.top;

        // y-offset: bottom edge of the in-window menu bar group (0 if none).
        // This mirrors the getnextscroll() logic used on macOS.
        int t_y = (m_owner != nil) ? (int)m_owner->getToolbarTopY() : 0;

        SetWindowPos(m_hwnd_toolbar, NULL,
                     0, t_y, t_width, t_height,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    MCToolbar              *m_owner;
    HWND                    m_hwnd_toolbar;
    HWND                    m_hwnd_parent;
    HIMAGELIST              m_image_list;
    int                     m_item_count;
    bool                    m_visible;
    ULONG_PTR               m_gdip_token;  // GDI+ token from GdiplusStartup
    TBBUTTON                m_buttons[W32_TOOLBAR_MAX_ITEMS];
    W32ToolbarItemData      m_item_data[W32_TOOLBAR_MAX_ITEMS];
};

////////////////////////////////////////////////////////////////////////////////
// Factory

MCToolbarBackend *MCToolbarCreatePlatformBackend(MCToolbar *p_owner)
{
    return new MCToolbarWin32Backend(p_owner);
}

////////////////////////////////////////////////////////////////////////////////
// WM_COMMAND routing — called from MCWindowProc in w32dcw32.cpp
//
// When the user clicks a toolbar button, comctl32's toolbar WndProc calls
// SendMessage(parent, WM_COMMAND, MAKEWPARAM(idCommand,0), (LPARAM)toolbar_hwnd)
// synchronously.  MCWindowProc has no WM_COMMAND case, so this free function
// is forward-declared in w32dcw32.cpp and called from there.
//
// The backend pointer is stored on the parent HWND via SetPropA in Create().

void MCWin32ToolbarHandleParentCommand(HWND p_parent, HWND p_sender,
                                       WPARAM p_wparam)
{
    MCToolbarWin32Backend *t_self =
        reinterpret_cast<MCToolbarWin32Backend *>(GetPropA(p_parent, "MCToolbar"));
    if (t_self != nullptr)
        t_self->HandleParentCommand(p_sender, p_wparam);
}

// Called from the WM_SIZE handler in w32dcw32.cpp so the toolbar tracks the
// parent window's width and stays at the correct y offset below the menu bar.
void MCWin32ToolbarHandleParentResize(HWND p_parent)
{
    MCToolbarWin32Backend *t_self =
        reinterpret_cast<MCToolbarWin32Backend *>(GetPropA(p_parent, "MCToolbar"));
    if (t_self != nullptr)
        t_self->HandleParentResize();
}

#endif // _WINDOWS
