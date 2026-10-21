# Script editor colour schemes as themes

The script editor's colorization schemes can now describe the colours of
the editor itself, not just the colours of the tokens in a script, and
scheme files are resolved by name wherever a scheme name is used.

## Scheme files

A scheme is a text file named `<Scheme Name>.scheme` containing one
setting per line:

    <type> <name> <attributes> <color>

* `type` is `group`, `class`, `keyword` or `editor`.
* `group` applies to every command (`commands`), property (`properties`),
  function (`functions`), control structure keyword (`structures`),
  chunk type (`chunks`) or operator (`operators`).
* `class` applies to a token class: `error`, `singlecomment`,
  `multicomment`, `identifier`, `literal`, `number`, `keyword`,
  `operator` or `continuation`.
* `keyword` applies to a single keyword, e.g. `keyword repeat`.
* `attributes` is a comma-separated list of `bold`, `italic`, `oblique`,
  `box`, `raised`, `underline` and `strikeout`, or `none`.
* `color` is `red,green,blue` with values from 0 to 255 and no spaces.

`editor` lines take no attributes and colour a part of the editor:

    editor background 40,42,54
    editor debugbackground 33,34,44
    editor gutterbackground 40,42,54
    editor gutterforeground 98,114,164
    editor foldingbackground 68,71,90

* `background` - the script field and handler list while editing. When
  absent, the light or dark editor background preference is used.
* `debugbackground` - the script field while the debugger has it locked.
* `gutterbackground` and `gutterforeground` - the line number gutter.
* `foldingbackground` - the code folding column.
* `selection` - the selection highlight in the script field.
* `findhighlight` - the background of text found by Find.
* `brackethighlight` - the background of a matched bracket pair.
* `placeholder` - the background of an unfilled completion placeholder.
* `executionline` - the line highlight used when jumping to an error or
  execution point.
* `handlerlisttext` - the text of the handler list. When absent, a colour
  contrasting with the list's background is used.
* `defaulthandlertext` - the dimmed text of default handlers not yet in
  the script.

A scheme may also declare which appearance it is designed for:

    editor appearance dark

The value is `light` or `dark`. The Preferences dialog uses it to list
the scheme only in the matching Colorization Scheme menu. A scheme with
no `appearance` line is listed in both.

Scheme files are looked up first in the `Schemes` folder of the user's
customisation folder, then in `Toolset/resources/supporting_files/schemes`
in the IDE. The IDE ships these schemes, each tagged for its appearance:

* Dark: Catppuccin Mocha, Dracula, GitHub Dark, Gruvbox Dark, Monokai,
  Nord, Solarized Dark, Tomorrow Night.
* Light: Atom One Light, Catppuccin Latte, GitHub Light, Gruvbox Light,
  Selenized Light, Solarized Light, Tomorrow, Xcode Light.

## Trying a scheme

1. Copy a `.scheme` file into the `Schemes` folder next to your
   `Plugins` and `Extensions` folders (create it if it does not exist),
   or save one from the Script Colour Editor.
2. Open Preferences > Script Editor and pick the scheme from the
   Colorization Scheme menu.
3. Reopen the script editor, or switch tabs, to see the editor and
   gutter colours update along with the token colours.

A scheme set with the message box, for example

    revIDESetPreference "cScriptEditor,colorization,schemedark", "Dracula"

is also resolved by name, including the bundled schemes, the next time a
script editor loads its preferences.
