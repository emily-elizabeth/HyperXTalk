script "ideautocompletelibrary"

-- Language words (commands, functions, properties, messages, keywords...),
-- one per line.  Taken from the dictionary where possible so the names keep
-- their camelCase, which is what the "hump" matching works on.
local sBuiltinWords
local sBuiltinFromDocs
local sBuiltinBuiltAt

-- Message path handler names: ruggedId -> return-separated list.  Refreshed
-- each time a new completion session starts, so handlers added since the
-- editor was opened are picked up.
local sPathHandlers

-- True once the user has moved the selection with the arrow keys.  Return
-- only accepts a completion after that; Tab always does.
local sNavigated

-- True while a debounced update is waiting to run.
local sUpdatePending

-- Popup state
local sPopupShowing
local sCurrentEditorId

-- Pending debounce timer
local sUpdateTimer

-- Pending deferred-hide timer (gives the pointer time to travel to the popup)
local sHideTimer

-- Mailbox for the debounced update (avoids quoting issues with long IDs in send)
local sPendingEditorId
local sPendingObjectId

-- Last clicked line in the popup (1-based). Set by PopupClicked.
local sSelectedLine

-- Caret state snapshotted at popup-show time (while the editor has focus).
-- Used by AcceptCompletion so that a popup click, which may move OS focus
-- away from the editor, does not cause getCaretToken / selectedChunk to
-- read from the wrong field.
local sSavedTokenStart
local sSavedToken

-- While true, ideAutocompleteCancelFind is suppressed.  Set around
-- show stack + focus-return so that the focus-change events those
-- operations fire do not immediately dismiss the popup.
local sSuppressCancel

constant kPopupName = "ideAutoCompletePopup"
constant kMinChars = 2
constant kMaxCompletions = 50
constant kMaxVisible = 10
constant kPopupWidth = 220
constant kItemHeight = 18
-- How long (ms) to wait before hiding the popup after a cancel event.
-- Increase this if the popup disappears before you can click an item.
constant kHideDelay = 400

--------------------------------------------------------------------------------
-- Library lifecycle
--------------------------------------------------------------------------------

on extensionInitialize
   -- No "target is me" guard: the guard could prevent initialization when the
   -- stack is loaded via send rather than dispatch.
   insert the script of me into back
   put false into sPopupShowing
   -- Popup is created lazily in ideAutocomplete_ShowPopup.
end extensionInitialize

on extensionFinalize
   cancel sUpdateTimer
   cancel sHideTimer
   -- Hide rather than delete: explicitly destroying a GTK window during engine
   -- shutdown causes a segfault on Linux.  The engine will clean up all stacks
   -- when it exits, so hiding is sufficient here.
   if there is a stack kPopupName then
      hide stack kPopupName
   end if
   remove the script of me from back
end extensionFinalize

--------------------------------------------------------------------------------
-- Public entry points called by revsecommoneditorbehavior
--------------------------------------------------------------------------------

-- Called when the object being edited changes.
-- Builds and caches the word list for the object's message path.
command ideAutocompleteIntrospectMessagePath pObjectId
   ideAutocomplete_EnsureBuiltinWords
   ideAutocomplete_RefreshPathHandlers pObjectId
end ideAutocompleteIntrospectMessagePath

-- Called when an object's script editor tab is closed.
command ideAutocompleteClearObjectCache pObject
   delete variable sPathHandlers[revRuggedId(pObject)]
end ideAutocompleteClearObjectCache

-- Called on every relevant keypress. Debounced to avoid firing on every char.
command ideAutocompleteUpdate pEditorId, pObjectId, pClasses
   cancel sUpdateTimer
   -- Store params in script locals: long IDs contain quotes and cannot be
   -- safely embedded in a send string.
   put pEditorId into sPendingEditorId
   put pObjectId into sPendingObjectId
   send "ideAutocomplete_DoUpdate" to me in 150 milliseconds
   put the result into sUpdateTimer
   put true into sUpdatePending
end ideAutocompleteUpdate

-- Called on mouseMove, closeField, exitField, escapeKey, returnInField.
-- pImmediate (escapeKey, returnInField) hides the popup straight away
-- instead of waiting to see whether the pointer is heading for it.
command ideAutocompleteCancelFind pEditorId, pImmediate
   cancel sUpdateTimer
   put false into sUpdatePending
   if pImmediate is true then
      ideAutocomplete_HidePopup
      exit ideAutocompleteCancelFind
   end if
   -- Suppressed while show stack + focus-return are in flight to prevent the
   -- focus-change events those operations fire from immediately closing the popup.
   if sSuppressCancel then
      exit ideAutocompleteCancelFind
   end if
   if sPopupShowing then
      -- The mouse is already over the popup (user hovering to read/click): don't hide.
      if the mouseStack is kPopupName then
         exit ideAutocompleteCancelFind
      end if
      -- mouseMove fires on every pixel of travel, so the pointer may not have
      -- reached the popup window yet.  Defer the hide long enough for the user
      -- to move the pointer there.  ideAutocomplete_DeferredHide re-checks
      -- mouseStack before actually hiding.
      cancel sHideTimer
      send "ideAutocomplete_DeferredHide" to me in kHideDelay milliseconds
      put the result into sHideTimer
      exit ideAutocompleteCancelFind
   end if
   ideAutocomplete_HidePopup
end ideAutocompleteCancelFind

-- Called when Tab is pressed.  Accepts the highlighted completion.
-- Returns true if the key should carry on to its normal job (formatting).
command ideAutocompleteHandleTab pEditorId, pObjectId
   ideAutocomplete_FlushPendingUpdate
   if not sPopupShowing then
      return true
   end if
   -- The word is already complete: let Tab format as usual.
   set the caseSensitive to true
   if ideAutocomplete_SelectedWord() is sSavedToken then
      ideAutocomplete_HidePopup
      return true
   end if
   ideAutocomplete_AcceptCompletion pEditorId
   return false
end ideAutocompleteHandleTab

-- Called when Return is pressed.  Only accepts once the user has picked an
-- entry with the arrow keys, so that ending a line on a word that happens to
-- have completions (put it, into len ...) still just inserts a new line.
-- Returns true if the key should carry on to its normal job.
command ideAutocompleteHandleReturn pEditorId
   if not (sPopupShowing and sNavigated is true) or sUpdatePending is true then
      return true
   end if
   ideAutocomplete_AcceptCompletion pEditorId
   return false
end ideAutocompleteHandleReturn

-- Called for the arrow keys.  Up and down move through the list while it is
-- showing; left and right dismiss it.
-- Returns true if the key should carry on to its normal job.
command ideAutocompleteHandleArrow pEditorId, pKeyName, pObjectId
   if not sPopupShowing then
      return true
   end if
   if pKeyName is not among the items of "up,down" then
      ideAutocomplete_HidePopup
      return true
   end if
   
   local tField, tCount
   put the long id of field "completions" of stack kPopupName into tField
   put the number of lines of the text of tField into tCount
   if pKeyName is "down" then
      add 1 to sSelectedLine
      if sSelectedLine > tCount then
         put 1 into sSelectedLine
      end if
   else
      subtract 1 from sSelectedLine
      if sSelectedLine < 1 then
         put tCount into sSelectedLine
      end if
   end if
   put true into sNavigated
   set the hilitedLine of tField to sSelectedLine
   
   -- Keep the highlighted line in view
   local tLineHeight, tTop, tVisible
   put the effective textHeight of tField into tLineHeight
   put (sSelectedLine - 1) * tLineHeight into tTop
   put the height of tField - 8 into tVisible
   if tTop < the vScroll of tField then
      set the vScroll of tField to tTop
   else if tTop + tLineHeight > the vScroll of tField + tVisible then
      set the vScroll of tField to tTop + tLineHeight - tVisible
   end if
   return false
end ideAutocompleteHandleArrow

-- Called when the Help key is pressed. Opens the dictionary for the hovered
-- completion if the popup is showing. Returns true if handled.
command ideAutocompleteHandleHelp pEditorId, pObjectId
   if not sPopupShowing then
      return false
   end if
   local tWord
   put line sSelectedLine of the text of field "completions" of stack kPopupName into tWord
   if tWord is not empty then
      dispatch "revIDEOpenDictionary" with tWord
   end if
   return true
end ideAutocompleteHandleHelp

--------------------------------------------------------------------------------
-- Private: word list
--------------------------------------------------------------------------------

-- Builds the list of language words once.  The dictionary is preferred
-- because its entry names are camelCase (openCard, itemDelimiter); the engine's
-- own lists are all lower case and only fill in anything the dictionary lacks.
private command ideAutocomplete_EnsureBuiltinWords
   if sBuiltinFromDocs is true then
      exit ideAutocomplete_EnsureBuiltinWords
   end if
   -- The dictionary may not be ready early in startup: retry now and then.
   if sBuiltinWords is not empty and the seconds - sBuiltinBuiltAt < 10 then
      exit ideAutocomplete_EnsureBuiltinWords
   end if
   
   -- The documentation cache holds extensions and libraries (and, depending
   -- on how it was built, the language itself); the database shipped with the
   -- IDE always has the language.  Read both.
   local tDocNames, tConnection, tOwnConnection, tPath
   try
      ideDocsInitialize
      put the result into tConnection
      put ideAutocomplete_DocNames(tConnection) into tDocNames
   catch tError
   end try
   try
      put revIDESpecialFolderPath("API") & slash & "api.sqlite" into tPath
      if there is a file tPath then
         put revOpenDatabase("sqlite", tPath) into tOwnConnection
         if tOwnConnection is a number then
            put return & ideAutocomplete_DocNames(tOwnConnection) after tDocNames
            revCloseDatabase tOwnConnection
         end if
      end if
   catch tError
   end try
   filter lines of tDocNames with regex pattern "^[A-Za-z_][A-Za-z0-9_]*$"
   
   local tEngineNames
   put commandNames() & return & functionNames() & return & propertyNames() into tEngineNames
   filter lines of tEngineNames with regex pattern "^[A-Za-z_][A-Za-z0-9_]*$"
   
   put ideAutocomplete_MergeWords(tDocNames, tEngineNames) into sBuiltinWords
   put (the number of lines of tDocNames > 1000) into sBuiltinFromDocs
   put the seconds into sBuiltinBuiltAt
end ideAutocomplete_EnsureBuiltinWords

-- Returns the entry names in a dictionary database that can appear in script.
private function ideAutocomplete_DocNames pConnection
   if pConnection is not a number then
      return empty
   end if
   local tNames
   put revDataFromQuery(tab, return, pConnection, \
         "SELECT DISTINCT entry_name FROM dictionary_data WHERE library_name NOT LIKE '%Builder' " & \
         "AND entry_type NOT IN ('glossary','library')") into tNames
   if tNames begins with "revdberr" then
      return empty
   end if
   return tNames
end ideAutocomplete_DocNames

-- Merges two word lists, ignoring case; where both have the same word the
-- spelling in pPreferred wins.
private function ideAutocomplete_MergeWords pPreferred, pOther
   local tSeenA, tResult, tKey
   repeat for each line tLine in pPreferred & return & pOther
      if tLine is empty then next repeat
      put toLower(tLine) into tKey
      if tSeenA[tKey] then next repeat
      put true into tSeenA[tKey]
      put tLine & return after tResult
   end repeat
   delete the last char of tResult
   return tResult
end ideAutocomplete_MergeWords

-- Handler names the object can call through its message path.  Private
-- handlers are left out: the object's own come from the editor text instead
-- (see ideAutocomplete_ScriptHandlers) and other objects' are not callable.
-- Libraries and back scripts are part of the path, so their handlers count.
private command ideAutocomplete_RefreshPathHandlers pObjectId
   local tNames
   if pObjectId is not empty and there is a pObjectId then
      local tHandlers
      try
         put the effective revAvailableHandlers of pObjectId into tHandlers
      catch tError
         put empty into tHandlers
      end try
      -- Handlers are listed object by object; the first line for each object
      -- ends with its long id.  The IDE's own libraries sit in the message
      -- path too, and their internals are of no use in a user's script.
      local tOwner, tSkip
      put false into tSkip
      repeat for each line tLine in tHandlers
         put word 5 to -1 of tLine into tOwner
         if tOwner is not empty then
            try
               put revIDEObjectIsOnIDEStack(tOwner) into tSkip
            catch tError
               put false into tSkip
            end try
         end if
         if tSkip or char 1 of word 1 of tLine is "P" then next repeat
         if word 2 of tLine is not empty then
            put word 2 of tLine & return after tNames
         end if
      end repeat
      delete the last char of tNames
   end if
   put tNames into sPathHandlers[revRuggedId(pObjectId)]
end ideAutocomplete_RefreshPathHandlers

-- Handler names declared in the text currently in the editor, applied or not.
-- This is what makes a handler completable from elsewhere in its own script.
private function ideAutocomplete_ScriptHandlers pField
   local tScript, tNames, tName
   put the text of pField into tScript
   filter lines of tScript with regex pattern \
         "(?i)^\s*(private\s+)?(on|command|function|getprop|setprop|before|after)\s+[A-Za-z_]"
   repeat for each line tLine in tScript
      if word 1 of tLine is "private" then
         put word 3 of tLine into tName
      else
         put word 2 of tLine into tName
      end if
      if matchText(tName, "^([A-Za-z_][A-Za-z0-9_]*)", tName) then
         put tName & return after tNames
      end if
   end repeat
   delete the last char of tNames
   return tNames
end ideAutocomplete_ScriptHandlers

-- Returns the regex a word must match to complete pToken.  The first letter
-- must start the word; each later letter either follows on directly or starts
-- a later "hump" (a capital, or a letter after an underscore).  So "oc" and
-- "opCa" both find openCard, as well as everything a plain prefix would.
private function ideAutocomplete_MatchPattern pToken
   local tPattern, tLower, tUpper
   repeat for each char tChar in pToken
      put toLower(tChar) into tLower
      put toUpper(tChar) into tUpper
      if tPattern is empty then
         put "^[" & tLower & tUpper & "]" into tPattern
      else if not matchText(tChar, "[A-Za-z]") then
         -- digit or underscore
         put "(?:" & tChar & "|[a-z0-9]*" & tChar & ")" after tPattern
      else
         put "(?:[" & tLower & tUpper & "]|[a-z0-9]*(?:" & tUpper & "|_[" & tLower & tUpper & "]))" after tPattern
      end if
   end repeat
   return tPattern
end ideAutocomplete_MatchPattern

-- Returns the words that complete pToken, best first: an exact match, then
-- plain prefixes, then hump matches.  Within each of those the script's own
-- handlers (pHandlers) come before language words (pBuiltins), alphabetically.
private function ideAutocomplete_Matches pToken, pHandlers, pBuiltins
   -- The pattern tells capitals from small letters, so match case-sensitively
   local tPattern
   put ideAutocomplete_MatchPattern(pToken) into tPattern
   set the caseSensitive to true
   filter lines of pHandlers with regex pattern tPattern
   filter lines of pBuiltins with regex pattern tPattern
   set the caseSensitive to false
   
   local tKeyed, tSource, tSeenA
   put "a" into tSource
   repeat for each line tLine in pHandlers & return & "/" & return & pBuiltins
      if tLine is "/" then
         put "b" into tSource
         next repeat
      end if
      if tLine is empty or tSeenA[toLower(tLine)] then
         next repeat
      end if
      put true into tSeenA[toLower(tLine)]
      if tLine is pToken then
         put "0" & tSource & tLine & return after tKeyed
      else if tLine begins with pToken then
         put "1" & tSource & tLine & return after tKeyed
      else
         put "2" & tSource & tLine & return after tKeyed
      end if
   end repeat
   delete the last char of tKeyed
   sort lines of tKeyed ascending text
   
   local tMatches
   repeat for each line tLine in line 1 to kMaxCompletions of tKeyed
      put char 3 to -1 of tLine & return after tMatches
   end repeat
   delete the last char of tMatches
   return tMatches
end ideAutocomplete_Matches

--------------------------------------------------------------------------------
-- Private: debounced update
--------------------------------------------------------------------------------

-- Not private: timer-fired sends cannot reach private handlers.
command ideAutocomplete_DoUpdate
   put false into sUpdatePending
   
   -- Read from the mailbox set by ideAutocompleteUpdate
   local pEditorId, pObjectId
   put sPendingEditorId into pEditorId
   put sPendingObjectId into pObjectId
   if pEditorId is empty or there is not a pEditorId then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   
   -- Get the partial word at the cursor
   dispatch "getCaretToken" to pEditorId
   local tToken
   put the result into tToken
   
   dispatch function "getScriptField" to pEditorId
   local tField
   put the result into tField
   
   if the number of chars of tToken < kMinChars or \
         not matchText(tToken, "^[A-Za-z_][A-Za-z0-9_]*$") or \
         tField is empty or there is not a tField then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   
   -- Only complete when the caret sits at the end of the word being typed,
   -- and not inside a string literal.
   local tCursor, tTokenStart, tBefore
   put word 2 of the selectedChunk of tField into tCursor
   put tCursor - the number of chars of tToken into tTokenStart
   if tTokenStart < 1 or the text of char tTokenStart to (tCursor - 1) of tField is not tToken then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   put empty into tBefore
   if tTokenStart > 1 then
      put the last line of (the text of char max(1, tTokenStart - 1000) to (tTokenStart - 1) of tField) into tBefore
   end if
   set the itemDelimiter to quote
   if tBefore is not empty and the number of items of (tBefore & "x") mod 2 is 0 then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   
   -- Gather the candidate words.  The message path is only re-read when a
   -- new popup is about to appear, not on every character.
   local tRuggedId
   put revRuggedId(pObjectId) into tRuggedId
   ideAutocomplete_EnsureBuiltinWords
   if not sPopupShowing or tRuggedId is not among the keys of sPathHandlers then
      ideAutocomplete_RefreshPathHandlers pObjectId
   end if
   
   local tHandlers
   put ideAutocomplete_ScriptHandlers(tField) & return & sPathHandlers[tRuggedId] into tHandlers
   
   local tMatches
   put ideAutocomplete_Matches(tToken, tHandlers, sBuiltinWords) into tMatches
   
   -- Nothing to show, or the only match is exactly the token already typed
   set the caseSensitive to true
   if tMatches is empty or tMatches is tToken then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   set the caseSensitive to false
   
   -- Snapshot the caret position now, while the editor field has focus.
   -- AcceptCompletion reads these saved values so a popup click (which may
   -- shift OS focus away from the editor) doesn't corrupt the offset.
   put tTokenStart into sSavedTokenStart
   put tToken into sSavedToken
   
   ideAutocomplete_ShowPopup pEditorId, tMatches
end ideAutocomplete_DoUpdate

-- Runs a debounced update straight away if one is waiting, so a key pressed
-- hard on the heels of typing acts on the list for what is actually typed.
private command ideAutocomplete_FlushPendingUpdate
   if sUpdatePending is true then
      cancel sUpdateTimer
      ideAutocomplete_DoUpdate
   end if
end ideAutocomplete_FlushPendingUpdate

private function ideAutocomplete_SelectedWord
   return line sSelectedLine of the text of field "completions" of stack kPopupName
end ideAutocomplete_SelectedWord

-- Not private: timer-fired sends cannot reach private handlers.
command ideAutocomplete_ClearSuppressCancel
   put false into sSuppressCancel
end ideAutocomplete_ClearSuppressCancel

-- Not private: timer-fired sends cannot reach private handlers.
-- Hides the popup only if the pointer has not moved onto the popup window.
command ideAutocomplete_DeferredHide
   if the mouseStack is kPopupName then
      exit ideAutocomplete_DeferredHide
   end if
   ideAutocomplete_HidePopup
end ideAutocomplete_DeferredHide

--------------------------------------------------------------------------------
-- Private: popup creation
--------------------------------------------------------------------------------

private command ideAutocomplete_CreatePopup
   if there is a stack kPopupName then
      delete stack kPopupName
   end if
   
   create invisible stack kPopupName
   set the style of stack kPopupName to "palette"
   set the decorations of stack kPopupName to empty
   set the resizable of stack kPopupName to false
   set the width of stack kPopupName to kPopupWidth
   set the height of stack kPopupName to kItemHeight
   
   create field "completions" in card 1 of stack kPopupName
   --   set the style of field "completions" of stack kPopupName to "list"
   set the listBehavior of field "completions" of stack kPopupName to TRUE
   set the lockText of field "completions" of stack kPopupName to true
   try
      set the traversalOn of field "completions" of stack kPopupName to false
   catch tErr
      -- traversalOn may not be supported on this platform; non-fatal
   end try
   set the showBorder of field "completions" of stack kPopupName to true
   set the vScrollbar of field "completions" of stack kPopupName to false
   set the hScrollbar of field "completions" of stack kPopupName to false
   set the textSize of field "completions" of stack kPopupName to 12
   set the fixedLineHeight of field "completions" of stack kPopupName to true
   
   -- Wire up click-to-accept on the popup field
   set the script of field "completions" of stack kPopupName to \
         "on mouseUp" & return & \
         "   send ideAutocomplete_PopupClicked to stack " & \
         quote & kPopupName & quote & return & \
         "end mouseUp"
end ideAutocomplete_CreatePopup

--------------------------------------------------------------------------------
-- Private: show / hide
--------------------------------------------------------------------------------

private command ideAutocomplete_ShowPopup pEditorId, pMatches
   -- Lazy init: create the popup stack if extensionInitialize hasn't run yet
   if there is not a stack kPopupName then
      ideAutocomplete_CreatePopup
      put false into sPopupShowing
   end if

   put pEditorId into sCurrentEditorId

   local tCount
   put the number of lines of pMatches into tCount

   local tVisibleRows
   put min(tCount, kMaxVisible) into tVisibleRows

   local tPopupHeight
   put tVisibleRows * kItemHeight into tPopupHeight

   -- Show scrollbar only when there are more items than fit in the visible rows
   set the vScrollbar of field "completions" of stack kPopupName to (tCount > kMaxVisible)

   -- Resize stack and field together
   set the height of stack kPopupName to tPopupHeight
   set the width of stack kPopupName to kPopupWidth
   set the rect of field "completions" of stack kPopupName to \
         0, 0, kPopupWidth, tPopupHeight

   set the text of field "completions" of stack kPopupName to pMatches
   put 1 into sSelectedLine
   put false into sNavigated
   set the vScroll of field "completions" of stack kPopupName to 0
   try
      set the hilitedLine of field "completions" of stack kPopupName to 1
   catch tErr
   end try

   -- Calculate screen position below the current caret line
   local tX, tY, tLineHeight
   ideAutocomplete_GetCaretScreenPos pEditorId, tX, tY, tLineHeight

   -- Keep the popup on the same screen as the caret.
   local tScreenRect
   put ideAutocomplete_ScreenRectFor(pEditorId, tX, tY) into tScreenRect
   if tX + kPopupWidth > item 3 of tScreenRect then
      put item 3 of tScreenRect - kPopupWidth into tX
   end if
   if tX < item 1 of tScreenRect then
      put item 1 of tScreenRect into tX
   end if
   if tY + tPopupHeight > item 4 of tScreenRect then
      -- Flip above the current line
      subtract tPopupHeight + tLineHeight from tY
   end if

   set the topleft of stack kPopupName to tX, tY

   -- Suppress cancelFind while show stack + refocus fire focus-change events.
   -- On Windows, palette windows still activate when shown and generate
   -- additional async focus events, so we keep the flag set longer there.
   put true into sSuppressCancel
   if not sPopupShowing then
      show stack kPopupName
      put true into sPopupShowing
      -- Some window managers place a window themselves when it is first
      -- mapped, ignoring where it was put while hidden: position it again.
      if the topleft of stack kPopupName is not (tX,tY) then
         set the topleft of stack kPopupName to tX, tY
      end if
   end if

   -- Return keyboard focus to the script editor so the user can keep typing.
   -- The popup is purely a visual overlay; it must not own the keyboard.
   ideAutocomplete_RefocusEditor pEditorId
   if the platform contains "win" then
      send "ideAutocomplete_ClearSuppressCancel" to me in 150 milliseconds
   else
      send "ideAutocomplete_ClearSuppressCancel" to me in 0 milliseconds
   end if
end ideAutocomplete_ShowPopup

private command ideAutocomplete_HidePopup
   cancel sHideTimer
   if sPopupShowing then
      hide stack kPopupName
      put false into sPopupShowing
   end if
   put empty into sCurrentEditorId
end ideAutocomplete_HidePopup

--------------------------------------------------------------------------------
-- Private: focus management
--------------------------------------------------------------------------------

-- Returns keyboard focus to the script editor field so the popup acts as a
-- passive overlay and the user can continue typing without interruption.
private command ideAutocomplete_RefocusEditor pEditorId
   dispatch function "getScriptField" to pEditorId
   if it is "handled" then
      local tField
      put the result into tField
      if tField is not empty and there is a tField then
         -- Only call focus on if the script field doesn't already hold LiveCode
         -- focus.  Re-focusing an already-focused field fires exitField, which
         -- triggers ideAutocompleteCancelFind and immediately dismisses the popup.
         if the long id of the focusedObject is not tField then
            -- On Windows, focus on fires synchronous exitField which immediately
            -- dismisses the popup, so defer it.  On macOS the deferred form
            -- doesn't work, so call directly.
            if the platform contains "win" then
               send "FocusOnField tField" to me in 1 milliseconds
            else
               focus on tField
            end if
         end if
      end if
   end if
end ideAutocomplete_RefocusEditor

-- Not private: timer-fired sends cannot reach private handlers.
on FocusOnField tField
   focus on tField
end FocusOnField

--------------------------------------------------------------------------------
-- Private: caret screen position
--------------------------------------------------------------------------------

-- Returns in rX,rY the screen position just below the start of the word
-- being typed, and in rLineHeight the height of a line of the editor.
private command ideAutocomplete_GetCaretScreenPos pEditorId, @rX, @rY, @rLineHeight
   dispatch function "getScriptField" to pEditorId
   local tField
   put the result into tField
   put kItemHeight into rLineHeight
   
   if tField is empty or not (there is a tField) then
      put ideAutocomplete_GlobalLoc(pEditorId, item 1 of the rect of pEditorId + 20, \
            item 4 of the rect of pEditorId - 30) into rX
      put item 2 of rX into rY
      put item 1 of rX into rX
      exit ideAutocomplete_GetCaretScreenPos
   end if
   
   if the effective textHeight of tField >= 1 then
      put the effective textHeight of tField into rLineHeight
   end if
   
   -- formattedRect gives the card position of the first character of the
   -- word, which stays right with wrapped lines, folded code and scrolling.
   local tCardX, tCardY, tTokenRect
   if sSavedTokenStart > 0 then
      put the formattedRect of char sSavedTokenStart of tField into tTokenRect
   end if
   if tTokenRect is not empty then
      put item 1 of tTokenRect into tCardX
      put item 4 of tTokenRect into tCardY
   else
      -- Fall back to working the line position out from the line number
      local tLineNum, tMarginTop
      put max(1, word 2 of the selectedLine of tField) into tLineNum
      put item 1 of the margins of tField into tMarginTop
      if the number of items of the margins of tField = 4 then
         put item 2 of the margins of tField into tMarginTop
      end if
      put the left of tField + 4 into tCardX
      put the top of tField + tMarginTop - 4 + tLineNum * rLineHeight - the vScroll of tField into tCardY
   end if
   
   -- Card to screen coordinates.  globalLoc knows about the window's position,
   -- which screen it is on, its menu bar and its scroll, on every platform.
   local tLoc
   put ideAutocomplete_GlobalLoc(tField, tCardX, tCardY) into tLoc
   put item 1 of tLoc into rX
   put item 2 of tLoc into rY
end ideAutocomplete_GetCaretScreenPos

-- Converts a point on the card of pObject's stack to screen coordinates.
private function ideAutocomplete_GlobalLoc pObject, pX, pY
   local tStack, tOldDefault, tLoc
   put ideAutocomplete_StackOf(pObject) into tStack
   put the defaultStack into tOldDefault
   try
      set the defaultStack to tStack
      put globalLoc(pX & comma & pY) into tLoc
   catch tError
      put empty into tLoc
   end try
   set the defaultStack to tOldDefault
   if tLoc is empty then
      put (the left of tStack + pX), (the top of tStack + pY) into tLoc
   end if
   return tLoc
end ideAutocomplete_GlobalLoc

-- Returns a reference to the stack that owns pObject.
private function ideAutocomplete_StackOf pObject
   local tLongId
   put the long id of pObject into tLongId
   return char (offset(" of stack ", tLongId) + 4) to -1 of tLongId
end ideAutocomplete_StackOf

-- Returns the rect of the screen the popup belongs on: the one containing the
-- caret, or failing that the one showing most of the script editor window.
-- (Never simply the primary screen: on a multi-monitor setup that sends the
-- popup to a different display from the editor.)
private function ideAutocomplete_ScreenRectFor pEditorId, pX, pY
   local tScreens, tLine
   put the screenRects into tScreens
   repeat for each line tLine in tScreens
      if pX >= item 1 of tLine and pX < item 3 of tLine and \
            pY >= item 2 of tLine and pY < item 4 of tLine then
         return tLine
      end if
   end repeat
   
   local tWindowRect, tBest, tBestArea, tWidth, tHeight
   put the rect of ideAutocomplete_StackOf(pEditorId) into tWindowRect
   put 0 into tBestArea
   repeat for each line tLine in tScreens
      put min(item 3 of tLine, item 3 of tWindowRect) - max(item 1 of tLine, item 1 of tWindowRect) into tWidth
      put min(item 4 of tLine, item 4 of tWindowRect) - max(item 2 of tLine, item 2 of tWindowRect) into tHeight
      if tWidth > 0 and tHeight > 0 and tWidth * tHeight > tBestArea then
         put tWidth * tHeight into tBestArea
         put tLine into tBest
      end if
   end repeat
   if tBest is empty then
      put the screenRect into tBest
   end if
   return tBest
end ideAutocomplete_ScreenRectFor

--------------------------------------------------------------------------------
-- Private: accept completion
--------------------------------------------------------------------------------

private command ideAutocomplete_AcceptCompletion pEditorId
   local tCompletion
   put line sSelectedLine of the text of field "completions" of stack kPopupName into tCompletion

   if tCompletion is empty then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_AcceptCompletion
   end if

   -- Use the caret position snapshotted when the popup was shown.
   -- Re-querying here would fail if a popup click moved OS focus off the editor.
   if sSavedToken is empty then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_AcceptCompletion
   end if

   -- Delegate to the behavior wrapper so textReplace runs in the behavior's
   -- own call context rather than dispatched from this back-script library.
   dispatch "ideAutocompleteInsertCompletion" to pEditorId with sSavedTokenStart, sSavedToken, tCompletion

   -- Inserting the word asks for a fresh update like any other edit; drop it,
   -- or the popup would come straight back for the word just completed.
   cancel sUpdateTimer
   put false into sUpdatePending

   ideAutocomplete_HidePopup
end ideAutocomplete_AcceptCompletion


--------------------------------------------------------------------------------
-- Popup click handler (dispatched from the popup field's script)
--------------------------------------------------------------------------------

on ideAutocomplete_PopupClicked
   -- the clickLine returns "line N" (a chunk expression), not a bare integer.
   -- Extract the number so sSelectedLine stays consistently an integer,
   -- matching what Navigate and AcceptCompletion expect.
   put word 2 of the clickLine into sSelectedLine
   ideAutocomplete_AcceptCompletion sCurrentEditorId
end ideAutocomplete_PopupClicked
