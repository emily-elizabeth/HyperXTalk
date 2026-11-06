# preferredAppearance test stack

`preferredAppearance.livecode` exercises the `preferredAppearance` global
property (system / light / dark) on any desktop engine that has it.

## Running it

- In the IDE: open the stack.
- With a standalone engine and no IDE (for example the Windows Debug engine
  uploaded by the build workflow): put the stack next to
  `standalone-community.exe` and run

      standalone-community.exe preferredAppearance.livecode

  On Linux: `./standalone-community preferredAppearance.livecode`.

## What to check

1. The `info` field shows the current `preferredAppearance`,
   `systemAppearance`, `systemWindowColor`, `systemTextColor` and
   `systemThemeName`. With the OS in light mode the first two read
   `system` and `light`.
2. Click **dark**. Expected: the window background, the field backgrounds,
   the option menu, combo box, check box and scrollbar redraw dark; on Windows
   the title bar turns dark too. `systemAppearance` reads `dark`, the colours
   change, and one `systemAppearanceChanged: dark, #......, #......` line is
   appended to the `log` field.
3. Click **light**: everything goes back to light, one `light` line is logged.
4. Click **system**: the OS setting applies again, one line is logged. Then
   flip the OS's own dark/light setting: the window follows and a line is
   logged each time. With **light** or **dark** selected, flipping the OS
   setting must not change the window.
5. Open a context menu or a tooltip while **dark** is selected on a light OS
   (Windows): it should be dark as well (uxtheme ForceDark; Windows 10 1903+).
6. Clicking the mode that is already active still logs one
   `systemAppearanceChanged` line (the property always sends it, as on macOS).
