# GTK3 / WebKitGTK Refactor — Testing Status & Methodology

> Branch: `webkitgtk-browser`
> Upstream PR: [#142](https://github.com/emily-elizabeth/HyperXTalk/pull/142)
> Platform: Linux x86_64 (Ubuntu/Debian)
> GTK Version: 3.24.41
> Date: 2026-05-26

---

## 1. How We Test

### Build Configuration
```bash
./config.sh --platform linux-x86_64
make compile-linux-x86_64 -j4
```

### Test Harnesses

| Target | Command | What it tests | GUI Required |
|--------|---------|---------------|--------------|
| C++ unit tests | `make check-linux-x86_64` | libFoundation, libGraphics, etc. | No |
| LCB VM tests | `make -C tests bin_dir=../linux-x86_64-bin lcb-check` | LiveCode Builder VM | No |
| LCS engine tests | `make -C tests bin_dir=../linux-x86_64-bin check` | LiveCode Script engine + IDE | Yes (xvfb-run) |
| LCS parser tests | `make -C tests bin_dir=../linux-x86_64-bin lcs-parser-check` | Script parser | No |
| Compiler tests | `make -C tests bin_dir=../linux-x86_64-bin compiler-check` | LCB compiler | No |
| IDE tests | `make -C ide/tests bin_dir=../../linux-x86_64-bin check` | IDE functionality | Yes |
| Extension tests | `make -C extensions bin_dir=../linux-x86_64-bin check` | LCB widgets/libraries | Yes |

### GUI Testing Environment
All GUI-required tests run under a virtual framebuffer:
```bash
xvfb-run -a <command>
```

### Manual Smoke Tests
Custom `.livecodescript` files under `_test_stacks/` exercise specific APIs:
- Widget creation (button, field, scrollbar, group, graphic, browser)
- Show/hide cycles
- Property mutation
- Invalid value handling
- Browser widget: create, set HTML, set URL, evaluate JS, show/hide

### Server/Headless Smoke Test
```bash
echo 'put the mouse' | xvfb-run -a ./linux-x86_64-bin/server-community
```

---

## 2. What Was Tested & Results

### Build
- [x] `make compile-linux-x86_64 -j4` — **PASS**
- [x] All binaries produced in `linux-x86_64-bin/`

### C++ Unit Tests (libFoundation)
- `test-libFoundation` — **76/77 PASS**
  - Known failure: `name.index_equal_string` (pre-existing, not GTK3-related)

### LCB VM Tests
- `lcb-check` — **836 tests behaved as expected**
  - 9 expected failures
  - 40 skipped on Linux:
    - 25 ObjC interop (`interop-objc.lcb` + `objc.lcb`)
    - 14 Java FFI (`java.lcb` + `foreign-binding.lcb`, bug 19934)
    - 1 bitwise bug 14939

### Manual Smoke Tests
- [x] Server headless start — **PASS**
- [x] IDE window open under xvfb — **PASS**
- [x] Basic widget creation cycles — **PASS**
- [x] Invalid button style (`"tab"`) — clean exit (was SIGSEGV before fix) — **PASS**
- [x] Valid scrollbar orientations (`"horizontal"`/`"vertical"`) — **PASS**
- [x] Browser widget creation, HTML injection, property setting — **PASS**
- [x] Full `_test_stacks/*.livecodescript` crash sweep under `xvfb-run` — **25/25 PASS, exit 0, no crashes**
  - Command: `for f in _test_stacks/*.livecodescript; do timeout 30s-60s xvfb-run -a ./linux-x86_64-bin/server-community "$f"; done`
  - Covered: stacks/cards, buttons, tab style, fields, scrollbars/sliders/progress, groups, graphics, show/hide, property changes while hidden, browser create/HTML/URL/properties/JS/nav, dialogs (answer/ask/file/folder/color), drag & drop, IME/unicode input

### Full LCS Engine/IDE Tests
- Status: **NOT YET COMPLETED in this pass** (very slow due to extension loading)
- No critical assertion failures observed after GTK3 fixes in prior partial run
- Previously flooded with `Gdk-CRITICAL: gdk_device_get_state: assertion 'GDK_IS_WINDOW (window)' failed` — **FIXED**

---

## 3. Fixes Applied (Chronological)

| Commit | Issue | Files |
|--------|-------|-------|
| `04dcf8a44` | Deprecated `gdk_display_get_pointer` / `gdk_display_warp_pointer` | `engine/src/lnxdce.cpp`, `engine/src/linux.stubs` |
| `9ab07dd57` | SIGSEGV on startup script error in `-ui` mode (heap corruption after `longjmp`) | `engine/src/dispatch.cpp` |
| `eb1a2675f` | `gdk_device_get_state` assertion failure; deprecated `gdk_pointer_grab` / `gdk_display_pointer_ungrab` | `engine/src/lnxdce.cpp`, `engine/src/lnxdcs.cpp`, `engine/src/lnxdnd.cpp`, `engine/src/lnxdc.h`, `engine/src/linux.stubs` |

---

## 4. Known Issues & Blockers

### Pre-existing (not GTK3-related)
- `name.index_equal_string` C++ unit test fails in `libfoundation/test/test_name.cpp`
- Full `make check-linux-x86_64` stops at this failure before running engine/IDE tests
- **Workaround:** Run engine tests directly with `make -C tests bin_dir=../linux-x86_64-bin check`

### GTK3 Deprecation Warnings (non-critical, build noise)
These compile but emit deprecation warnings. They should be cleaned up before merge:

| Warning | Location | Status | GTK3 Replacement |
|---------|----------|--------|------------------|
| `gdk_beep` | `engine/src/lnxdcs.cpp:752` | ⏳ | `gdk_display_beep` |
| `gdk_cairo_create` | `engine/src/lnxdcs.cpp:1074` | ✅ FIXED | `gdk_window_begin_draw_frame` |
| `gdk_screen_get_width/height` | `engine/src/lnxdcs.cpp:620,644,1277` | ✅ FIXED | `gdk_monitor_get_geometry` |
| `gdk_screen_get_width_mm/height_mm` | `engine/src/lnxdcs.cpp:658,672` | ✅ FIXED | `gdk_monitor_get_width_mm` |
| `gdk_visual_get_colormap_size` | `engine/src/lnxdcs.cpp:341` | ⏳ | N/A (colormaps removed) |
| `gdk_cursor_unref` | `engine/src/lnxdcs.cpp`, `engine/src/lnxdnd.cpp` | ⏳ | `g_object_unref` |
| `gtk_widget_get_style` | `engine/src/linux-theme.cpp:199` | ✅ FIXED | `gtk_widget_get_style_context` |
| `gdk_device_grab/ungrab` | `engine/src/lnxdcs.cpp`, `engine/src/lnxdnd.cpp` | ⏳ | `gdk_seat_grab/ungrab` |
| `gdk_window_set_background_rgba` | `engine/src/lnxdcs.cpp:1465` | ✅ FIXED | CSS styling / queue draw |
| `gdk_window_process_updates` | `engine/src/lnxdcs.cpp:1469` | ✅ FIXED | `gdk_window_invalidate_rect` |
| `gdk_get_display` | `engine/src/lnxdcs.cpp:204` | ⏳ | `gdk_display_get_name(gdk_display_get_default())` |

### iODBC Build Warning
- `libiodbc` configure fails GTK+ test (`gtk-config` missing) but builds without GUI extensions
- **Impact:** None — ODBC works fine without GTK GUI

---

## 5. What Remains

### Must Fix (before PR merge)
1. **Replace remaining `gdk_device_grab/ungrab` with `gdk_seat_grab/ungrab`** — `gdk_device_grab` is deprecated in GTK3.24+
2. **Clean up remaining deprecation warnings** — `gdk_beep`, `gdk_cursor_unref`, `gdk_get_display`, `gdk_visual_get_colormap_size`
3. **Investigate `name.index_equal_string` test failure** — may be a real bug or test issue

### Should Fix (nice-to-have)
4. Browser widget stress test — multi-tab, heavy JS execution
5. Run full IDE tests and document any widget-specific failures

### Already Fixed (2026-05-26)
- ✅ `gdk_screen_get_width/height` fallbacks removed — `lnxdcs.cpp`
- ✅ `gdk_screen_get_width_mm/height_mm` fallbacks removed — `lnxdcs.cpp`
- ✅ `gdk_window_set_background_rgba` + `gdk_window_process_updates` removed — `lnxdcs.cpp`
- ✅ `gtk_widget_get_style` → `GtkStyleContext` — `linux-theme.cpp`
- ✅ `gdk_cairo_create` → `gdk_window_begin_draw_frame` in XOR selection rect — `lnxdcs.cpp`
- ✅ `linux.stubs` weak stubs cleaned up for eliminated functions

### QA Matrix (to be completed)

| Test | Server | IDE | Notes |
|------|--------|-----|-------|
| Button widget | ✅ | ✅ | |
| Field widget | ✅ | ✅ | |
| Scrollbar/slider | ✅ | ✅ | |
| Group | ✅ | ✅ | |
| Graphic | ✅ | ✅ | |
| Browser widget | ✅ | ✅ | Server smoke covers create/HTML/URL/properties/show-hide |
| Browser JS eval | ✅ | ⏳ | Server script covers JS eval, needs IDE validation |
| Browser navigation | ✅ | ⏳ | Server script covers navigation, needs IDE validation |
| Drag & drop | ✅ | ⏳ | Server script covers dragData/dragImage/dragAction, needs IDE validation |
| Native theme rendering | ✅ | ✅ | Server smoke + prior IDE visual check |
| File dialogs | ✅ | ⏳ | Server script covers answer file/folder, needs IDE validation |
| Color dialogs | ✅ | ⏳ | Server script covers answer color, needs IDE validation |
| Font dialogs | ✅ | ⏳ | Server script covers answer font (partial — no native font dialog on GTK3 headless) |
| IME input | ✅ | ⏳ | Server script covers unicodeText/focus/select, needs IDE validation |

---

## 6. Branch Status

```
upstream/webkitgtk-browser: eb1a2675f (HEAD)
local/webkitgtk-browser:    eb1a2675f (even with upstream)
```

All fixes are pushed to `emily-elizabeth/HyperXTalk:webkitgtk-browser` and reflected in PR #142.

---

## 7. Quick Commands for Continued Work

```bash
# Rebuild after code changes
make compile-linux-x86_64 -j4

# Fast non-GUI test
make -C tests bin_dir=../linux-x86_64-bin lcb-check

# Full GUI engine tests (slow)
make -C tests bin_dir=../linux-x86_64-bin check

# Headless smoke test
echo 'put the mouse' | xvfb-run -a ./linux-x86_64-bin/server-community

# Run specific test file
xvfb-run -a ./linux-x86_64-bin/HyperXTalk ../tests/_testrunner.livecodescript run ../tests/lcs/core/engine/button.livecodescript
```

---

*Document maintained as part of PR #142 — update when test results or fixes change.*
