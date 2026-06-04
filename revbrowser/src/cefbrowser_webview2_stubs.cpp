// cefbrowser_webview2_stubs.cpp
//
// Stub implementations of the two CEF entry points that revbrowser.cpp
// calls.  On Windows we use WebView2 (via w32browser.cpp/CWebBrowser),
// not CEF, so:
//
//   MCCefBrowserInstantiate  -> delegates to InstantiateBrowser()
//   MCCefFinalise            -> no-op (WebView2 needs no global teardown)
//
// This file is compiled instead of cefbrowser.cpp for the Windows build.

#include "w32browser.h"

// Forward declaration — defined in w32browser.cpp.
CWebBrowserBase *InstantiateBrowser(int p_window_id);

CWebBrowserBase *MCCefBrowserInstantiate(int p_window_id)
{
    return InstantiateBrowser(p_window_id);
}

void MCCefFinalise(void)
{
    // Nothing to do for WebView2.
}
