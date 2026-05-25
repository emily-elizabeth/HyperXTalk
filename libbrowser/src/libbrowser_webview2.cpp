/* Copyright (C) 2015 LiveCode Ltd.

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

// ============================================================
// libbrowser_webview2.cpp
// Windows WebView2 backend — core implementation.
//
// Dependency notes
// ----------------
//  * Microsoft.Web.WebView2 NuGet package must be installed.
//    Add the package's build\native\include to Additional Include Directories
//    and link against WebView2Loader.dll (or the static loader lib).
//  * COM must be initialised (COINIT_APARTMENTTHREADED) on the calling thread
//    before any MCWebView2Browser method is used.  The LiveCode engine already
//    does this via CoInitializeEx in its Win32 initialisation path.
// ============================================================

#include <core.h>
#include "libbrowser_webview2.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commctrl.h>   // SetWindowSubclass / RemoveWindowSubclass
#include <shlobj.h>     // SHGetFolderPathW / CSIDL_LOCAL_APPDATA
#pragma comment(lib, "shell32.lib")  // SHGetFolderPathW
#include <wrl/client.h>
#include <wrl/event.h>
#include <WebView2.h>
#include <stdio.h>   // sprintf_s / _snwprintf_s
#include <stdlib.h>  // _wtoi, _wtof
#include <string.h>  // memcpy, strlen, wcscpy_s

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "WebView2LoaderStatic.lib")

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

// ============================================================
// Minimal growable wide-string buffer (avoids std::wstring)
// ============================================================

struct WideBuffer
{
    wchar_t *data;
    size_t   len;  // characters allocated (including NUL)

    WideBuffer() : data(nullptr), len(0) {}
    ~WideBuffer() { if (data) MCBrowserMemoryDeallocate(data); }

    bool Append(const wchar_t *p_str, size_t p_count)
    {
        size_t t_cur = data ? wcslen(data) : 0;
        size_t t_need = t_cur + p_count + 1;
        if (t_need > len)
        {
            void *t_mem;
            if (!MCBrowserMemoryAllocate(t_need * sizeof(wchar_t), t_mem))
                return false;
            if (data)
            {
                memcpy(t_mem, data, t_cur * sizeof(wchar_t));
                MCBrowserMemoryDeallocate(data);
            }
            data = (wchar_t *)t_mem;
            len  = t_need;
        }
        memcpy(data + t_cur, p_str, p_count * sizeof(wchar_t));
        data[t_cur + p_count] = L'\0';
        return true;
    }

    bool Append(const wchar_t *p_str)
    {
        return p_str ? Append(p_str, wcslen(p_str)) : true;
    }

    bool AppendChar(wchar_t c) { return Append(&c, 1); }
};

// ============================================================
// String helpers
// ============================================================

/*static*/
bool MCWebView2Browser::Utf8ToWide(const char *p_utf8, wchar_t *&r_wide)
{
    if (p_utf8 == nullptr)
    {
        r_wide = nullptr;
        return true;
    }

    int t_len = MultiByteToWideChar(CP_UTF8, 0, p_utf8, -1, nullptr, 0);
    if (t_len <= 0)
        return false;

    void *t_mem;
    if (!MCBrowserMemoryAllocate(t_len * sizeof(wchar_t), t_mem))
        return false;

    r_wide = (wchar_t *)t_mem;
    MultiByteToWideChar(CP_UTF8, 0, p_utf8, -1, r_wide, t_len);
    return true;
}

/*static*/
bool MCWebView2Browser::WideToUtf8(const wchar_t *p_wide, char *&r_utf8)
{
    if (p_wide == nullptr)
    {
        r_utf8 = nullptr;
        return true;
    }

    int t_len = WideCharToMultiByte(CP_UTF8, 0, p_wide, -1,
                                    nullptr, 0, nullptr, nullptr);
    if (t_len <= 0)
        return false;

    void *t_mem;
    if (!MCBrowserMemoryAllocate(t_len, t_mem))
        return false;

    r_utf8 = (char *)t_mem;
    WideCharToMultiByte(CP_UTF8, 0, p_wide, -1, r_utf8, t_len,
                        nullptr, nullptr);
    return true;
}

/*static*/
bool MCWebView2Browser::StringCopy(const char *p_src, char *&x_dst)
{
    if (x_dst != nullptr)
    {
        MCBrowserMemoryDeallocate(x_dst);
        x_dst = nullptr;
    }
    if (p_src == nullptr)
        return true;

    size_t t_len = strlen(p_src) + 1;
    void *t_mem;
    if (!MCBrowserMemoryAllocate(t_len, t_mem))
        return false;

    x_dst = (char *)t_mem;
    memcpy(x_dst, p_src, t_len);
    return true;
}

// ============================================================
// Message-loop pump
// ============================================================

/*static*/
bool MCWebView2Browser::PumpUntilDone(bool &r_done, DWORD p_timeout_ms)
{
    DWORD t_start = GetTickCount();

    while (!r_done)
    {
        if (p_timeout_ms != INFINITE &&
            (GetTickCount() - t_start) > p_timeout_ms)
            return false;

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                PostQuitMessage((int)msg.wParam);
                return false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Yield 5 ms so WebView2 callbacks can fire on this thread.
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    }
    return true;
}

// ============================================================
// Constructor / Destructor
// ============================================================

MCWebView2Browser::MCWebView2Browser(HWND p_parent_hwnd)
    : m_parent_hwnd(p_parent_hwnd)
    , m_host_hwnd(nullptr)
    , m_shim_script_id(nullptr)
    , m_url(nullptr)
    , m_pending_url(nullptr)
    , m_html_text(nullptr)
    , m_html_base_url(nullptr)
    , m_user_agent(nullptr)
    , m_handler_list(nullptr)
    , m_js_enabled(true)
    , m_allow_new_windows(false)
    , m_allow_user_interaction(true)
    , m_context_menu_enabled(true)
    , m_vscroll_enabled(true)
    , m_hscroll_enabled(true)
{
    MCBrowserMemoryClear(m_rect);
    MCBrowserMemoryClear(m_tok_nav_starting);
    MCBrowserMemoryClear(m_tok_nav_completed);
    MCBrowserMemoryClear(m_tok_content_loading);
    MCBrowserMemoryClear(m_tok_source_changed);
    MCBrowserMemoryClear(m_tok_web_message);
    MCBrowserMemoryClear(m_tok_new_window);
}

// ============================================================
// WM_SIZE / WM_SHOWWINDOW subclass — bounds and visibility in sync
// ============================================================
//
// Installed on m_host_hwnd (the dedicated child HWND returned by
// GetNativeLayer()).  WebView2's ICoreWebView2Controller does NOT
// automatically track its parent window's size, so on every WM_SIZE we
// call put_Bounds(GetClientRect(m_host_hwnd)).  The LiveCode native layer
// only needs to call MoveWindow(m_host_hwnd, …) and WebView2 adapts.
//
// On WM_SHOWWINDOW we synchronise put_IsVisible with the HWND visibility
// so the WebView2 DComp surface stops compositing when the widget is hidden
// (e.g. in edit mode), preventing the ghost render.

/*static*/
LRESULT CALLBACK MCWebView2Browser::SizeSubclassProc(
    HWND      hwnd,
    UINT      msg,
    WPARAM    wp,
    LPARAM    lp,
    UINT_PTR  id,
    DWORD_PTR data)
{
    MCWebView2Browser *t_browser = reinterpret_cast<MCWebView2Browser *>(data);

    if (msg == WM_SIZE && t_browser != nullptr &&
        t_browser->m_controller != nullptr)
    {
        RECT t_r;
        GetClientRect(hwnd, &t_r);
        t_browser->m_controller->put_Bounds(t_r);
    }
    else if (msg == WM_SHOWWINDOW && t_browser != nullptr &&
             t_browser->m_controller != nullptr)
    {
        // Synchronise WebView2 visibility with the host HWND visibility.
        // When MCNativeLayerWin32::doSetVisible(false) calls
        // ShowWindow(m_host_hwnd, SW_HIDE), WM_SHOWWINDOW(wParam=FALSE)
        // fires here and we call put_IsVisible(FALSE).  This stops WebView2
        // from compositing its DComp surface, eliminating the ghost render
        // that was visible even with WS_VISIBLE=0 on parent HWNDs.
        t_browser->m_controller->put_IsVisible(wp ? TRUE : FALSE);
    }
    else if (msg == WM_NCDESTROY)
    {
        RemoveWindowSubclass(hwnd, SizeSubclassProc, id);
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

// ============================================================
// Constructor / Destructor
// ============================================================

MCWebView2Browser::~MCWebView2Browser()
{
    // Remove WM_SIZE/WM_SHOWWINDOW subclass before releasing the controller.
    // The subclass is installed on m_host_hwnd (not m_parent_hwnd).
    // SizeSubclassProc auto-removes itself on WM_NCDESTROY, so this is a
    // belt-and-suspenders guard for the case where the browser is released
    // before the host HWND is destroyed (e.g. via MCBrowserRelease called
    // before set-native-layer-to-nothing).
    if (m_host_hwnd != nullptr)
        RemoveWindowSubclass(m_host_hwnd, SizeSubclassProc, k_size_subclass_id);
    // m_host_hwnd is owned by MCNativeLayerWin32 (ReleaseNativeView destroys it).
    // Do NOT call DestroyWindow(m_host_hwnd) here.

    // Unregister event handlers before releasing COM objects.
    if (m_webview != nullptr)
    {
        m_webview->remove_NavigationStarting(m_tok_nav_starting);
        m_webview->remove_NavigationCompleted(m_tok_nav_completed);
        m_webview->remove_ContentLoading(m_tok_content_loading);
        m_webview->remove_SourceChanged(m_tok_source_changed);
        m_webview->remove_WebMessageReceived(m_tok_web_message);
        m_webview->remove_NewWindowRequested(m_tok_new_window);

        if (m_shim_script_id != nullptr)
        {
            m_webview->RemoveScriptToExecuteOnDocumentCreated(m_shim_script_id);
            CoTaskMemFree(m_shim_script_id);
            m_shim_script_id = nullptr;
        }

        m_webview->Stop();
    }

    if (m_controller != nullptr)
        m_controller->Close();

    // Free all cached strings (all allocated via MCBrowserMemoryAllocate).
    if (m_url)          MCBrowserMemoryDeallocate(m_url);
    if (m_pending_url)  MCBrowserMemoryDeallocate(m_pending_url);
    if (m_html_text)    MCBrowserMemoryDeallocate(m_html_text);
    if (m_html_base_url) MCBrowserMemoryDeallocate(m_html_base_url);
    if (m_user_agent)   MCBrowserMemoryDeallocate(m_user_agent);
    if (m_handler_list) MCBrowserMemoryDeallocate(m_handler_list);
}

// ============================================================
// Host-HWND class registration
// ============================================================
//
// m_host_hwnd is a WS_CHILD of the stack window (m_parent_hwnd).  It acts
// as the container for the WebView2 controller and is the HWND that
// GetNativeLayer() returns to the LC native-layer system.
//
// We register a minimal window class "HXT_WV2HOST" on first use so we have
// full control over the window (no static-control side-effects, no default
// painting, etc.).

static bool RegisterHostWindowClass(HINSTANCE p_hinstance)
{
    static bool s_registered = false;
    if (s_registered)
        return true;

    WNDCLASSEXA t_wc = {};
    t_wc.cbSize        = sizeof(WNDCLASSEXA);
    t_wc.lpfnWndProc   = DefWindowProcA;
    t_wc.hInstance     = p_hinstance;
    t_wc.lpszClassName = "HXT_WV2HOST";
    // WS_CLIPCHILDREN so the GDI clip region for the parent is correct.
    // No background brush — WebView2 paints everything via DComp.
    t_wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    s_registered = (RegisterClassExA(&t_wc) != 0) ||
                   (GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
    return s_registered;
}

// ============================================================
// Initialize
// ============================================================

bool MCWebView2Browser::Initialize()
{
    // ------------------------------------------------------------------
    // Step 0 — Create the dedicated host HWND
    // ------------------------------------------------------------------
    // The host HWND is the window that MCNativeLayerWin32 manages (moves,
    // resizes, shows, hides).  The WebView2 controller is parented to THIS
    // window, not to m_parent_hwnd (the stack window).
    //
    // Without a separate host HWND, GetNativeLayer() would return
    // m_parent_hwnd (the stack window), and MCNativeLayerWin32 would try to
    // reparent, resize, and hide the entire stack window — causing the
    // "ghost render" (WebView2 DComp surface covering the whole app window).
    {
        HINSTANCE t_hinstance =
            (HINSTANCE)GetWindowLongPtr(m_parent_hwnd, GWLP_HINSTANCE);
        if (t_hinstance == nullptr)
            t_hinstance = GetModuleHandleA(nullptr);

        if (!RegisterHostWindowClass(t_hinstance))
            return false;

        m_host_hwnd = CreateWindowExA(
            0,
            "HXT_WV2HOST",
            "",
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 1, 1,
            m_parent_hwnd,
            nullptr,
            t_hinstance,
            nullptr);

        if (m_host_hwnd == nullptr)
            return false;
    }

    // ------------------------------------------------------------------
    // Step 1 — Create the WebView2 environment (async → pump message loop)
    // ------------------------------------------------------------------
    // IMPORTANT: Do NOT pass nullptr for the user data folder.
    // When the app is installed in C:\Program Files\..., the WebView2
    // default (exe_dir\HyperXTalk.exe.WebView2) is inside Program Files
    // and is not writable by a standard (non-admin) user, causing
    // CreateCoreWebView2EnvironmentWithOptions to fail immediately.
    // Use %LOCALAPPDATA%\HyperXTalk\EBWebView instead — always writable.
    bool t_env_done = false;
    bool t_env_ok   = false;

    // Build the user data folder path: %LOCALAPPDATA%\HyperXTalk\EBWebView
    wchar_t t_local_appdata[MAX_PATH] = {};
    wchar_t t_user_data_folder[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0,
                                   t_local_appdata)))
    {
        _snwprintf_s(t_user_data_folder, MAX_PATH, _TRUNCATE,
                     L"%s\\HyperXTalk\\EBWebView", t_local_appdata);
    }
    // If SHGetFolderPathW fails, t_user_data_folder is empty → WebView2
    // falls back to its own default (which may fail on first-run but is
    // at least no worse than passing nullptr explicitly).
    const wchar_t *t_udf_ptr =
        (t_user_data_folder[0] != L'\0') ? t_user_data_folder : nullptr;

    HRESULT t_hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,    // browser executable folder (nullptr = use system Edge)
        t_udf_ptr,  // user data folder — writable per-user location
        nullptr,    // ICoreWebView2EnvironmentOptions
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [&](HRESULT p_result,
                ICoreWebView2Environment *p_env) -> HRESULT
            {
                if (SUCCEEDED(p_result) && p_env != nullptr)
                {
                    m_environment = p_env;
                    t_env_ok = true;
                }
                t_env_done = true;
                return S_OK;
            }).Get());

    if (FAILED(t_hr) || !PumpUntilDone(t_env_done) || !t_env_ok)
        return false;

    // ------------------------------------------------------------------
    // Step 2 — Create the controller (async → pump message loop)
    // ------------------------------------------------------------------
    bool t_ctl_done = false;
    bool t_ctl_ok   = false;

    t_hr = m_environment->CreateCoreWebView2Controller(
        m_host_hwnd,   // ← parent is the dedicated host HWND, not m_parent_hwnd
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [&](HRESULT p_result,
                ICoreWebView2Controller *p_controller) -> HRESULT
            {
                if (SUCCEEDED(p_result) && p_controller != nullptr)
                {
                    m_controller = p_controller;
                    m_controller->get_CoreWebView2(&m_webview);
                    t_ctl_ok = true;
                }
                t_ctl_done = true;
                return S_OK;
            }).Get());

    if (FAILED(t_hr) || !PumpUntilDone(t_ctl_done) || !t_ctl_ok ||
        m_webview == nullptr)
        return false;

    // ------------------------------------------------------------------
    // Step 3 — Register event handlers
    // ------------------------------------------------------------------
    m_webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this](ICoreWebView2 *s,
                   ICoreWebView2NavigationStartingEventArgs *a) -> HRESULT
            { return OnNavigationStarting(s, a); }).Get(),
        &m_tok_nav_starting);

    m_webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2 *s,
                   ICoreWebView2NavigationCompletedEventArgs *a) -> HRESULT
            { return OnNavigationCompleted(s, a); }).Get(),
        &m_tok_nav_completed);

    m_webview->add_ContentLoading(
        Callback<ICoreWebView2ContentLoadingEventHandler>(
            [this](ICoreWebView2 *s,
                   ICoreWebView2ContentLoadingEventArgs *a) -> HRESULT
            { return OnContentLoading(s, a); }).Get(),
        &m_tok_content_loading);

    m_webview->add_SourceChanged(
        Callback<ICoreWebView2SourceChangedEventHandler>(
            [this](ICoreWebView2 *s,
                   ICoreWebView2SourceChangedEventArgs *a) -> HRESULT
            { return OnSourceChanged(s, a); }).Get(),
        &m_tok_source_changed);

    m_webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2 *s,
                   ICoreWebView2WebMessageReceivedEventArgs *a) -> HRESULT
            { return OnWebMessageReceived(s, a); }).Get(),
        &m_tok_web_message);

    m_webview->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [this](ICoreWebView2 *s,
                   ICoreWebView2NewWindowRequestedEventArgs *a) -> HRESULT
            { return OnNewWindowRequested(s, a); }).Get(),
        &m_tok_new_window);

    // ------------------------------------------------------------------
    // Step 4 — Apply settings, bounds, visibility, and subclass
    // ------------------------------------------------------------------
    ApplySettings();

    // Install the WM_SIZE/WM_SHOWWINDOW subclass on m_host_hwnd so:
    //   - WM_SIZE  → put_Bounds(GetClientRect(m_host_hwnd))
    //     MCNativeLayerWin32::updateViewGeometry calls MoveWindow(m_host_hwnd)
    //     which sends WM_SIZE, automatically keeping the controller bounds in sync.
    //   - WM_SHOWWINDOW → put_IsVisible(wParam)
    //     MCNativeLayerWin32::doSetVisible calls ShowWindow(m_host_hwnd), which
    //     fires WM_SHOWWINDOW, synchronising WebView2's DComp visibility.
    //     This prevents the ghost render: with the OLD architecture,
    //     m_parent_hwnd was the stack window and WebView2 rendered its
    //     DComp surface across the entire app window.  Now m_host_hwnd is
    //     a small child HWND and WebView2 is properly contained inside it.
    SetWindowSubclass(m_host_hwnd, SizeSubclassProc,
                      k_size_subclass_id, (DWORD_PTR)this);

    // m_host_hwnd is 1×1 at this point; native layer will resize it via
    // MoveWindow shortly after set-native-layer is called.  Set initial
    // bounds to {0,0,1,1} so WebView2 is valid but minimal.
    {
        RECT t_r = { 0, 0, 1, 1 };
        m_controller->put_Bounds(t_r);
    }

    // Start HIDDEN.  MCNativeLayerWin32::doSetVisible(true) will call
    // ShowWindow(m_host_hwnd, SW_SHOWNOACTIVATE), which fires WM_SHOWWINDOW
    // to the subclass above, which calls put_IsVisible(TRUE).
    // This prevents a flash of unsized WebView2 content during startup.
    m_controller->put_IsVisible(FALSE);

    // ------------------------------------------------------------------
    // Step 5 — Load deferred content (URL or HTML set before Initialize)
    // ------------------------------------------------------------------
    if (m_html_text != nullptr)
    {
        wchar_t *t_wide = nullptr;
        if (Utf8ToWide(m_html_text, t_wide))
        {
            m_webview->NavigateToString(t_wide);
            MCBrowserMemoryDeallocate(t_wide);
        }
        MCBrowserMemoryDeallocate(m_html_text);
        m_html_text = nullptr;
        if (m_html_base_url)
        {
            MCBrowserMemoryDeallocate(m_html_base_url);
            m_html_base_url = nullptr;
        }
    }
    else if (m_pending_url != nullptr)
    {
        wchar_t *t_wide = nullptr;
        if (Utf8ToWide(m_pending_url, t_wide))
        {
            m_webview->Navigate(t_wide);
            MCBrowserMemoryDeallocate(t_wide);
        }
        MCBrowserMemoryDeallocate(m_pending_url);
        m_pending_url = nullptr;
    }

    // ------------------------------------------------------------------
    // Step 6 — Inject JS handler shim (if handler list was set before init)
    // ------------------------------------------------------------------
    if (m_handler_list != nullptr && m_handler_list[0] != '\0')
        UpdateHandlerShim();

    return true;
}

// ============================================================
// Settings
// ============================================================

void MCWebView2Browser::ApplySettings()
{
    if (m_webview == nullptr)
        return;

    ComPtr<ICoreWebView2Settings> t_settings;
    if (FAILED(m_webview->get_Settings(&t_settings)) || t_settings == nullptr)
        return;

    t_settings->put_IsScriptEnabled(m_js_enabled ? TRUE : FALSE);
    t_settings->put_AreDefaultContextMenusEnabled(
        m_context_menu_enabled ? TRUE : FALSE);
    t_settings->put_IsStatusBarEnabled(FALSE);
    t_settings->put_AreDefaultScriptDialogsEnabled(TRUE);
    t_settings->put_IsWebMessageEnabled(TRUE);

    // User-agent override requires ICoreWebView2Settings2 (SDK >= 1.0.864.35)
    if (m_user_agent != nullptr)
    {
        ComPtr<ICoreWebView2Settings2> t_s2;
        if (SUCCEEDED(t_settings.As(&t_s2)) && t_s2 != nullptr)
        {
            wchar_t *t_wide = nullptr;
            if (Utf8ToWide(m_user_agent, t_wide))
            {
                t_s2->put_UserAgent(t_wide);
                MCBrowserMemoryDeallocate(t_wide);
            }
        }
    }
}

// ============================================================
// JS handler shim injection
// ============================================================
//
// The shim creates window.livecode.<handler> functions that forward
// calls to native via window.chrome.webview.postMessage().
//
// Message wire format (newline-separated, sent as a plain string):
//   line 0 : "lc_handler"
//   line 1 : handler name
//   line 2+: one argument per line, type-prefixed:
//               b:<true|false>   boolean
//               i:<integer>      integer
//               d:<double>       double (stored as string to preserve precision)
//               s:<text>         string (text is the raw value — no escaping)
//               n:               null / undefined
// ============================================================

void MCWebView2Browser::UpdateHandlerShim()
{
    if (m_webview == nullptr)
        return;

    // Remove the previous shim (if any).
    if (m_shim_script_id != nullptr)
    {
        m_webview->RemoveScriptToExecuteOnDocumentCreated(m_shim_script_id);
        CoTaskMemFree(m_shim_script_id);
        m_shim_script_id = nullptr;
    }

    if (m_handler_list == nullptr || m_handler_list[0] == '\0')
        return;

    // Build a JS array literal of handler names from the '\n'-separated list.
    // e.g. "foo\nbar\nbaz"  →  ["foo","bar","baz"]
    WideBuffer t_names;
    t_names.Append(L"[");

    bool t_first = true;
    const char *p = m_handler_list;
    while (*p != '\0')
    {
        const char *q = p;
        while (*q != '\0' && *q != '\n' && *q != '\r' && *q != ',') q++;

        if (q > p)
        {
            wchar_t *t_name_wide = nullptr;
            int t_wlen = MultiByteToWideChar(CP_UTF8, 0, p, (int)(q - p),
                                             nullptr, 0);
            void *t_mem;
            if (t_wlen > 0 &&
                MCBrowserMemoryAllocate((t_wlen + 1) * sizeof(wchar_t), t_mem))
            {
                t_name_wide = (wchar_t *)t_mem;
                MultiByteToWideChar(CP_UTF8, 0, p, (int)(q - p),
                                    t_name_wide, t_wlen);
                t_name_wide[t_wlen] = L'\0';

                if (!t_first) t_names.Append(L",");
                t_first = false;
                t_names.AppendChar(L'"');
                t_names.Append(t_name_wide);
                t_names.AppendChar(L'"');

                MCBrowserMemoryDeallocate(t_name_wide);
            }
        }

        p = q;
        while (*p == '\n' || *p == '\r' || *p == ',') p++;
    }
    t_names.Append(L"]");

    // Build the full shim script.
    WideBuffer t_script;
    t_script.Append(
        L"(function() {\n"
        L"  if (!window.livecode) window.livecode = {};\n"
        L"  var __lc_names = ");
    t_script.Append(t_names.data);
    t_script.Append(
        L";\n"
        L"  __lc_names.forEach(function(name) {\n"
        L"    window.livecode[name] = function() {\n"
        L"      var parts = ['lc_handler', name];\n"
        L"      for (var i = 0; i < arguments.length; i++) {\n"
        L"        var a = arguments[i];\n"
        L"        var t = typeof a;\n"
        L"        if (a === null || a === undefined) {\n"
        L"          parts.push('n:');\n"
        L"        } else if (t === 'boolean') {\n"
        L"          parts.push('b:' + a);\n"
        L"        } else if (t === 'number') {\n"
        L"          if (Number.isInteger(a)) parts.push('i:' + a);\n"
        L"          else parts.push('d:' + a);\n"
        L"        } else {\n"
        L"          parts.push('s:' + String(a));\n"
        L"        }\n"
        L"      }\n"
        L"      window.chrome.webview.postMessage(parts.join('\\n'));\n"
        L"    };\n"
        L"  });\n"
        L"})();\n");

    // AddScriptToExecuteOnDocumentCreated is async; pump the loop briefly.
    bool t_done = false;
    LPWSTR t_id = nullptr;

    m_webview->AddScriptToExecuteOnDocumentCreated(
        t_script.data,
        Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
            [&](HRESULT p_hr, LPCWSTR p_id) -> HRESULT
            {
                if (SUCCEEDED(p_hr) && p_id != nullptr)
                {
                    size_t t_len = wcslen(p_id) + 1;
                    t_id = (LPWSTR)CoTaskMemAlloc(t_len * sizeof(wchar_t));
                    if (t_id)
                        memcpy(t_id, p_id, t_len * sizeof(wchar_t));
                }
                t_done = true;
                return S_OK;
            }).Get());

    PumpUntilDone(t_done, 2000);
    m_shim_script_id = t_id;

    // Also execute the shim immediately in the current document so handlers
    // are available without requiring a page reload.
    if (t_script.data != nullptr)
    {
        m_webview->ExecuteScript(t_script.data, nullptr);
    }
}

// ============================================================
// Web message dispatch  (JS → native)
// ============================================================

void MCWebView2Browser::DispatchWebMessage(const wchar_t *p_msg)
{
    if (p_msg == nullptr)
        return;

    // Split the message on newlines into parts[].
    // We use a simple fixed-capacity approach: parse in-place.
    // Max 64 parts (handler + name + 62 args) is ample for LiveCode usage.
    const int k_max_parts = 64;
    const wchar_t *parts[k_max_parts];
    int             lens[k_max_parts];
    int             n_parts = 0;

    const wchar_t *p = p_msg;
    while (*p != L'\0' && n_parts < k_max_parts)
    {
        const wchar_t *q = p;
        while (*q != L'\0' && *q != L'\n' && *q != L'\r') q++;
        parts[n_parts] = p;
        lens[n_parts]  = (int)(q - p);
        n_parts++;
        p = q;
        while (*p == L'\n' || *p == L'\r') p++;
    }

    // Validate: at least two parts, first must be "lc_handler".
    if (n_parts < 2)
        return;
    if (wcsncmp(parts[0], L"lc_handler", (size_t)lens[0]) != 0 ||
        lens[0] != (int)wcslen(L"lc_handler"))
        return;

    // Convert handler name to UTF-8.
    // parts[1] may NOT be NUL-terminated (it's a slice of the original buffer),
    // so we copy it to a temp wide string first.
    wchar_t *t_name_wide = nullptr;
    {
        void *t_mem;
        if (!MCBrowserMemoryAllocate((lens[1] + 1) * sizeof(wchar_t), t_mem))
            return;
        t_name_wide = (wchar_t *)t_mem;
        memcpy(t_name_wide, parts[1], lens[1] * sizeof(wchar_t));
        t_name_wide[lens[1]] = L'\0';
    }

    char *t_handler_utf8 = nullptr;
    if (!WideToUtf8(t_name_wide, t_handler_utf8))
    {
        MCBrowserMemoryDeallocate(t_name_wide);
        return;
    }
    MCBrowserMemoryDeallocate(t_name_wide);

    // Build MCBrowserListRef from the typed arg parts (parts[2..n_parts-1]).
    int t_arg_count = n_parts - 2;
    MCBrowserListRef t_params = nullptr;
    if (!MCBrowserListCreate(t_params, (uint32_t)t_arg_count))
    {
        MCBrowserMemoryDeallocate(t_handler_utf8);
        return;
    }

    for (int i = 0; i < t_arg_count; i++)
    {
        const wchar_t *t_part = parts[i + 2];
        int            t_plen = lens[i + 2];

        if (t_plen < 2) // must be at least "X:"
        {
            MCBrowserListAppendUTF8String(t_params, "");
            continue;
        }

        wchar_t t_type = t_part[0]; // 'b', 'i', 'd', 's', 'n'
        // Value starts at t_part[2] (after "X:"), length = t_plen - 2.
        const wchar_t *t_val     = t_part + 2;
        int            t_val_len = t_plen - 2;

        // Copy value to a NUL-terminated wide string for conversion.
        wchar_t *t_val_wide = nullptr;
        {
            void *t_mem;
            if (MCBrowserMemoryAllocate((t_val_len + 1) * sizeof(wchar_t), t_mem))
            {
                t_val_wide = (wchar_t *)t_mem;
                memcpy(t_val_wide, t_val, t_val_len * sizeof(wchar_t));
                t_val_wide[t_val_len] = L'\0';
            }
        }

        switch (t_type)
        {
        case L'n':
            MCBrowserListAppendUTF8String(t_params, "");
            break;

        case L'b':
            MCBrowserListAppendBoolean(t_params,
                t_val_wide != nullptr &&
                wcsncmp(t_val_wide, L"true", 4) == 0);
            break;

        case L'i':
            MCBrowserListAppendInteger(t_params,
                t_val_wide ? (int32_t)_wtoi(t_val_wide) : 0);
            break;

        case L'd':
        case L's':
        default:
        {
            // Store doubles as strings to preserve precision;
            // string args pass through directly.
            char *t_utf8 = nullptr;
            if (t_val_wide != nullptr && WideToUtf8(t_val_wide, t_utf8))
            {
                MCBrowserListAppendUTF8String(t_params, t_utf8);
                MCBrowserMemoryDeallocate(t_utf8);
            }
            else
            {
                MCBrowserListAppendUTF8String(t_params, "");
            }
            break;
        }
        }

        if (t_val_wide) MCBrowserMemoryDeallocate(t_val_wide);
    }

    // Fire the handler.
    OnJavaScriptCall(t_handler_utf8, t_params);

    MCBrowserListRelease(t_params);
    MCBrowserMemoryDeallocate(t_handler_utf8);
}

// ============================================================
// Event callbacks
// ============================================================

HRESULT MCWebView2Browser::OnNavigationStarting(
    ICoreWebView2 *p_sender,
    ICoreWebView2NavigationStartingEventArgs *p_args)
{
    LPWSTR t_uri = nullptr;
    p_args->get_Uri(&t_uri);

    char *t_url = nullptr;
    if (t_uri != nullptr)
        WideToUtf8(t_uri, t_url);

    // Check the navigation request handler (allow all for now;
    // full cancellation support can be added via p_args->put_Cancel(TRUE)).
    OnNavigationBegin(/*in_frame=*/false, t_url ? t_url : "");

    if (t_url) MCBrowserMemoryDeallocate(t_url);
    if (t_uri) CoTaskMemFree(t_uri);

    return S_OK;
}

HRESULT MCWebView2Browser::OnNavigationCompleted(
    ICoreWebView2 *p_sender,
    ICoreWebView2NavigationCompletedEventArgs *p_args)
{
    BOOL t_success = TRUE;
    p_args->get_IsSuccess(&t_success);

    // Refresh the cached URL from the current source.
    LPWSTR t_src = nullptr;
    m_webview->get_Source(&t_src);

    char *t_url = nullptr;
    if (t_src != nullptr)
    {
        WideToUtf8(t_src, t_url);
        CoTaskMemFree(t_src);
    }
    if (t_url != nullptr)
    {
        StringCopy(t_url, m_url);
        // m_url now owns the string; t_url still needs freeing.
        MCBrowserMemoryDeallocate(t_url);
        // Re-read m_url for the callbacks below.
        t_url = nullptr;
        WideToUtf8((wchar_t*)L"", t_url); // placeholder
        StringCopy(m_url, t_url);         // copy for passing down
    }

    // Simpler: just pass m_url directly.
    const char *t_cb_url = m_url ? m_url : "";

    if (t_success)
    {
        OnNavigationComplete(false, t_cb_url);
    }
    else
    {
        COREWEBVIEW2_WEB_ERROR_STATUS t_err = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
        p_args->get_WebErrorStatus(&t_err);
        char t_err_str[32];
        sprintf_s(t_err_str, sizeof(t_err_str), "WebErrorStatus=%d", (int)t_err);
        OnNavigationFailed(false, t_cb_url, t_err_str);
    }

    if (t_url) MCBrowserMemoryDeallocate(t_url);
    return S_OK;
}

HRESULT MCWebView2Browser::OnContentLoading(
    ICoreWebView2 *p_sender,
    ICoreWebView2ContentLoadingEventArgs *p_args)
{
    LPWSTR t_src = nullptr;
    m_webview->get_Source(&t_src);

    char *t_url = nullptr;
    if (t_src != nullptr)
    {
        WideToUtf8(t_src, t_url);
        CoTaskMemFree(t_src);
    }

    OnDocumentLoadBegin(false, t_url ? t_url : "");
    if (t_url) MCBrowserMemoryDeallocate(t_url);
    return S_OK;
}

HRESULT MCWebView2Browser::OnSourceChanged(
    ICoreWebView2 *p_sender,
    ICoreWebView2SourceChangedEventArgs *p_args)
{
    // Keep m_url in sync with the actual current URL.
    LPWSTR t_src = nullptr;
    if (SUCCEEDED(m_webview->get_Source(&t_src)) && t_src != nullptr)
    {
        char *t_utf8 = nullptr;
        if (WideToUtf8(t_src, t_utf8))
        {
            StringCopy(t_utf8, m_url);
            MCBrowserMemoryDeallocate(t_utf8);
        }
        CoTaskMemFree(t_src);
    }

    // Fire OnDocumentLoadComplete when the source settles after a navigation.
    OnDocumentLoadComplete(false, m_url ? m_url : "");
    return S_OK;
}

HRESULT MCWebView2Browser::OnWebMessageReceived(
    ICoreWebView2 *p_sender,
    ICoreWebView2WebMessageReceivedEventArgs *p_args)
{
    LPWSTR t_msg = nullptr;

    // The shim uses postMessage(string), so TryGetWebMessageAsString succeeds.
    HRESULT t_hr = p_args->TryGetWebMessageAsString(&t_msg);
    if (FAILED(t_hr) || t_msg == nullptr)
    {
        // Fallback: accept JSON-encoded messages too.
        p_args->get_WebMessageAsJson(&t_msg);
    }

    if (t_msg != nullptr)
    {
        DispatchWebMessage(t_msg);
        CoTaskMemFree(t_msg);
    }
    return S_OK;
}

HRESULT MCWebView2Browser::OnNewWindowRequested(
    ICoreWebView2 *p_sender,
    ICoreWebView2NewWindowRequestedEventArgs *p_args)
{
    if (m_allow_new_windows)
    {
        // Open in the existing view rather than a real new window.
        LPWSTR t_uri = nullptr;
        p_args->get_Uri(&t_uri);
        if (t_uri != nullptr)
        {
            m_webview->Navigate(t_uri);
            CoTaskMemFree(t_uri);
        }
    }
    // Always mark as handled to prevent Edge from opening a new OS window.
    p_args->put_Handled(TRUE);
    return S_OK;
}

// ============================================================
// MCBrowser interface — GetNativeLayer / Rect
// ============================================================

void *MCWebView2Browser::GetNativeLayer()
{
    // Return the dedicated host HWND created during Initialize().
    // MCNativeLayerWin32 will reparent, resize, show, and hide this HWND.
    // The WebView2 controller is parented to m_host_hwnd, so all those
    // operations correctly affect the browser content.
    //
    // This mirrors MCCefWin32Browser::PlatformGetNativeLayer() which returns
    // the CEF-created child HWND (t_browser->GetHost()->GetWindowHandle()),
    // NOT m_parent_window (the stack window).
    return (void *)(m_host_hwnd != nullptr ? m_host_hwnd : m_parent_hwnd);
}

bool MCWebView2Browser::GetRect(MCBrowserRect &r_rect)
{
    r_rect = m_rect;
    return true;
}

bool MCWebView2Browser::SetRect(const MCBrowserRect &p_rect)
{
    m_rect = p_rect;
    if (m_controller != nullptr)
    {
        // The WebView2 controller is hosted inside m_host_hwnd, which the
        // native layer positions via MoveWindow.  When MoveWindow fires
        // WM_SIZE, SizeSubclassProc calls put_Bounds(GetClientRect(m_host_hwnd))
        // automatically.  An explicit SetRect call must therefore set bounds
        // relative to m_host_hwnd's origin (i.e. always starting at 0,0).
        RECT t_r = { 0, 0,
                     p_rect.right - p_rect.left,
                     p_rect.bottom - p_rect.top };
        return SUCCEEDED(m_controller->put_Bounds(t_r));
    }
    // Cached — applied in Initialize().
    return true;
}

// ============================================================
// Navigation
// ============================================================

bool MCWebView2Browser::GoBack()
{
    if (m_webview == nullptr) return false;
    BOOL t_can = FALSE;
    m_webview->get_CanGoBack(&t_can);
    if (!t_can) return false;
    return SUCCEEDED(m_webview->GoBack());
}

bool MCWebView2Browser::GoForward()
{
    if (m_webview == nullptr) return false;
    BOOL t_can = FALSE;
    m_webview->get_CanGoForward(&t_can);
    if (!t_can) return false;
    return SUCCEEDED(m_webview->GoForward());
}

bool MCWebView2Browser::GoToURL(const char *p_url)
{
    if (p_url == nullptr)
        return false;

    if (m_webview == nullptr)
        return StringCopy(p_url, m_pending_url);

    wchar_t *t_wide = nullptr;
    if (!Utf8ToWide(p_url, t_wide))
        return false;

    HRESULT t_hr = m_webview->Navigate(t_wide);
    MCBrowserMemoryDeallocate(t_wide);
    return SUCCEEDED(t_hr);
}

bool MCWebView2Browser::LoadHTMLText(const char *p_htmltext,
                                      const char *p_base_url)
{
    // Note: WebView2's NavigateToString does not accept an explicit base URL.
    // For base-URL support, use SetVirtualHostNameToFolderMapping or a
    // data: URI scheme.  For now the base URL parameter is stored but unused.
    if (m_webview == nullptr)
    {
        bool t_ok = StringCopy(p_htmltext, m_html_text);
        if (t_ok && p_base_url != nullptr)
            t_ok = StringCopy(p_base_url, m_html_base_url);
        return t_ok;
    }

    wchar_t *t_wide = nullptr;
    if (!Utf8ToWide(p_htmltext, t_wide))
        return false;

    HRESULT t_hr = m_webview->NavigateToString(t_wide);
    MCBrowserMemoryDeallocate(t_wide);
    return SUCCEEDED(t_hr);
}

bool MCWebView2Browser::StopLoading()
{
    if (m_webview == nullptr) return false;
    return SUCCEEDED(m_webview->Stop());
}

bool MCWebView2Browser::Reload()
{
    if (m_webview == nullptr) return false;
    return SUCCEEDED(m_webview->Reload());
}

// ============================================================
// EvaluateJavaScript  (synchronous from caller's perspective)
// ============================================================

bool MCWebView2Browser::EvaluateJavaScript(const char *p_script,
                                             char *&r_result)
{
    if (m_webview == nullptr || p_script == nullptr)
        return false;

    wchar_t *t_wide = nullptr;
    if (!Utf8ToWide(p_script, t_wide))
        return false;

    bool  t_done   = false;
    bool  t_ok     = false;
    char *t_result = nullptr;

    HRESULT t_hr = m_webview->ExecuteScript(
        t_wide,
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [&](HRESULT p_hr, LPCWSTR p_result) -> HRESULT
            {
                if (SUCCEEDED(p_hr) && p_result != nullptr)
                {
                    // WebView2 returns the result JSON-encoded.
                    // Unwrap JSON strings ("hello" → hello), pass numbers as-is,
                    // map null/undefined to empty string.
                    size_t t_len = wcslen(p_result);
                    const wchar_t *t_raw = p_result;

                    if (t_len >= 2 &&
                        t_raw[0] == L'"' && t_raw[t_len - 1] == L'"')
                    {
                        // JSON string — strip outer quotes and unescape.
                        WideBuffer t_buf;
                        for (size_t i = 1; i < t_len - 1; i++)
                        {
                            if (t_raw[i] == L'\\' && i + 1 < t_len - 1)
                            {
                                i++;
                                switch (t_raw[i])
                                {
                                case L'"':  t_buf.AppendChar(L'"');  break;
                                case L'\\': t_buf.AppendChar(L'\\'); break;
                                case L'/':  t_buf.AppendChar(L'/');  break;
                                case L'n':  t_buf.AppendChar(L'\n'); break;
                                case L'r':  t_buf.AppendChar(L'\r'); break;
                                case L't':  t_buf.AppendChar(L'\t'); break;
                                default:    t_buf.AppendChar(t_raw[i]); break;
                                }
                            }
                            else
                            {
                                t_buf.AppendChar(t_raw[i]);
                            }
                        }
                        WideToUtf8(t_buf.data ? t_buf.data : L"", t_result);
                    }
                    else if (wcscmp(t_raw, L"null") == 0 ||
                             wcscmp(t_raw, L"undefined") == 0)
                    {
                        // Allocate an empty string.
                        void *t_mem;
                        if (MCBrowserMemoryAllocate(1, t_mem))
                        {
                            t_result = (char *)t_mem;
                            t_result[0] = '\0';
                        }
                    }
                    else
                    {
                        // Number, boolean, or other literal — return as UTF-8.
                        WideToUtf8(t_raw, t_result);
                    }

                    t_ok = (t_result != nullptr);
                }
                t_done = true;
                return S_OK;
            }).Get());

    MCBrowserMemoryDeallocate(t_wide);

    if (FAILED(t_hr) || !PumpUntilDone(t_done) || !t_ok)
        return false;

    r_result = t_result;
    return true;
}

// ============================================================
// Bool properties
// ============================================================

bool MCWebView2Browser::GetBoolProperty(MCBrowserProperty p_property,
                                         bool &r_value)
{
    switch (p_property)
    {
    case kMCBrowserAllowUserInteraction:
        r_value = m_allow_user_interaction; return true;

    case kMCBrowserAllowNewWindows:
        r_value = m_allow_new_windows; return true;

    case kMCBrowserEnableContextMenu:
        r_value = m_context_menu_enabled; return true;

    case kMCBrowserVerticalScrollbarEnabled:
        r_value = m_vscroll_enabled; return true;

    case kMCBrowserHorizontalScrollbarEnabled:
        r_value = m_hscroll_enabled; return true;

    case kMCBrowserIsSecure:
        r_value = (m_url != nullptr && strncmp(m_url, "https://", 8) == 0);
        return true;

    case kMCBrowserCanGoBack:
        if (m_webview != nullptr)
        {
            BOOL t_v = FALSE;
            m_webview->get_CanGoBack(&t_v);
            r_value = (t_v == TRUE);
        }
        else
            r_value = false;
        return true;

    case kMCBrowserCanGoForward:
        if (m_webview != nullptr)
        {
            BOOL t_v = FALSE;
            m_webview->get_CanGoForward(&t_v);
            r_value = (t_v == TRUE);
        }
        else
            r_value = false;
        return true;

    default:
        return false;
    }
}

bool MCWebView2Browser::SetBoolProperty(MCBrowserProperty p_property,
                                         bool p_value)
{
    switch (p_property)
    {
    case kMCBrowserAllowUserInteraction:
        m_allow_user_interaction = p_value;
        if (m_parent_hwnd)
            EnableWindow(m_parent_hwnd, p_value ? TRUE : FALSE);
        return true;

    case kMCBrowserAllowNewWindows:
        m_allow_new_windows = p_value;
        return true;

    case kMCBrowserEnableContextMenu:
        m_context_menu_enabled = p_value;
        if (m_webview != nullptr) ApplySettings();
        return true;

    case kMCBrowserVerticalScrollbarEnabled:
        m_vscroll_enabled = p_value;
        return true;

    case kMCBrowserHorizontalScrollbarEnabled:
        m_hscroll_enabled = p_value;
        return true;

    default:
        return false;
    }
}

// ============================================================
// String properties
// ============================================================

bool MCWebView2Browser::GetStringProperty(MCBrowserProperty p_property,
                                           char *&r_utf8_string)
{
    char *t_copy = nullptr;

    switch (p_property)
    {
    case kMCBrowserURL:
        if (!StringCopy(m_url ? m_url : "", t_copy))
            return false;
        r_utf8_string = t_copy;
        return true;

    case kMCBrowserHTMLText:
        // Live query via JS.
        return EvaluateJavaScript("document.documentElement.outerHTML",
                                  r_utf8_string);

    case kMCBrowserUserAgent:
        if (m_user_agent != nullptr)
        {
            if (!StringCopy(m_user_agent, t_copy))
                return false;
            r_utf8_string = t_copy;
            return true;
        }
        // Query from WebView2 via JS.
        return EvaluateJavaScript("navigator.userAgent", r_utf8_string);

    case kMCBrowserJavaScriptHandlers:
        if (!StringCopy(m_handler_list ? m_handler_list : "", t_copy))
            return false;
        r_utf8_string = t_copy;
        return true;

    default:
        return false;
    }
}

bool MCWebView2Browser::SetStringProperty(MCBrowserProperty p_property,
                                           const char *p_utf8_string)
{
    switch (p_property)
    {
    case kMCBrowserURL:
        return GoToURL(p_utf8_string);

    case kMCBrowserHTMLText:
        return LoadHTMLText(p_utf8_string, nullptr);

    case kMCBrowserUserAgent:
        if (!StringCopy(p_utf8_string, m_user_agent))
            return false;
        if (m_webview != nullptr) ApplySettings();
        return true;

    case kMCBrowserJavaScriptHandlers:
        if (!StringCopy(p_utf8_string, m_handler_list))
            return false;
        UpdateHandlerShim();
        return true;

    default:
        return false;
    }
}

// ============================================================
// Integer properties  (none currently used on Windows)
// ============================================================

bool MCWebView2Browser::GetIntegerProperty(MCBrowserProperty p_property,
                                            int32_t &r_value)
{
    return false;
}

bool MCWebView2Browser::SetIntegerProperty(MCBrowserProperty p_property,
                                            int32_t p_value)
{
    return false;
}

#endif // _WIN32
