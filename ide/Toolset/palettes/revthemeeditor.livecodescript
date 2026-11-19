script "revThemeEditor"
-- The theme editor: edits the interface colours of a scheme file.
--
-- A scheme file ("<name>.scheme") holds the script editor's colours and, as
-- "interface <key> <color>" lines, the colours of a theme for the IDE or for a
-- stack. This window edits the interface lines only; the Script Colour Editor
-- in Preferences edits the rest. A colour the scheme does not set is shown
-- with the value derived from its script editor colours, marked as derived.
--
-- The window is its own preview: the colours being edited are set as this
-- stack's lightTheme and darkTheme, so every control in it, the sample
-- controls on the right included, is drawn with them. (This is a script-only
-- stack, so the engine does not count it as an IDE stack and it takes its own
-- theme, not the IDE's.)
--
-- Saving always writes to the user's Schemes folder. Saving a scheme that is
-- bundled with the IDE writes a copy there, which then takes precedence.

local sScheme        -- the name of the scheme being edited
local sText          -- the text of its file as loaded, line endings normalised
local sExplicit      -- array: the interface colours set explicitly, by key
local sDirty         -- true if there are unsaved changes

constant kRowTop = 96
constant kRowHeight = 30
constant kPreviewLeft = 480

on preOpenStack
   if there is no button "Scheme" of me then buildInterface
   set the title of me to "Theme Editor"
   populateSchemeMenu
   if sScheme is empty or revIDESchemeFile(sScheme) is empty then
      loadScheme line 1 of revIDESchemeNames()
   else
      showScheme
   end if
   set the loc of me to the screenLoc
end preOpenStack

on closeStackRequest
   if not okToDiscard() then exit closeStackRequest
   pass closeStackRequest
end closeStackRequest

-- Sent by revIDEOpenThemeEditor to show a particular scheme.
on themeEditorEdit pScheme
   if pScheme is sScheme or revIDESchemeFile(pScheme) is empty then exit themeEditorEdit
   if not okToDiscard() then exit themeEditorEdit
   loadScheme pScheme
end themeEditorEdit

--------------------------------------------------------------------------------
-- Building the window

private function keyLabel pKey
   switch pKey
      case "foregroundColor"
         return "Text"
      case "backgroundColor"
         return "Window background"
      case "fieldColor"
         return "Field background"
      case "hiliteColor"
         return "Highlight and accent"
      case "hiliteTextColor"
         return "Text on highlight"
      case "borderColor"
         return "Borders"
      case "topColor"
         return "3D top edge"
      case "bottomColor"
         return "3D bottom edge"
      case "shadowColor"
         return "Shadow"
      case "focusColor"
         return "Focus ring"
   end switch
   return pKey
end keyLabel

-- A rectangle as the comma-separated string the rect property takes.
private function r pLeft, pTop, pRight, pBottom
   return pLeft & comma & pTop & comma & pRight & comma & pBottom
end r

private command makeLabel pName, pText, pRect, pAlign
   create field pName
   set the rect of it to pRect
   set the opaque of it to false
   set the showBorder of it to false
   set the lockText of it to true
   set the traversalOn of it to false
   set the dontWrap of it to true
   set the textAlign of it to pAlign
   set the text of it to pText
end makeLabel

private command makeButton pName, pLabel, pRect
   create button pName
   set the label of it to pLabel
   set the rect of it to pRect
end makeButton

private command buildInterface
   local tKey, tTop, tOldDefault, tRows
   put the defaultStack into tOldDefault
   set the defaultStack to the short name of me
   lock screen
   lock messages

   put the number of items of revIDESchemeInterfaceKeys() into tRows
   set the width of me to 900
   set the height of me to kRowTop + tRows * kRowHeight + 56
   set the resizable of me to false

   -- the scheme, and what can be done with it
   makeLabel "SchemeLabel", "Scheme:", "16,18,80,40", "right"
   create button "Scheme"
   set the style of it to "menu"
   set the menuMode of it to "option"
   set the rect of it to 86,17,300,41
   makeButton "Save", "Save", "316,17,386,41"
   makeButton "SaveAs", "Save As…", "392,17,482,41"
   makeButton "Revert", "Revert", "488,17,558,41"
   makeLabel "Info", empty, "16,50,884,72", "left"
   makeLabel "ColoursHeading", "Interface colours", "16,74,300,94", "left"
   set the textStyle of field "ColoursHeading" to "bold"

   -- one row for each colour
   put kRowTop into tTop
   repeat for each item tKey in revIDESchemeInterfaceKeys()
      makeLabel "label_" & tKey, keyLabel(tKey) & ":", r(16, tTop + 4, 166, tTop + 24), "right"
      create graphic "swatch_" & tKey
      set the style of it to "rectangle"
      set the rect of it to r(174, tTop + 3, 214, tTop + 25)
      set the opaque of it to true
      set the lineSize of it to 1
      makeLabel "value_" & tKey, empty, r(222, tTop + 4, 372, tTop + 24), "left"
      makeButton "set_" & tKey, "Choose…", r(376, tTop + 2, 452, tTop + 26)
      makeButton "clear_" & tKey, "×", r(456, tTop + 2, 480, tTop + 26)
      set the toolTip of button ("clear_" & tKey) to "Remove this colour from the scheme"
      add kRowHeight to tTop
   end repeat

   -- sample controls, drawn in the colours being edited like everything else
   makeLabel "PreviewHeading", "Preview", r(kPreviewLeft + 20, 74, kPreviewLeft + 300, 94), "left"
   set the textStyle of field "PreviewHeading" to "bold"
   makeButton "SampleButton", "Button", r(kPreviewLeft + 20, kRowTop, kPreviewLeft + 120, kRowTop + 24)
   makeButton "SampleDefault", "Default", r(kPreviewLeft + 130, kRowTop, kPreviewLeft + 230, kRowTop + 24)
   set the default of button "SampleDefault" to true
   makeButton "SampleSelected", "Selected", r(kPreviewLeft + 240, kRowTop, kPreviewLeft + 340, kRowTop + 24)
   set the style of button "SampleSelected" to "roundrect"
   set the hilite of button "SampleSelected" to true
   set the autoHilite of button "SampleSelected" to false
   makeButton "SampleCheck", "Checkbox", r(kPreviewLeft + 20, kRowTop + 34, kPreviewLeft + 120, kRowTop + 56)
   set the style of button "SampleCheck" to "checkbox"
   set the showBorder of button "SampleCheck" to false
   set the hiliteBorder of button "SampleCheck" to false
   set the hilite of button "SampleCheck" to true
   makeButton "SampleRadio", "Radio button", r(kPreviewLeft + 130, kRowTop + 34, kPreviewLeft + 240, kRowTop + 56)
   set the style of button "SampleRadio" to "radiobutton"
   set the showBorder of button "SampleRadio" to false
   set the hiliteBorder of button "SampleRadio" to false
   set the hilite of button "SampleRadio" to true
   create button "SampleOption"
   set the style of it to "menu"
   set the menuMode of it to "option"
   set the text of it to "Option menu" & return & "Second choice"
   set the rect of it to r(kPreviewLeft + 250, kRowTop + 33, kPreviewLeft + 390, kRowTop + 57)

   create field "SampleField"
   set the rect of it to r(kPreviewLeft + 20, kRowTop + 68, kPreviewLeft + 390, kRowTop + 92)
   set the text of it to "An editable field: select some of this text"
   create field "SampleLinks"
   set the rect of it to r(kPreviewLeft + 20, kRowTop + 100, kPreviewLeft + 390, kRowTop + 124)
   set the lockText of it to true
   set the traversalOn of it to false
   set the text of it to "Locked text with a link in it."
   set the textStyle of word 5 of field "SampleLinks" to "link"
   create field "SampleList"
   set the rect of it to r(kPreviewLeft + 20, kRowTop + 132, kPreviewLeft + 390, kRowTop + 196)
   set the listBehavior of it to true
   set the text of it to "List field, first line" & return & "Second line, selected" & return & "Third line"
   set the hilitedLine of it to 2
   create scrollbar "SampleProgress"
   set the style of it to "progress"
   set the rect of it to r(kPreviewLeft + 20, kRowTop + 208, kPreviewLeft + 390, kRowTop + 224)
   set the thumbPosition of it to 40000
   try
      create widget "SampleSegmented" as "com.livecode.widget.segmented"
      set the rect of it to r(kPreviewLeft + 20, kRowTop + 238, kPreviewLeft + 390, kRowTop + 268)
      set the itemNames of it to "one,two,three"
      set the itemLabels of it to "One,Two,Three"
      set the hilitedItemNames of it to "two"
      set the foregroundColor of widget "SampleSegmented" to empty
      set the backgroundColor of widget "SampleSegmented" to empty
      set the hiliteColor of widget "SampleSegmented" to empty
      set the borderColor of widget "SampleSegmented" to empty
   end try
   makeLabel "PreviewNote", "System-drawn parts (button bezels, checkbox, menu arrows) follow the current light or dark mode, not the scheme.", \
         r(kPreviewLeft + 20, kRowTop + 276, kPreviewLeft + 400, kRowTop + 312), "left"
   set the dontWrap of field "PreviewNote" to false
   set the textSize of field "PreviewNote" to 11

   unlock messages
   unlock screen
   set the defaultStack to tOldDefault
end buildInterface

--------------------------------------------------------------------------------
-- Loading and showing a scheme

private command populateSchemeMenu
   lock messages
   set the text of button "Scheme" of me to revIDESchemeNames()
   if sScheme is not empty then
      set the label of button "Scheme" of me to sScheme
   end if
   unlock messages
end populateSchemeMenu

private command loadScheme pScheme
   local tFile
   put revIDESchemeFile(pScheme) into tFile
   if tFile is empty then exit loadScheme
   put pScheme into sScheme
   put URL ("file:" & tFile) into sText
   replace CRLF with LF in sText
   replace numToCodepoint(13) with LF in sText
   put revIDESchemeExplicitInterfaceColors(sText) into sExplicit
   put false into sDirty
   showScheme
end loadScheme

-- The scheme's text with its interface lines replaced by the colours being
-- edited: what would be written by Save.
private function editedText
   local tLine, tResult, tKey
   repeat for each line tLine in sText
      if word 1 of tLine is "interface" then next repeat
      put tLine & return after tResult
   end repeat
   repeat while char -1 of tResult is return
      delete char -1 of tResult
   end repeat
   repeat for each item tKey in revIDESchemeInterfaceKeys()
      if sExplicit[tKey] is not empty then
         put return & "interface" && tKey && sExplicit[tKey] after tResult
      end if
   end repeat
   return tResult & return
end editedText

private function schemeAppearance
   local tLine
   repeat for each line tLine in sText
      if word 1 of tLine is "editor" and word 2 of tLine is "appearance" then return word 3 of tLine
   end repeat
   return empty
end schemeAppearance

private function schemeIsBundled
   return not (revIDESchemeFile(sScheme) begins with revEnvironmentUserColorSchemesPath())
end schemeIsBundled

private command showScheme
   local tTheme, tKey, tInfo, tValue, tError
   lock screen
   put revIDESchemeInterfaceThemeFromText(editedText()) into tTheme

   repeat for each item tKey in revIDESchemeInterfaceKeys()
      if sExplicit[tKey] is not empty then
         put sExplicit[tKey] into tValue
      else if tTheme[tKey] is not empty then
         put tTheme[tKey] && "(derived)" into tValue
      else if tKey is "hiliteTextColor" then
         put "automatic" into tValue
      else
         put "system colour" into tValue
      end if
      set the text of field ("value_" & tKey) of me to tValue
      if tTheme[tKey] is empty then
         set the opaque of graphic ("swatch_" & tKey) of me to false
      else
         set the opaque of graphic ("swatch_" & tKey) of me to true
         set the backgroundColor of graphic ("swatch_" & tKey) of me to tTheme[tKey]
      end if
      set the enabled of button ("clear_" & tKey) of me to (sExplicit[tKey] is not empty)
   end repeat

   -- the window previews the colours whichever appearance the system is in
   try
      do "set the lightTheme of me to tTheme"
      do "set the darkTheme of me to tTheme"
   catch tError
   end try

   switch schemeAppearance()
      case "light"
         put "For light mode." into tInfo
         break
      case "dark"
         put "For dark mode." into tInfo
         break
      default
         put "For light and dark mode." into tInfo
   end switch
   if schemeIsBundled() then
      put " Bundled with the IDE: saving writes a copy to your Schemes folder, which then takes its place." after tInfo
   else
      put " In your Schemes folder." after tInfo
   end if
   set the text of field "Info" of me to tInfo

   lock messages
   set the label of button "Scheme" of me to sScheme
   unlock messages
   set the enabled of button "Save" of me to sDirty
   set the enabled of button "Revert" of me to sDirty
   if sDirty then
      set the title of me to "Theme Editor: " & sScheme & " (edited)"
   else
      set the title of me to "Theme Editor: " & sScheme
   end if
   unlock screen
end showScheme

--------------------------------------------------------------------------------
-- Editing

private command chooseColor pKey
   local tTheme, tStart
   put revIDESchemeInterfaceThemeFromText(editedText()) into tTheme
   put tTheme[pKey] into tStart
   if tStart is empty then put "128,128,128" into tStart
   answer color with tStart
   if the result is "Cancel" or it is empty then exit chooseColor
   put item 1 to 3 of it into sExplicit[pKey]
   put true into sDirty
   showScheme
end chooseColor

private command clearColor pKey
   if sExplicit[pKey] is empty then exit clearColor
   delete variable sExplicit[pKey]
   put true into sDirty
   showScheme
end clearColor

on mouseUp
   local tName
   put the short name of the target into tName
   set the itemDelimiter to "_"
   switch item 1 of tName
      case "set"
      case "swatch"
         chooseColor item 2 of tName
         break
      case "clear"
         clearColor item 2 of tName
         break
      case "Save"
         saveScheme sScheme
         break
      case "SaveAs"
         saveSchemeAs
         break
      case "Revert"
         loadScheme sScheme
         break
      default
         pass mouseUp
   end switch
end mouseUp

on menuPick pItem
   if the short name of the target is not "Scheme" then pass menuPick
   if pItem is sScheme then exit menuPick
   if not okToDiscard() then
      lock messages
      set the label of button "Scheme" of me to sScheme
      unlock messages
      exit menuPick
   end if
   loadScheme pItem
end menuPick

-- True if there are no unsaved changes, or the user chose to save or discard them.
private function okToDiscard
   if sDirty is not true then return true
   answer question "Save the changes to" && quote & sScheme & quote & "?" with "Cancel" or "Discard" or "Save"
   switch it
      case "Save"
         saveScheme sScheme
         return (sDirty is not true)
      case "Discard"
         put false into sDirty
         return true
   end switch
   return false
end okToDiscard

--------------------------------------------------------------------------------
-- Saving

private command saveScheme pName
   local tFolder, tFile, tText
   put revEnvironmentUserColorSchemesPath() into tFolder
   if there is no folder tFolder then create folder tFolder
   put tFolder & slash & pName & ".scheme" into tFile
   put editedText() into tText
   put tText into URL ("file:" & tFile)
   if the result is not empty then
      answer error "The scheme could not be saved:" && the result
      exit saveScheme
   end if
   put pName into sScheme
   put tText into sText
   delete char -1 of sText
   put false into sDirty
   populateSchemeMenu
   showScheme
   -- if the IDE is using this scheme as its theme, show the change at once
   revIDEApplyThemePreference
end saveScheme

private command saveSchemeAs
   local tName
   ask question "Save the scheme as:" with sScheme && "copy"
   if the result is "Cancel" or it is empty then exit saveSchemeAs
   put it into tName
   if tName contains slash or tName contains ":" or tName is "default" then
      answer error "That name cannot be used for a scheme."
      exit saveSchemeAs
   end if
   if there is a file (revEnvironmentUserColorSchemesPath() & slash & tName & ".scheme") then
      answer warning "A scheme named" && quote & tName & quote && "is already in your Schemes folder. Replace it?" with "Cancel" or "Replace"
      if it is not "Replace" then exit saveSchemeAs
   end if
   saveScheme tName
end saveSchemeAs
