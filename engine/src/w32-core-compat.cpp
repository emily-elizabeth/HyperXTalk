/* Copyright (C) 2016 LiveCode Ltd.

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

#include "prefix.h"

#include "globals.h"
#include "uidc.h"
#include "platform.h"

// ── ISpellChecker COM interfaces (Windows 8+) ────────────────────────────────
//
// <spellcheck.h> is gated on NTDDI_WIN8, but the project targets _WIN32_WINNT
// 0x0601 (Windows 7) for graceful degradation.  We therefore declare the
// interfaces inline with explicit GUIDs — the same technique used by
// w32-notification.cpp for WinRT interfaces.
//
// GUIDs match the published Windows SDK (all SDK versions 8.0+):
//   CLSID_SpellCheckerFactory  {7AB36653-1796-484B-BDFA-E74F1DB7C1DC}
//   ISpellCheckerFactory       {8E018A9D-2415-4677-BF08-794EA61F94BB}
//   ISpellChecker              {B6FD0B71-E2BC-4653-8D05-F197E412770B}
//   IEnumSpellingError         {803E3BD4-2828-4410-8290-418D1D73C762}
//   ISpellingError             {B7C82D61-FBE8-4B47-9B27-6C0D2E0DE0A3}
//
// Only the vtable slots we actually call need to be declared; later slots in
// the real implementation are not affected.

static const CLSID s_CLSID_SpellCheckerFactory =
    {0x7ab36653, 0x1796, 0x484b, {0xbd, 0xfa, 0xe7, 0x4f, 0x1d, 0xb7, 0xc1, 0xdc}};

// Forward declarations for types referenced by interface methods we skip.
struct IEnumString;
struct ISpellCheckerChangedEventHandler;
struct IOptionDescription;

// CORRECTIVE_ACTION — only the enum tag is needed (vtable placeholder param).
typedef enum CORRECTIVE_ACTION
{
    CORRECTIVE_ACTION_NONE              = 0,
    CORRECTIVE_ACTION_GET_SUGGESTIONS   = 1,
    CORRECTIVE_ACTION_REPLACE           = 2,
    CORRECTIVE_ACTION_DELETE            = 3,
} CORRECTIVE_ACTION;

struct ISpellingError;
struct IEnumSpellingError;
struct ISpellChecker;

// ISpellingError — vtable layout (IUnknown + 4 own methods):
//   [3] get_StartIndex, [4] get_Length, [5] get_CorrectiveAction, [6] get_Replacement
MIDL_INTERFACE("B7C82D61-FBE8-4B47-9B27-6C0D2E0DE0A3")
ISpellingError : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_StartIndex(
        /* [out] */ ULONG *value) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_Length(
        /* [out] */ ULONG *value) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_CorrectiveAction(
        /* [out] */ CORRECTIVE_ACTION *value) = 0;

    virtual HRESULT STDMETHODCALLTYPE get_Replacement(
        /* [out] */ LPWSTR *value) = 0;
};

// IEnumSpellingError — vtable layout (IUnknown + 1 own method):
//   [3] Next  — returns S_OK (got one) or S_FALSE (exhausted)
MIDL_INTERFACE("803E3BD4-2828-4410-8290-418D1D73C762")
IEnumSpellingError : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Next(
        /* [out] */ ISpellingError **value) = 0;
};

// ISpellChecker — we declare only through Check() (slot [4]).
// The real interface has ~14 methods; slots beyond [4] are unused here.
//   [3] get_LanguageTag, [4] Check
MIDL_INTERFACE("B6FD0B71-E2BC-4653-8D05-F197E412770B")
ISpellChecker : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_LanguageTag(
        /* [out] */ LPWSTR *value) = 0;

    virtual HRESULT STDMETHODCALLTYPE Check(
        /* [in]  */ LPCWSTR             text,
        /* [out] */ IEnumSpellingError **value) = 0;
};

// ISpellCheckerFactory — full declaration (3 own methods used: IsSupported,
// CreateSpellChecker; get_SupportedLanguages must be declared first to keep
// vtable offsets correct).
//   [3] get_SupportedLanguages, [4] IsSupported, [5] CreateSpellChecker
MIDL_INTERFACE("8E018A9D-2415-4677-BF08-794EA61F94BB")
ISpellCheckerFactory : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_SupportedLanguages(
        /* [out] */ IEnumString **value) = 0;

    virtual HRESULT STDMETHODCALLTYPE IsSupported(
        /* [in]  */ LPCWSTR languageTag,
        /* [out] */ BOOL   *value) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateSpellChecker(
        /* [in]  */ LPCWSTR       languageTag,
        /* [out] */ ISpellChecker **value) = 0;
};

// ── ITaskbarList3 COM interface (Windows 7+) ──────────────────────────────────
//
// Like ISpellChecker above, we define the interface inline so that
// _WIN32_WINNT=0x0601 doesn't cause problems with version-guarded SDK headers.
//
// GUIDs from the published Windows SDK (all versions):
//   CLSID_TaskbarList  {56FDF344-FD6D-11d0-958A-006097C9A090}
//   ITaskbarList       {56FDF342-FD6D-11d0-958A-006097C9A090}
//   ITaskbarList2      {602D4995-B13A-429b-A66E-1935E44F4317}
//   ITaskbarList3      {EA1AFB91-9E28-4B86-90E9-9E9F8A5EEFAF}

static const CLSID s_CLSID_TaskbarList =
    {0x56fdf344, 0xfd6d, 0x11d0, {0x95, 0x8a, 0x00, 0x60, 0x97, 0xc9, 0xa0, 0x90}};

typedef enum TBPFLAG
{
    TBPF_NOPROGRESS    = 0,
    TBPF_INDETERMINATE = 0x1,
    TBPF_NORMAL        = 0x2,
    TBPF_ERROR         = 0x4,
    TBPF_PAUSED        = 0x8,
} TBPFLAG;

// ITaskbarList — base interface; only HrInit() is needed.
MIDL_INTERFACE("56FDF342-FD6D-11d0-958A-006097C9A090")
ITaskbarList : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE HrInit(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddTab(HWND hwnd) = 0;
    virtual HRESULT STDMETHODCALLTYPE DeleteTab(HWND hwnd) = 0;
    virtual HRESULT STDMETHODCALLTYPE ActivateTab(HWND hwnd) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetActiveAlt(HWND hwnd) = 0;
};

// ITaskbarList2 — declares MarkFullscreenWindow; must be in vtable.
MIDL_INTERFACE("602D4995-B13A-429b-A66E-1935E44F4317")
ITaskbarList2 : public ITaskbarList
{
public:
    virtual HRESULT STDMETHODCALLTYPE MarkFullscreenWindow(HWND hwnd, BOOL fFullscreen) = 0;
};

// ITaskbarList3 — full vtable through SetOverlayIcon (method index 9 within
// ITaskbarList3's own methods; 0-indexed from the start of ITaskbarList3).
// Placeholder signatures for unused slots use void* to avoid header deps.
MIDL_INTERFACE("EA1AFB91-9E28-4B86-90E9-9E9F8A5EEFAF")
ITaskbarList3 : public ITaskbarList2
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetProgressValue(HWND hwnd, ULONGLONG ullCompleted, ULONGLONG ullTotal) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProgressState(HWND hwnd, TBPFLAG tbpFlags) = 0;
    virtual HRESULT STDMETHODCALLTYPE RegisterTab(HWND hwndTab, HWND hwndMDI) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnregisterTab(HWND hwndTab) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetTabOrder(HWND hwndTab, HWND hwndInsertBefore) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetTabActiveThumb(HWND hwndTab, HWND hwndMDI) = 0;
    virtual HRESULT STDMETHODCALLTYPE ThumbBarAddButtons(HWND hwnd, UINT cButtons, void *pButton) = 0;
    virtual HRESULT STDMETHODCALLTYPE ThumbBarUpdateButtons(HWND hwnd, UINT cButtons, void *pButton) = 0;
    virtual HRESULT STDMETHODCALLTYPE ThumbBarSetImageList(HWND hwnd, void *himl) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOverlayIcon(HWND hwnd, HICON hIcon, LPCWSTR pszDescription) = 0;
};

////////////////////////////////////////////////////////////////////////////////
// MCPlatform* compatability functions
//   Implementation of MCPlatform* functions required for MCPlatformPlayer implementation.

void MCPlatformBreakWait(void)
{
	MCscreen->pingwait();
}

// ── Windows spell checking via ISpellChecker ─────────────────────────────────
//
// Strategy mirrors mac-core.mm / NSSpellChecker:
//   1. Lazily create one ISpellChecker instance for the user's default locale
//      and cache it for the lifetime of the process.
//   2. For each call, convert the MCStringRef to a LPCWSTR and pass it to
//      ISpellChecker::Check(), which returns an IEnumSpellingError.
//   3. Walk the error enumerator, collecting (start, length) pairs that
//      correspond directly to UTF-16 code-unit positions — the same unit that
//      MCStringRef uses on Windows — so no position conversion is needed.
//   4. Return a heap-allocated MCRange[] that the caller owns (and frees with
//      delete[], matching the Mac side).
//
// Failure modes:
//   • If the user's locale has no registered spell-check provider, IsSupported
//     returns FALSE and we fall back to r_count == 0.
//   • If COM is not yet initialized on the calling thread, CoCreateInstance
//     will fail and we fall back silently.

void MCPlatformSpellCheckText(MCStringRef p_text, MCRange*& r_errors, uindex_t& r_count)
{
    r_errors = nil;
    r_count  = 0;

    if (!p_text || MCStringIsEmpty(p_text))
        return;

    // Lazy-init: create one ISpellChecker for the user's default locale.
    static ISpellChecker *s_checker   = NULL;
    static bool           s_init_done = false;

    if (!s_init_done)
    {
        s_init_done = true;

        ISpellCheckerFactory *t_factory = NULL;
        HRESULT t_hr = CoCreateInstance(s_CLSID_SpellCheckerFactory,
                                        NULL, CLSCTX_INPROC_SERVER,
                                        __uuidof(ISpellCheckerFactory),
                                        (void **)&t_factory);
        if (SUCCEEDED(t_hr) && t_factory != NULL)
        {
            wchar_t t_locale[LOCALE_NAME_MAX_LENGTH] = {};
            if (GetUserDefaultLocaleName(t_locale, LOCALE_NAME_MAX_LENGTH) > 0)
            {
                BOOL t_supported = FALSE;
                t_hr = t_factory->IsSupported(t_locale, &t_supported);
                if (SUCCEEDED(t_hr) && t_supported)
                    t_factory->CreateSpellChecker(t_locale, &s_checker);
            }
            t_factory->Release();
        }
    }

    if (s_checker == NULL)
        return;

    // Convert MCStringRef to a NUL-terminated wchar_t string.
    // MCStringConvertToWString allocates a unichar_t (= uint16_t) buffer;
    // on Windows wchar_t is also 16-bit, so the reinterpret_cast below is safe.
    unichar_t *t_wbuf = NULL;
    if (!MCStringConvertToWString(p_text, t_wbuf))
        return;

    IEnumSpellingError *t_enum = NULL;
    HRESULT t_check_hr = s_checker->Check(reinterpret_cast<LPCWSTR>(t_wbuf), &t_enum);
    MCMemoryDeallocate(t_wbuf);

    if (FAILED(t_check_hr) || t_enum == NULL)
        return;

    // Walk the error enumerator into a growable MCRange array.
    uindex_t  t_capacity = 8;
    uindex_t  t_count    = 0;
    MCRange  *t_errors   = new (nothrow) MCRange[t_capacity];
    if (!t_errors)
    {
        t_enum->Release();
        return;
    }

    ISpellingError *t_error = NULL;
    while (t_enum->Next(&t_error) == S_OK)
    {
        ULONG t_start  = 0;
        ULONG t_length = 0;
        t_error->get_StartIndex(&t_start);
        t_error->get_Length(&t_length);
        t_error->Release();
        t_error = NULL;

        // Grow the array if needed (doubling strategy).
        if (t_count == t_capacity)
        {
            uindex_t  t_new_cap = t_capacity * 2;
            MCRange  *t_grown   = new (nothrow) MCRange[t_new_cap];
            if (!t_grown)
                break;
            for (uindex_t i = 0; i < t_count; i++)
                t_grown[i] = t_errors[i];
            delete[] t_errors;
            t_errors   = t_grown;
            t_capacity = t_new_cap;
        }

        t_errors[t_count].offset = (uindex_t)t_start;
        t_errors[t_count].length = (uindex_t)t_length;
        t_count++;
    }
    t_enum->Release();

    if (t_count == 0)
    {
        delete[] t_errors;
        return;
    }

    r_errors = t_errors;
    r_count  = t_count;
}

// ── Taskbar progress (ITaskbarList3) ─────────────────────────────────────────

void MCPlatformSetTaskbarProgress(void *p_hwnd, double p_value)
{
    if (p_hwnd == nil)
        return;

    HWND t_hwnd = (HWND)p_hwnd;

    // Lazy-init: create one ITaskbarList3 for the process lifetime.
    static ITaskbarList3 *s_taskbar   = NULL;
    static bool           s_init_done = false;

    if (!s_init_done)
    {
        s_init_done = true;
        ITaskbarList3 *t_tbl = NULL;
        HRESULT t_hr = CoCreateInstance(s_CLSID_TaskbarList, NULL,
                                        CLSCTX_INPROC_SERVER,
                                        __uuidof(ITaskbarList3),
                                        (void **)&t_tbl);
        if (SUCCEEDED(t_hr) && t_tbl != NULL)
        {
            if (SUCCEEDED(t_tbl->HrInit()))
                s_taskbar = t_tbl;
            else
                t_tbl->Release();
        }
    }

    if (s_taskbar == NULL)
        return;

    if (p_value < 0.0)
    {
        // Indeterminate spinner.
        s_taskbar->SetProgressState(t_hwnd, TBPF_INDETERMINATE);
    }
    else if (p_value == 0.0)
    {
        // Hide the progress bar.
        s_taskbar->SetProgressState(t_hwnd, TBPF_NOPROGRESS);
    }
    else
    {
        // Normal fill — clamp to [0,1].
        double t_clamped = p_value > 1.0 ? 1.0 : p_value;
        s_taskbar->SetProgressState(t_hwnd, TBPF_NORMAL);
        s_taskbar->SetProgressValue(t_hwnd,
                                    (ULONGLONG)(t_clamped * 10000.0),
                                    (ULONGLONG)10000);
    }
}

// ── Taskbar overlay icon (ITaskbarList3::SetOverlayIcon) ─────────────────────

void MCPlatformSetTaskbarOverlayIcon(void *p_hwnd, MCStringRef p_icon_path)
{
    if (p_hwnd == nil)
        return;

    HWND t_hwnd = (HWND)p_hwnd;

    // Lazy-init: create one ITaskbarList3 for the process lifetime.
    static ITaskbarList3 *s_taskbar   = NULL;
    static bool           s_init_done = false;

    if (!s_init_done)
    {
        s_init_done = true;
        ITaskbarList3 *t_tbl = NULL;
        HRESULT t_hr = CoCreateInstance(s_CLSID_TaskbarList, NULL,
                                        CLSCTX_INPROC_SERVER,
                                        __uuidof(ITaskbarList3),
                                        (void **)&t_tbl);
        if (SUCCEEDED(t_hr) && t_tbl != NULL)
        {
            if (SUCCEEDED(t_tbl->HrInit()))
                s_taskbar = t_tbl;
            else
                t_tbl->Release();
        }
    }

    if (s_taskbar == NULL)
        return;

    // Empty path — clear the overlay icon.
    if (p_icon_path == nil || MCStringIsEmpty(p_icon_path))
    {
        s_taskbar->SetOverlayIcon(t_hwnd, NULL, NULL);
        return;
    }

    // Load the icon from the file path.
    unichar_t *t_wpath = NULL;
    if (!MCStringConvertToWString(p_icon_path, t_wpath))
        return;

    HICON t_icon = (HICON)LoadImageW(NULL,
                                     reinterpret_cast<LPCWSTR>(t_wpath),
                                     IMAGE_ICON, 0, 0,
                                     LR_LOADFROMFILE | LR_DEFAULTSIZE);
    MCMemoryDeallocate(t_wpath);

    if (t_icon == NULL)
        return;

    s_taskbar->SetOverlayIcon(t_hwnd, t_icon, NULL);
    DestroyIcon(t_icon);
}

// ── Taskbar badge (ITaskbarList3::SetOverlayIcon + GDI) ───────────────────────
//
// Dynamically paints a red circle with white count text onto a 16×16 HICON,
// then sets it as the taskbar button's overlay icon.
//
// Icon layout:
//   Colour plane : red circle (RGB CC 00 00) with white text.
//   Mask plane   : 1-bit monochrome; 0 = opaque, 1 = transparent.
//   Both planes use the same bounding ellipse so edges align exactly.

static HICON CreateBadgeIcon(UINT p_count)
{
    const int W = 16, H = 16;

    // ── Colour plane ─────────────────────────────────────────────────────────
    HDC     t_screen  = GetDC(NULL);
    HDC     t_col_dc  = CreateCompatibleDC(t_screen);
    HBITMAP t_col_bmp = CreateCompatibleBitmap(t_screen, W, H);
    ReleaseDC(NULL, t_screen);

    if (t_col_dc == NULL || t_col_bmp == NULL)
    {
        if (t_col_dc)  DeleteDC(t_col_dc);
        if (t_col_bmp) DeleteObject(t_col_bmp);
        return NULL;
    }

    HBITMAP t_old_col = (HBITMAP)SelectObject(t_col_dc, t_col_bmp);
    RECT    t_all     = {0, 0, W, H};
    HPEN    t_no_pen  = (HPEN)GetStockObject(NULL_PEN);

    // Black background (masked transparent outside the circle).
    FillRect(t_col_dc, &t_all, (HBRUSH)GetStockObject(BLACK_BRUSH));

    // Red filled circle.
    HBRUSH t_red = CreateSolidBrush(RGB(0xCC, 0x00, 0x00));
    SelectObject(t_col_dc, t_no_pen);
    SelectObject(t_col_dc, t_red);
    Ellipse(t_col_dc, 0, 0, W, H);
    DeleteObject(t_red);

    // White count text — shrink font for three-character strings ("99+").
    wchar_t t_text[5] = {};
    if (p_count > 99)
        wcscpy_s(t_text, L"99+");
    else
        swprintf_s(t_text, 5, L"%u", p_count);

    int   t_fh   = (wcslen(t_text) >= 3) ? -6 : -9;
    HFONT t_font = CreateFontW(t_fh, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    SetBkMode(t_col_dc, TRANSPARENT);
    SetTextColor(t_col_dc, RGB(0xFF, 0xFF, 0xFF));
    HFONT t_old_font = (HFONT)SelectObject(t_col_dc, t_font);
    DrawTextW(t_col_dc, t_text, -1, &t_all, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(t_col_dc, t_old_font);
    DeleteObject(t_font);

    SelectObject(t_col_dc, t_old_col);
    DeleteDC(t_col_dc);

    // ── Mask plane (1-bit): 0 = opaque, 1 = transparent ──────────────────────
    HDC     t_mask_dc  = CreateCompatibleDC(NULL);
    HBITMAP t_mask_bmp = CreateBitmap(W, H, 1, 1, NULL);

    if (t_mask_dc == NULL || t_mask_bmp == NULL)
    {
        if (t_mask_dc)  DeleteDC(t_mask_dc);
        if (t_mask_bmp) DeleteObject(t_mask_bmp);
        DeleteObject(t_col_bmp);
        return NULL;
    }

    HBITMAP t_old_mask = (HBITMAP)SelectObject(t_mask_dc, t_mask_bmp);

    // White fill (transparent) then black ellipse (opaque circle region).
    FillRect(t_mask_dc, &t_all, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SelectObject(t_mask_dc, t_no_pen);
    SelectObject(t_mask_dc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    Ellipse(t_mask_dc, 0, 0, W, H);

    SelectObject(t_mask_dc, t_old_mask);
    DeleteDC(t_mask_dc);

    // ── Assemble HICON ────────────────────────────────────────────────────────
    ICONINFO t_ii = {};
    t_ii.fIcon    = TRUE;
    t_ii.hbmColor = t_col_bmp;
    t_ii.hbmMask  = t_mask_bmp;
    HICON t_icon  = CreateIconIndirect(&t_ii);

    DeleteObject(t_col_bmp);
    DeleteObject(t_mask_bmp);
    return t_icon;
}

void MCPlatformSetBadge(void *p_hwnd, uinteger_t p_count)
{
    if (p_hwnd == nil)
        return;

    HWND t_hwnd = (HWND)p_hwnd;

    static ITaskbarList3 *s_taskbar   = NULL;
    static bool           s_init_done = false;

    if (!s_init_done)
    {
        s_init_done = true;
        ITaskbarList3 *t_tbl = NULL;
        HRESULT t_hr = CoCreateInstance(s_CLSID_TaskbarList, NULL,
                                        CLSCTX_INPROC_SERVER,
                                        __uuidof(ITaskbarList3),
                                        (void **)&t_tbl);
        if (SUCCEEDED(t_hr) && t_tbl != NULL)
        {
            if (SUCCEEDED(t_tbl->HrInit()))
                s_taskbar = t_tbl;
            else
                t_tbl->Release();
        }
    }

    if (s_taskbar == NULL)
        return;

    if (p_count == 0)
    {
        // Zero clears the badge.
        s_taskbar->SetOverlayIcon(t_hwnd, NULL, NULL);
        return;
    }

    HICON t_icon = CreateBadgeIcon((UINT)p_count);
    if (t_icon == NULL)
        return;

    s_taskbar->SetOverlayIcon(t_hwnd, t_icon, NULL);
    DestroyIcon(t_icon);
}

void MCPlatformShareContent(MCPlatformWindowRef, MCPlatformShareType, MCValueRef, bool, MCRectangle, MCStringRef)
{
    // Not supported on Windows.
}

bool MCPlatformWaitForEvent(double duration, bool blocking)
{
	bool t_dispatch = !blocking;
	return MCscreen->wait(duration, t_dispatch, false);
}

////////////////////////////////////////////////////////////////////////////////
