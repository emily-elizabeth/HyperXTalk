script "com.hyperxtalk.library.appearance"
/*
Copyright (C) 2026 HyperXTalk

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

HyperXTalk is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
HyperXTalk. If not, see <http://www.gnu.org/licenses/>.
*/

-- ---------------------------------------------------------------------------
-- Script-local state
-- ---------------------------------------------------------------------------

-- sColorRegistry["roleName"]["light"] = color string ("r,g,b" or "#rrggbb")
-- sColorRegistry["roleName"]["dark"]  = color string
local sColorRegistry

-- ---------------------------------------------------------------------------
-- Extension lifecycle
-- ---------------------------------------------------------------------------

on extensionInitialize
   if the target is not me then
      pass extensionInitialize
   end if

   insert the script of me into back

   if the environment contains "development" then
      set the _ideoverride of me to true
   end if

   _initDefaultPalette
end extensionInitialize

on extensionFinalize
   if the target is not me then
      pass extensionFinalize
   end if

   remove the script of me from back
end extensionFinalize

/**

Title: Appearance Library

Author: HyperXTalk

Type: library

Version: 1.0.0

Description:
Dark mode-aware semantic color system for HyperXTalk developers.

Developers define named color roles — each carrying a light and dark variant —
and annotate controls with cSemantic* custom properties. The library holds the
single systemAppearanceChanged handler that walks every open developer stack
and applies the correct color automatically when the OS appearance changes.

Usage:
   -- 1. Load the library (or include it via Standalone Settings)
   start using stack "com.hyperxtalk.library.appearance"

   -- 2. Override or add roles as needed (optional — a default palette ships)
   defineSemanticColor "accent", "0,122,255", "10,132,255"

   -- 3. Annotate controls at design time or in openCard/openStack:
   set cSemanticFillColor       of btn "Primary" to "accent"
   set cSemanticForegroundColor of btn "Primary" to "accent_label"
   set cSemanticBackgroundColor of this card to "background"

   -- 4. Force an immediate apply (e.g. after loading a new card):
   applySemanticColors

   -- 5. Query a color value directly (for custom drawing):
   set the fillColor of grc "Border" to semanticColor("separator")

*/

-- ---------------------------------------------------------------------------
-- Default semantic color palette
-- Covers the most common roles. Override any of these with defineSemanticColor.
-- ---------------------------------------------------------------------------

private command _initDefaultPalette
   -- Page-level background (window / card fill)
   defineSemanticColor "background",       "255,255,255",  "0,0,0"

   -- Elevated surface: panels, list row backgrounds, grouped sections
   defineSemanticColor "surface",          "242,242,247",  "28,28,30"

   -- Primary text / foreground
   defineSemanticColor "label",            "0,0,0",        "255,255,255"

   -- Secondary / placeholder / caption text
   defineSemanticColor "label_secondary",  "60,60,67",     "174,174,178"

   -- System accent: buttons, links, highlighted controls
   defineSemanticColor "accent",           "0,122,255",    "10,132,255"

   -- Text drawn on top of an accent-colored fill
   defineSemanticColor "accent_label",     "255,255,255",  "255,255,255"

   -- Separator / divider lines between sections
   defineSemanticColor "separator",        "198,198,200",  "56,56,58"

   -- Text field / input area background
   defineSemanticColor "input_background", "255,255,255",  "44,44,46"

   -- Destructive action color (delete, remove, warning)
   defineSemanticColor "destructive",      "255,59,48",    "255,69,58"
end _initDefaultPalette

-- ---------------------------------------------------------------------------
-- Public API
-- ---------------------------------------------------------------------------

/**

Summary: Register or update a semantic color role.

Parameters:
pName: The role name, e.g. "background", "accent", "myBrand".
pLightColor: Color value used in light mode ("r,g,b" or "#rrggbb").
pDarkColor: Color value used in dark mode ("r,g,b" or "#rrggbb").

Description:
Creates or replaces the named semantic color role. Both light and dark
variants must be supplied. The name is case-sensitive.

Example:
   defineSemanticColor "brand", "0,90,200", "30,140,255"

*/
command defineSemanticColor pName, pLightColor, pDarkColor
   put pLightColor into sColorRegistry[pName]["light"]
   put pDarkColor  into sColorRegistry[pName]["dark"]
end defineSemanticColor

/**

Summary: Return the current-appearance color for a named semantic role.

Parameters:
pName: The role name previously registered with defineSemanticColor.

Returns: The color string for the current OS appearance, or empty if the role
is not registered.

Description:
Resolves the named role to either its light or dark color value based on the
current value of `the systemAppearance`. Use this when you need a color value
for custom drawing rather than a simple property assignment.

Example:
   set the foregroundColor of grc "Border" to semanticColor("separator")

*/
function semanticColor pName
   return sColorRegistry[pName][_currentMode()]
end semanticColor

/**

Summary: Immediately apply all cSemantic* custom properties to controls.

Parameters:
pTarget (optional): The long id of a specific control, card, or stack. If
omitted, every open developer stack is walked.

Description:
Pushes the current-appearance color values onto every control that carries a
cSemantic* custom property. Call this after programmatically setting custom
properties at runtime, after calling defineSemanticColor mid-session, or when
opening a new card that needs its colors initialized.

Example:
   -- Re-apply after dynamically changing a role:
   defineSemanticColor "accent", "220,40,40", "255,80,80"
   applySemanticColors

   -- Apply to a single control only:
   applySemanticColors the long id of btn "Submit"

*/
command applySemanticColors pTarget
   local tMode
   put _currentMode() into tMode

   if pTarget is not empty then
      _applyToObject pTarget, tMode
   else
      _applyToAllOpenStacks tMode
   end if
end applySemanticColors

-- ---------------------------------------------------------------------------
-- systemAppearanceChanged — the one handler developers no longer need to write
-- ---------------------------------------------------------------------------

on systemAppearanceChanged pMode, pWindowColor, pTextColor
   _applyToAllOpenStacks _currentMode()
   pass systemAppearanceChanged
end systemAppearanceChanged

-- ---------------------------------------------------------------------------
-- Private helpers
-- ---------------------------------------------------------------------------

private function _currentMode
   if the systemAppearance is "dark" then return "dark"
   return "light"
end _currentMode

-- Walk every open developer mainstack (and their substacks).
-- IDE palettes are skipped in development mode so revpalettebehavior
-- continues to own the IDE's own appearance.
private command _applyToAllOpenStacks pMode
   local tStack
   repeat for each line tStack in the mainstacks
      if _isIDEStack(tStack) then next repeat
      _applyToStack tStack, pMode
   end repeat
end _applyToAllOpenStacks

-- Apply to a named stack and recurse into its substacks.
private command _applyToStack pStackName, pMode
   _applyToObject (the long id of stack pStackName), pMode

   local tCardCount, c
   put the number of cards of stack pStackName into tCardCount
   repeat with c = 1 to tCardCount
      local tCardRef
      put "card" && c && "of stack" && quote & pStackName & quote into tCardRef
      _applyToObject (the long id of tCardRef), pMode
      _applyToControlsInContainer tCardRef, pMode
   end repeat

   -- Recurse into substacks
   local tSub
   repeat for each line tSub in the substacks of stack pStackName
      _applyToStack tSub, pMode
   end repeat
end _applyToStack

-- Walk all controls in a container (card or group), recursing into groups.
private command _applyToControlsInContainer pContainerRef, pMode
   local tCount, k, tCtrlID
   put the number of controls of pContainerRef into tCount
   repeat with k = 1 to tCount
      put the long id of control k of pContainerRef into tCtrlID
      _applyToObject tCtrlID, pMode
      -- If it is a group, recurse so nested controls are also updated
      if word 1 of tCtrlID is "group" then
         _applyToControlsInContainer tCtrlID, pMode
      end if
   end repeat
end _applyToControlsInContainer

-- Apply all recognised cSemantic* custom properties to a single object.
private command _applyToObject pLongID, pMode
   -- cSemanticFillColor → fillColor
   local tRole, tColor
   put the cSemanticFillColor of pLongID into tRole
   if tRole is not empty then
      put sColorRegistry[tRole][pMode] into tColor
      if tColor is not empty then set the fillColor of pLongID to tColor
   end if

   -- cSemanticForegroundColor → foregroundColor
   put the cSemanticForegroundColor of pLongID into tRole
   if tRole is not empty then
      put sColorRegistry[tRole][pMode] into tColor
      if tColor is not empty then set the foregroundColor of pLongID to tColor
   end if

   -- cSemanticBackgroundColor → backgroundColor
   put the cSemanticBackgroundColor of pLongID into tRole
   if tRole is not empty then
      put sColorRegistry[tRole][pMode] into tColor
      if tColor is not empty then set the backgroundColor of pLongID to tColor
   end if

   -- cSemanticBorderColor → borderColor
   put the cSemanticBorderColor of pLongID into tRole
   if tRole is not empty then
      put sColorRegistry[tRole][pMode] into tColor
      if tColor is not empty then set the borderColor of pLongID to tColor
   end if

   -- cSemanticShadowColor → shadowColor
   put the cSemanticShadowColor of pLongID into tRole
   if tRole is not empty then
      put sColorRegistry[tRole][pMode] into tColor
      if tColor is not empty then set the shadowColor of pLongID to tColor
   end if

   -- cSemanticHiliteColor → hiliteColor
   put the cSemanticHiliteColor of pLongID into tRole
   if tRole is not empty then
      put sColorRegistry[tRole][pMode] into tColor
      if tColor is not empty then set the hiliteColor of pLongID to tColor
   end if
end _applyToObject

-- Determine whether a stack is an IDE/system stack that should be left alone.
-- In standalone builds there are no IDE stacks, so this always returns false.
private function _isIDEStack pName
   if the environment is not "development" then return false

   -- revStackNameIsIDEStack lives in revcommonlibrary (IDE back-script chain).
   -- Use it when available; fall back to a conservative prefix check otherwise.
   local tResult
   try
      put revStackNameIsIDEStack(pName) into tResult
   catch tErr
      put (pName begins with "rev" \
           or pName begins with "com.livecode.") into tResult
   end try
   return tResult
end _isIDEStack
