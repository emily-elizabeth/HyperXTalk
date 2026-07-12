# WebView2 Migration Guide

Replaces the CEF 74 browser backend with Microsoft WebView2.  
No CEF DLL bundle, no ghost-render DPI hacks, no separate renderer process.

---

## New files

| File | Purpose |
|------|---------|
| `libbrowser/src/libbrowser_webview2.h` | `MCWebView2Browser` class declaration |
| `libbrowser/src/libbrowser_webview2.cpp` | Full `MCBrowser` interface implementation |
| `libbrowser/src/libbrowser_webview2_win.cpp` | `MCWebView2BrowserFactory` + lifecycle hooks |
| `engine/src/native-layer-win32-wv2.h` | Simplified `MCNativeLayerWin32` header (no CEF members) |
| `engine/src/native-layer-win32-wv2.cpp` | Simplified native layer implementation |

---

## Step 1 — Add the WebView2 NuGet package

In the HyperXTalk solution root, run:

```
nuget install Microsoft.Web.WebView2 -OutputDirectory packages
```

Or install via Visual Studio: **Tools → NuGet Package Manager → Manage NuGet Packages for Solution** → search `Microsoft.Web.WebView2`.

The package provides:
- `build\native\include\WebView2.h` — COM headers
- `build\native\x64\WebView2LoaderStatic.lib` — static loader (no DLL needed at runtime)

---

## Step 2 — Update `libbrowser` vcxproj

File: `build-win-x86_64\hyperxtalk\libbrowser\libbrowser.vcxproj` (or equivalent)

**Add to `AdditionalIncludeDirectories`:**
```
$(SolutionDir)packages\Microsoft.Web.WebView2.1.0.3240.44\build\native\include
```
(adjust version number to match installed package)

**Replace CEF source files with WebView2 source files:**

Remove:
```xml
<ClCompile Include="..\..\..\..\libbrowser\src\libbrowser_cef.cpp" />
<ClCompile Include="..\..\..\..\libbrowser\src\libbrowser_cef_win.cpp" />
<ClCompile Include="..\..\..\..\libbrowser\src\libbrowser_cefprocess.cpp" />
<ClCompile Include="..\..\..\..\libbrowser\src\libbrowser_cefprocess_win.cpp" />
```

Add:
```xml
<ClCompile Include="..\..\..\..\libbrowser\src\libbrowser_webview2.cpp" />
<ClCompile Include="..\..\..\..\libbrowser\src\libbrowser_webview2_win.cpp" />
```

**Add to `AdditionalDependencies`:**
```
WebView2LoaderStatic.lib;shlwapi.lib
```

**Add to `AdditionalLibraryDirectories`:**
```
$(SolutionDir)packages\Microsoft.Web.WebView2.1.0.3240.44\build\native\x64
```

---

## Step 3 — Update `engine` vcxproj

File: `build-win-x86_64\hyperxtalk\engine\development.vcxproj` (or kernel.vcxproj)

**Replace the CEF native layer with the WebView2 one:**

Remove:
```xml
<ClCompile Include="..\..\..\..\engine\src\native-layer-win32.cpp" />
```

Add:
```xml
<ClCompile Include="..\..\..\..\engine\src\native-layer-win32-wv2.cpp" />
```

**Update the include that selects the header** — wherever `native-layer-win32.h` is included indirectly through the build system, the new `.cpp` file already includes `native-layer-win32-wv2.h` which has the same include guard (`__MC_WIDGET_NATIVE__`), so no other header changes are needed.

---

## Step 4 — Wire the factory into `MCBrowserLibraryInitialize`

In `libbrowser/src/libbrowser.cpp`, find `MCBrowserLibraryInitialize()` and replace the CEF factory registration:

```cpp
// BEFORE (CEF):
extern bool MCCefBrowserFactoryCreate(MCBrowserFactoryRef &r_factory);
// ...
MCCefBrowserFactoryCreate(s_factory_list[0].instance);

// AFTER (WebView2):
extern bool MCWebView2BrowserFactoryCreate(MCBrowserFactoryRef &r_factory);
// ...
MCWebView2BrowserFactoryCreate(s_factory_list[0].instance);
```

The factory ID string passed to `MCBrowserFactoryGet()` in the LiveCode engine should remain unchanged (whatever it currently is — "CEF" or similar) or updated to "WebView2".  Check `revbrowser/src/cefbrowser_w32.cpp` for the factory ID used at the call site.

---

## Step 5 — Remove `revbrowser-cefprocess`

The separate renderer process (`revbrowser/src/cefprocess*.cpp`, `revbrowser-cefprocess.exe`) is not needed with WebView2.  Remove the project reference and deployment step.

---

## Step 6 — Remove CEF DLL bundle

The following files in the deployment directory are no longer needed:
- `libcef.dll`
- `chrome_elf.dll`
- `icudtl.dat`
- `v8_context_snapshot.bin`
- `cef.pak` / `cef_100_percent.pak` / `cef_200_percent.pak`
- `locales\` folder
- `revbrowser-cefprocess.exe`

WebView2 uses the system Edge runtime (installed with Windows 10/11).  No browser files need to be bundled with HyperXTalk.

---

## Runtime requirement

WebView2 runtime ships with:
- Windows 11 (all editions)
- Windows 10 via Microsoft Edge (version 86+)

For enterprise deployments without Edge, the **Evergreen Standalone Installer** or **Fixed Version** runtime can be bundled.  See: https://developer.microsoft.com/microsoft-edge/webview2/

---

## What this migration fixes

| Problem | Status |
|---------|--------|
| Ghost render at 150% DPI (v15–v22 saga) | **Gone** — WebView2 is DPI-aware by design |
| `ShowWindow` → `HideImpl` → CEF callback re-entry | **Gone** — no `ShowWindow(cef_widget)` |
| DComp surface independent of `WS_VISIBLE` | **Gone** — WebView2 honours `WS_VISIBLE` |
| `MCCefWidgetWndProc` subclassing | **Gone** |
| Separate renderer process (`revbrowser-cefprocess`) | **Gone** |
| Navigation / progress / JS callback bugs (libffi) | **Gone** — WebView2 delivers callbacks on the UI thread |
| CEF 74 (2019) security/web-compat debt | **Gone** — WebView2 tracks system Edge (Chromium 120+) |
