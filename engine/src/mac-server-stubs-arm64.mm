// ARM64 server stubs - placeholder
#if defined(__arm64__) || defined(__aarch64__)
// No stubs needed - osxflst.cpp provides MCFontlist
#endif

#include "prefix.h"
#include "platform.h"

// Spell checking is not available in the server engine.
void MCPlatformSpellCheckText(MCStringRef p_text, MCRange*& r_errors, uindex_t& r_count)
{
    r_errors = nil;
    r_count  = 0;
}

// Sharing is not available in the server engine.
void MCPlatformShareContent(MCPlatformWindowRef, MCPlatformShareType, MCValueRef, bool, MCRectangle, MCStringRef)
{
}

// Taskbar / badge / jump-list features are desktop-only; no-ops in the server engine.
void MCPlatformSetBadge(void * /*p_hwnd*/, uinteger_t /*p_count*/)
{
}

void MCPlatformSetTaskbarProgress(void * /*p_hwnd*/, double /*p_value*/)
{
}

void MCPlatformSetJumpList(MCStringRef /*p_tasks*/, MCStringRef /*p_category*/)
{
}

// bringApplicationToFront is a no-op in the server engine (no GUI).
void MCMacActivateApplication(void)
{
}

// Global hotkeys are desktop-only; no-ops in the server engine.
bool MCPlatformRegisterHotkey(MCStringRef /*p_key*/, int32_t /*p_id*/)
{
    return false;
}

void MCPlatformUnregisterHotkey(int32_t /*p_id*/)
{
}

void MCPlatformUnregisterAllHotkeys()
{
}
