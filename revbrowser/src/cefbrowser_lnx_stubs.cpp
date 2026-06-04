// cefbrowser_lnx_stubs.cpp
//
// Stub implementations of the two CEF entry points that revbrowser.cpp
// calls.  On Linux we will use WebViewGTK (in progress), not CEF, so:
//
//   MCCefBrowserInstantiate  -> delegates to InstantiateBrowser()
//   MCCefFinalise            -> no-op (no global CEF teardown needed)
//
// This file is compiled instead of cefbrowser.cpp for the Linux build.

#include "revbrowser.h"

// Forward declaration — defined in lnxbrowser.cpp.
CWebBrowserBase *InstantiateBrowser(int p_window_id);

CWebBrowserBase *MCCefBrowserInstantiate(int p_window_id)
{
    return InstantiateBrowser(p_window_id);
}

void MCCefFinalise(void)
{
    // Nothing to do — no CEF to tear down.
}
