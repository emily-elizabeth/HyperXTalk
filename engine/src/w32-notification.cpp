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
// Windows notification backend.
//
// Uses WinRT Toast Notifications (Windows 8+) via WRL/COM.
// Windows does not require a permission prompt; permission is considered
// implicitly granted on all supported versions.
//
// Notes:
//   • The calling application must have an AppUserModelID (AUMID) set in
//     order for Toast notifications to be delivered.  This is automatically
//     satisfied when the engine is launched from a packaged MSIX app or a
//     Start-Menu shortcut that carries the AUMID property.
//   • Each notification is tagged with the caller-supplied tag (or a GUID),
//     so that MCPlatformCancelNotification can remove it by ID.
//

// ── WRL / WinRT and C++ standard headers MUST come first ─────────────────────
// Windows SDK 10.0.26100.0 wrl/implements.h uses 'string' as a template
// parameter identifier.  Engine headers pulled in by prefix.h (via w32prefix.h
// → windows.h and sysdefs.h macros) can corrupt the preprocessor environment
// in a way that makes MSVC emit C2059 "syntax error: 'string'" inside WRL.
// Including WRL before any engine header keeps WRL in a clean macro environment.
#include <string>
#include <mutex>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wrl.h>
#include <wrl/event.h>
#include <roapi.h>
#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>
#include <shellapi.h>
#include <shlobj.h>
// Link WinRT bootstrap library (RoInitialize, RoGetActivationFactory, etc.)
#pragma comment(lib, "runtimeobject.lib")

// ── Engine headers (after WRL to avoid macro pollution) ───────────────────────
#include "prefix.h"
#include "mcstring.h"
#include "notification.h"

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::UI::Notifications;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;

////////////////////////////////////////////////////////////////////////////////
// Helpers: MCStringRef ↔ HSTRING / std::wstring

static std::wstring _mcstr_to_wstr(MCStringRef p_str)
{
    if (p_str == nil || MCStringIsEmpty(p_str))
        return L"";
    // MCStringConvertToWString allocates a NUL-terminated unichar_t buffer;
    // on Windows unichar_t is uint16_t which is the same width as wchar_t.
    unichar_t *t_buf = nil;
    if (!MCStringConvertToWString(p_str, t_buf))
        return L"";
    std::wstring t_result(reinterpret_cast<wchar_t*>(t_buf));
    MCMemoryDeallocate(t_buf);
    return t_result;
}

static HString _make_hstring(const wchar_t *p_str)
{
    HString h;
    h.Set(p_str);
    return h;
}

static HString _make_hstring(const std::wstring& p_str)
{
    return _make_hstring(p_str.c_str());
}

////////////////////////////////////////////////////////////////////////////////
// Toast group / app ID constants

static const wchar_t * const kToastGroup = L"HyperXTalk";

////////////////////////////////////////////////////////////////////////////////
// AUMID registration
//
// Windows requires the AUMID to be registered under
//   HKCU\SOFTWARE\Classes\AppUserModelId\<AUMID>
// before it will deliver toast notifications to an unpackaged desktop app.
// We create this key on first use; it persists across sessions.

static void _ensure_aumid_registered(const std::wstring& p_aumid)
{
    // Build the registry key path.
    std::wstring t_key_path = L"SOFTWARE\\Classes\\AppUserModelId\\" + p_aumid;

    HKEY t_key;
    DWORD t_disposition;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, t_key_path.c_str(),
                        0, nullptr, 0, KEY_WRITE, nullptr,
                        &t_key, &t_disposition) != ERROR_SUCCESS)
        return;

    // Only write values on first creation to avoid overwriting user settings.
    if (t_disposition == REG_CREATED_NEW_KEY)
    {
        const wchar_t *t_name = L"HyperXTalk";
        RegSetValueExW(t_key, L"DisplayName", 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(t_name),
                       static_cast<DWORD>((wcslen(t_name) + 1) * sizeof(wchar_t)));
    }

    RegCloseKey(t_key);
}

////////////////////////////////////////////////////////////////////////////////
// Token-to-notification map
// We store the IToastNotification ComPtr keyed by tag so we can dismiss it.

static std::mutex                                               s_map_mutex;
static std::unordered_map<std::wstring, ComPtr<IToastNotification>> s_notifications;

////////////////////////////////////////////////////////////////////////////////
// Get the AUMID currently set for this process (or an empty string).

static std::wstring _get_aumid()
{
    PWSTR t_id = nullptr;
    if (SUCCEEDED(GetCurrentProcessExplicitAppUserModelID(&t_id)) && t_id)
    {
        std::wstring t_result(t_id);
        CoTaskMemFree(t_id);
        return t_result;
    }
    // Fall back to the executable name without path.
    wchar_t t_path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, t_path, MAX_PATH);
    PWSTR t_name = wcsrchr(t_path, L'\\');
    return t_name ? std::wstring(t_name + 1) : std::wstring(t_path);
}

////////////////////////////////////////////////////////////////////////////////
// Generate a random GUID string usable as a notification tag.

static std::wstring _new_guid_str()
{
    GUID g;
    CoCreateGuid(&g);
    wchar_t buf[40];
    StringFromGUID2(g, buf, 40);
    return std::wstring(buf);
}

////////////////////////////////////////////////////////////////////////////////
// Build the toast XML template string.
//
//   <toast>
//     <visual>
//       <binding template="ToastGeneric">
//         <text id="1">TITLE</text>
//         <text id="2">BODY</text>
//       </binding>
//     </visual>
//   </toast>

static std::wstring _build_toast_xml(const std::wstring& p_title,
                                     const std::wstring& p_body,
                                     const std::wstring& p_tag)
{
    // Minimal XML escaping for the text content.
    auto escape = [](const std::wstring& s) -> std::wstring {
        std::wstring r;
        r.reserve(s.size());
        for (wchar_t c : s)
        {
            switch (c)
            {
                case L'&':  r += L"&amp;";  break;
                case L'<':  r += L"&lt;";   break;
                case L'>':  r += L"&gt;";   break;
                case L'"':  r += L"&quot;"; break;
                default:    r += c;          break;
            }
        }
        return r;
    };

    std::wstring xml = L"<toast launch=\"";
    xml += escape(p_tag);
    xml += L"\"><visual><binding template=\"ToastGeneric\">";
    xml += L"<text id=\"1\">" + escape(p_title) + L"</text>";
    if (!p_body.empty())
        xml += L"<text id=\"2\">" + escape(p_body) + L"</text>";
    xml += L"</binding></visual></toast>";
    return xml;
}

////////////////////////////////////////////////////////////////////////////////
// Attempt to show a WinRT toast.  Returns true on success.

// Emit a one-line diagnostic via OutputDebugStringW (capture with DebugView).
static void _toast_dbg(const wchar_t *p_step, HRESULT hr)
{
    wchar_t buf[256];
    swprintf_s(buf, L"[HyperXTalk toast] %s hr=0x%08X\n", p_step, (unsigned)hr);
    OutputDebugStringW(buf);
}

static bool _try_show_toast(const std::wstring& p_title,
                             const std::wstring& p_body,
                             const std::wstring& p_tag)
{
    // Initialise Windows Runtime for this thread if needed.
    HRESULT hr_init = RoInitialize(RO_INIT_SINGLETHREADED);
    _toast_dbg(L"RoInitialize", hr_init);
    // RPC_E_CHANGED_MODE is fine — COM was already initialised on this thread.

    // Obtain IToastNotificationManagerStatics.
    ComPtr<IToastNotificationManagerStatics> t_mgr;
    HRESULT hr = GetActivationFactory(
        HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),
        &t_mgr);
    _toast_dbg(L"GetActivationFactory(Manager)", hr);
    if (FAILED(hr))
        return false;

    // Obtain the notifier for our AUMID, ensuring it is registered first.
    std::wstring t_aumid = _get_aumid();
    _ensure_aumid_registered(t_aumid);
    {
        std::wstring t_msg = L"AUMID=" + t_aumid;
        OutputDebugStringW((L"[HyperXTalk toast] " + t_msg + L"\n").c_str());
    }
    ComPtr<IToastNotifier> t_notifier;
    hr = t_mgr->CreateToastNotifierWithId(
        HStringReference(t_aumid.c_str()).Get(), &t_notifier);
    _toast_dbg(L"CreateToastNotifierWithId", hr);
    if (FAILED(hr))
        return false;

    // Build the XML document.
    std::wstring t_xml = _build_toast_xml(p_title, p_body, p_tag);
    OutputDebugStringW((L"[HyperXTalk toast] XML=" + t_xml + L"\n").c_str());

    // Activate a XmlDocument instance (IXmlDocumentIO is an instance interface,
    // not a factory/statics interface, so GetActivationFactory cannot supply it).
    ComPtr<IInspectable> t_xml_inspectable;
    hr = RoActivateInstance(
        HStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(),
        &t_xml_inspectable);
    _toast_dbg(L"RoActivateInstance(XmlDocument)", hr);
    if (FAILED(hr))
        return false;

    ComPtr<IXmlDocumentIO> t_xml_io;
    hr = t_xml_inspectable.As(&t_xml_io);
    _toast_dbg(L"As(IXmlDocumentIO)", hr);
    if (FAILED(hr))
        return false;

    hr = t_xml_io->LoadXml(HStringReference(t_xml.c_str()).Get());
    _toast_dbg(L"LoadXml", hr);
    if (FAILED(hr))
        return false;

    ComPtr<IXmlDocument> t_xml_doc;
    hr = t_xml_inspectable.As(&t_xml_doc);
    _toast_dbg(L"As(IXmlDocument)", hr);
    if (FAILED(hr))
        return false;

    // Create the notification.
    ComPtr<IToastNotificationFactory> t_factory;
    hr = GetActivationFactory(
        HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(),
        &t_factory);
    _toast_dbg(L"GetActivationFactory(ToastNotification)", hr);
    if (FAILED(hr))
        return false;

    ComPtr<IToastNotification> t_toast;
    hr = t_factory->CreateToastNotification(t_xml_doc.Get(), &t_toast);
    _toast_dbg(L"CreateToastNotification", hr);
    if (FAILED(hr))
        return false;

    // Set tag + group so we can remove it later via IToastNotificationHistory.
    ComPtr<IToastNotification2> t_toast2;
    if (SUCCEEDED(t_toast.As(&t_toast2)))
    {
        t_toast2->put_Tag(HStringReference(p_tag.c_str()).Get());
        t_toast2->put_Group(HStringReference(kToastGroup).Get());
    }

    // Wire up Activated callback → MCNotificationDispatchClicked.
    auto activated_cb = Callback<ITypedEventHandler<ToastNotification*, IInspectable*>>(
        [p_tag](IToastNotification*, IInspectable*) -> HRESULT
        {
            MCStringRef t_mcstr;
            /* UNCHECKED */ MCStringCreateWithWString(reinterpret_cast<const unichar_t*>(p_tag.c_str()), t_mcstr);
            MCNotificationDispatchClicked(t_mcstr);
            MCValueRelease(t_mcstr);
            return S_OK;
        });

    EventRegistrationToken t_token;
    t_toast->add_Activated(activated_cb.Get(), &t_token);

    // Show the toast.
    hr = t_notifier->Show(t_toast.Get());
    _toast_dbg(L"IToastNotifier::Show", hr);
    if (FAILED(hr))
        return false;

    // Remember the notification so we can dismiss it later.
    {
        std::lock_guard<std::mutex> guard(s_map_mutex);
        s_notifications[p_tag] = t_toast;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Fallback: Shell_NotifyIcon balloon tip.
// Used when WinRT is unavailable (Windows 7 or WinRT init failure).
//
// We keep a hidden window + NOTIFYICONDATA alive for the lifetime of the
// process.  The balloon tip fires NIN_BALLOONUSERCLICK when the user clicks,
// which we route back to HyperXTalk.

static HWND       s_balloon_hwnd    = nullptr;
static HINSTANCE  s_hinstance       = nullptr;
static bool       s_nid_added       = false;
static std::wstring s_balloon_tag;   // tag of the most recently shown balloon

#define WM_BALLOONCLICK  (WM_USER + 1)

static LRESULT CALLBACK _BalloonWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_USER && lp == NIN_BALLOONUSERCLICK)
    {
        // User clicked the balloon.
        MCStringRef t_mcstr;
        /* UNCHECKED */ MCStringCreateWithWString(reinterpret_cast<const unichar_t*>(s_balloon_tag.c_str()), t_mcstr);
        MCNotificationDispatchClicked(t_mcstr);
        MCValueRelease(t_mcstr);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND _ensure_balloon_window()
{
    if (s_balloon_hwnd)
        return s_balloon_hwnd;

    s_hinstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = _BalloonWndProc;
    wc.hInstance     = s_hinstance;
    wc.lpszClassName = L"MCNotificationBalloon";
    RegisterClassExW(&wc);  // may fail if already registered — that's fine

    s_balloon_hwnd = CreateWindowExW(0, L"MCNotificationBalloon", L"",
                                     0, 0, 0, 0, 0,
                                     HWND_MESSAGE, nullptr, s_hinstance, nullptr);
    return s_balloon_hwnd;
}

static void _show_balloon(const std::wstring& p_title,
                          const std::wstring& p_body,
                          const std::wstring& p_tag)
{
    HWND hwnd = _ensure_balloon_window();
    if (!hwnd)
        return;

    s_balloon_tag = p_tag;

    NOTIFYICONDATAW nid = {};
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = hwnd;
    nid.uID              = 1;
    nid.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_INFO;
    nid.uCallbackMessage = WM_USER;
    nid.hIcon            = LoadIconW(nullptr, MAKEINTRESOURCEW(32512)); // IDI_APPLICATION
    nid.dwInfoFlags      = NIIF_INFO | NIIF_NOSOUND;

    wcsncpy_s(nid.szTip,  L"HyperXTalk", _TRUNCATE);
    wcsncpy_s(nid.szInfoTitle, p_title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo,      p_body.c_str(),  _TRUNCATE);

    if (!s_nid_added)
    {
        Shell_NotifyIconW(NIM_ADD, &nid);
        s_nid_added = true;
    }
    else
    {
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Platform entry points

void MCPlatformRequestNotificationPermission()
{
    // Windows does not require an explicit permission dialog.
    MCNotificationDispatchPermissionGranted();
}

void MCPlatformShowNotification(MCStringRef p_title, MCStringRef p_body, MCStringRef p_tag)
{
    OutputDebugStringW(L"[HyperXTalk toast] MCPlatformShowNotification called\n");

    std::wstring t_title = _mcstr_to_wstr(p_title);
    std::wstring t_body  = _mcstr_to_wstr(p_body);

    std::wstring t_tag;
    if (p_tag != nil && !MCStringIsEmpty(p_tag))
        t_tag = _mcstr_to_wstr(p_tag);
    else
        t_tag = _new_guid_str();

    if (!_try_show_toast(t_title, t_body, t_tag))
        _show_balloon(t_title, t_body, t_tag);
}

void MCPlatformCancelNotification(MCStringRef p_tag)
{
    std::wstring t_tag = _mcstr_to_wstr(p_tag);

    // Remove from our local map.
    ComPtr<IToastNotification> t_toast;
    {
        std::lock_guard<std::mutex> guard(s_map_mutex);
        auto it = s_notifications.find(t_tag);
        if (it != s_notifications.end())
        {
            t_toast = it->second;
            s_notifications.erase(it);
        }
    }

    // Ask the history manager to remove it by tag.
    ComPtr<IToastNotificationManagerStatics2> t_mgr2;
    if (SUCCEEDED(GetActivationFactory(
            HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),
            &t_mgr2)))
    {
        ComPtr<IToastNotificationHistory> t_history;
        if (SUCCEEDED(t_mgr2->get_History(&t_history)))
            t_history->Remove(HStringReference(t_tag.c_str()).Get());
    }
}

void MCPlatformCancelAllNotifications()
{
    {
        std::lock_guard<std::mutex> guard(s_map_mutex);
        s_notifications.clear();
    }

    ComPtr<IToastNotificationManagerStatics2> t_mgr2;
    if (SUCCEEDED(GetActivationFactory(
            HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(),
            &t_mgr2)))
    {
        ComPtr<IToastNotificationHistory> t_history;
        if (SUCCEEDED(t_mgr2->get_History(&t_history)))
        {
            std::wstring t_aumid = _get_aumid();
            t_history->ClearWithId(HStringReference(t_aumid.c_str()).Get());
        }
    }

    // Dismiss the balloon icon too.
    if (s_nid_added && s_balloon_hwnd)
    {
        NOTIFYICONDATAW nid = {};
        nid.cbSize = sizeof(nid);
        nid.hWnd   = s_balloon_hwnd;
        nid.uID    = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        s_nid_added = false;
    }
}

void MCPlatformGetNotificationPermission(MCStringRef& r_permission)
{
    // Windows has no user-facing permission concept for desktop apps.
    /* UNCHECKED */ MCStringCreateWithCString("granted", r_permission);
}
