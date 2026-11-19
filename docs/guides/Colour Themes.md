---
group: intermediate
---
# Colour Themes

## Introduction

A colour theme is a set of colours that a stack is drawn in: its window,
its fields, its text, its highlights and its borders. A stack can have two,
one for light mode and one for dark mode, and the engine changes from one to
the other when the system does. Nothing has to be scripted for that to
happen, in the IDE or in a standalone.

Themes take the place of the system's default colours. They do not override
colours you have set: an object with its own `backgroundColor` keeps it.
So a stack built without any colour properties can be given a completely
different look, and changed back, by setting one property.

## Giving a Stack a Theme

### In the Property Inspector

Open the stack's Property Inspector and go to the Colors section. There are
two menus:

* **Color theme (light mode)**
* **Color theme (dark mode)**

Each lists the colour schemes that suit that appearance, the same schemes
the Script Editor offers. Choose one for each mode, or leave either on
**Default** to keep the system's colours in that mode. **Edit themes…**
opens the theme editor, described below.

The stack stores the colours, not the name of the scheme. It needs no scheme
file when it runs, and a later change to the scheme does not change the
stack until you choose the scheme again.

### By script

A theme is an array of colours. Set it with the `lightTheme` and `darkTheme`
properties of the stack:

    local tTheme
    put "40,42,54" into tTheme["backgroundColor"]
    put "33,34,44" into tTheme["fieldColor"]
    put "248,248,242" into tTheme["foregroundColor"]
    put "255,121,198" into tTheme["hiliteColor"]
    set the darkTheme of this stack to tTheme

Set either property to `empty` to remove that theme. A theme does not have
to be complete: a colour it leaves out stays the system's.

The keys are:

| Key | What it colours |
| --- | --- |
| `backgroundColor` | The window and the background of controls |
| `foregroundColor` | Text and lines |
| `fieldColor` | The background of bordered fields |
| `hiliteColor` | Selections, and the accent of buttons, progress bars, checkboxes and tabs |
| `hiliteTextColor` | Text on the highlight (black or white is chosen if not set) |
| `borderColor` | Borders |
| `linkColor` | Link text, where the stack has not set its own link colour |
| `topColor`, `bottomColor`, `shadowColor` | 3D edges and drop shadows (worked out from the background if not set) |
| `focusColor` | The outline of the focused control |

## What Follows a Theme

* **Classic controls.** Buttons, fields, scrollbars, groups and graphics
  that have no colour of their own take the theme's. The `effective` colour
  properties report the theme's colours.
* **Controls the system draws.** On macOS, push buttons, option menus,
  checkboxes, radio buttons, tabs, sliders and progress bars are drawn in
  the light or dark style that suits the theme's background, with the
  theme's `hiliteColor` as their accent.
* **Widgets.** The widgets that come with the IDE read their colours when
  they paint, so they follow the theme. A colour set on a widget still wins.
* **The Data Grid.** Any colour not set on the grid comes from the theme.
* **Substacks.** A substack with no theme of its own uses its mainstack's.

An open menu's list keeps the system's highlight colour, because the system
draws it.

## Standalones

A theme is saved in the stack file, so a standalone uses it with no extra
work. Build the standalone as usual. When the user changes between light and
dark mode, the standalone changes theme with it.

To keep a standalone in one appearance whatever the system is set to, set
the `preferredAppearance` in the mainstack's `preOpenStack` handler:

    on preOpenStack
       set the preferredAppearance to "dark"
    end preOpenStack

## Reacting to a Change of Theme

Most stacks need no script. If a script works out colours of its own from
the theme, two messages say when to do that again:

* `themeChanged` is sent to the current card when the stack's `lightTheme`
  or `darkTheme` is set.
* `systemAppearanceChanged` is sent when the system changes between light
  and dark mode.

For example:

    on themeChanged
       colourBanner
    end themeChanged

    on systemAppearanceChanged
       colourBanner
    end systemAppearanceChanged

    command colourBanner
       set the backgroundColor of graphic "Banner" to \
             the effective hiliteColor of this stack
    end colourBanner

## Theming the IDE

The IDE's own windows have a theme of their own, separate from the stacks
you work on. Choose it in Preferences > Appearance, where there is an IDE
theme menu for light mode and one for dark mode. The Script Editor keeps
its own Colorization Scheme setting, so the editor and the rest of the IDE
can use different schemes.

The IDE's theme never applies to your stacks, and a stack's theme never
applies to the IDE.

## The Theme Editor

Choose **Edit themes…** from any of the theme menus to open the theme
editor. It shows the interface colours of a scheme, with a preview of
controls drawn in them.

* **Choose…** sets a colour. **×** removes it, so that it is worked out from
  the scheme's Script Editor colours, or left to the system.
* **Save** writes the scheme to your Schemes folder. Saving a scheme that
  comes with the IDE makes your own copy of it, which is used in place of
  the original.
* **Save As…** makes a new scheme under another name.
* **Revert** discards the changes made since the scheme was last saved.
* **Restore Bundled…** removes your copy of a scheme that comes with the
  IDE. **Delete…** removes a scheme of your own.

Your schemes are kept in the `Schemes` folder of your HyperXTalk documents
folder, next to `Plugins` and `Extensions`.

## Scheme Files

A scheme is a text file with the extension `.scheme`. It holds the Script
Editor's colours and, optionally, interface colours on lines of the form

    interface <key> <red>,<green>,<blue>

where the key is one of those in the table above. A scheme with no such
lines can still be used as a theme: its interface colours are worked out
from its editor colours.

## Writing Widgets That Follow a Theme

A widget follows the theme if it takes its colours from its host while it
paints, in place of fixed colours. In LiveCode Builder:

* `my foreground paint`, `my background paint` and the other `my ... paint`
  expressions already return the theme's colours where the widget has none
  of its own.
* `my ... color` returns the same colour as a `Color`, to test or adjust.
* `my own ... color` returns the colour set on the widget itself, or
  `nothing`, so a widget can tell a colour the user chose from a default.
* `the theme color "<key>"` returns one colour of the theme in use, or
  `nothing`.
* `the system appearance` returns "light", "dark" or "custom".

All of these can be read in `OnPaint`. The engine redraws every stack when
the theme or the appearance changes, so a widget written this way needs no
handler for either.
