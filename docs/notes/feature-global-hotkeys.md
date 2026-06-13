# Global hotkeys

New statements `registerHotkey`, `unregisterHotkey`, and `unregisterAllHotkeys`
allow scripts to bind system-wide keyboard shortcuts that fire a handler even
when HyperXTalk is in the background.

## Syntax

```
registerHotkey <key>, <handlerName>
unregisterHotkey <key>
unregisterAllHotkeys
```

`<key>` is a modifier+key string such as `"Ctrl+Shift+P"`.  Modifiers are
case-insensitive and may use common aliases:

| Alias(es)             | Meaning  |
|-----------------------|----------|
| `Ctrl`, `Control`     | Control  |
| `Shift`               | Shift    |
| `Alt`, `Option`       | Alt/Meta |
| `Win`, `Cmd`, `Super` | Super    |

`<handlerName>` is the name of a message handler in the stack script (or
anywhere in the message path) that will be called when the hotkey fires.

`the result` is set to an error string on failure, or empty on success.

## Platform notes

### macOS
Uses `CGEventTap` / `RegisterEventHotKey`.  All modifier combinations are
supported.

### Windows
Uses `RegisterHotKey`.  All modifier combinations are supported.

### Linux — known limitations

Global hotkeys on Linux are inherently less reliable than on macOS or Windows.
Some applications intercept keyboard input before it reaches the global hook,
meaning a registered hotkey may not fire when those applications have focus.
This is most commonly seen with:

- **Electron-based apps** (VS Code, Slack, Discord, etc.) — these create their
  own input grab and can block hotkeys registered via either the portal or
  `XGrabKey`.
- **Games and fullscreen apps** — applications that request an exclusive input
  grab will prevent all global hotkeys from firing while they are in focus.
- **Other hotkey managers** (e.g. KDE's custom shortcuts, `sxhkd`, `xbindkeys`)
  — if another tool has already claimed the same combination, registration will
  succeed but the hotkey will not reliably fire.

If a hotkey is not being recognised in a particular application, try a less
common modifier combination (e.g. add `Super`) or register the hotkey from
within that application instead.

### Linux — Wayland (GNOME 43+, KDE Plasma 5.27+)

Uses the **XDG GlobalShortcuts D-Bus portal**
(`org.freedesktop.portal.GlobalShortcuts`).

**Required launch method.** The process must be identified by a systemd user
scope whose name follows the `app-<AppID>-*.scope` pattern.  The easiest way
to achieve this is to launch HyperXTalk via `systemd-run`:

```sh
systemd-run --user --scope --unit="app-hyperxtalk-$(date +%s).scope" ./HyperXTalk
```

Without this, xdg-desktop-portal cannot identify the calling application and
will reject the shortcut registration.

**First-time assignment dialog.** On the first call to `registerHotkey` for a
given key combination, the desktop environment may show a shortcut-assignment
dialog asking the user to confirm or reassign the key.  Subsequent calls with
the same key reuse the existing assignment without showing the dialog again.

**Alt modifier not supported on GNOME.** GNOME Shell reserves the Alt modifier
for its own use (Alt+Tab, Alt+F4, accessibility bindings, etc.).  The
GlobalShortcuts portal backend silently rejects any combination that includes
Alt — no dialog is shown and the hotkey will not fire.  Use `Ctrl`, `Shift`,
or `Super` instead.  For example, `"Ctrl+Shift+P"` works; `"Alt+Ctrl+Shift+P"`
does not.

### Linux — X11

Uses `XGrabKey` on the root window via a dedicated background thread.
All modifier combinations that are not already grabbed by the window manager
are supported.  If a combination is in use, `the result` is set to
`"hotkey already in use by another application"`.
