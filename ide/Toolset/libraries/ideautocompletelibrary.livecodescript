script "ideautocompletelibrary"

-- Word list cache: ruggedId -> return-separated sorted word list
local sWordListCache

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
   ideAutocomplete_BuildWordList pObjectId
end ideAutocompleteIntrospectMessagePath

-- Called when an object's script editor tab is closed.
command ideAutocompleteClearObjectCache pObject
   delete variable sWordListCache[revRuggedId(pObject)]
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
end ideAutocompleteUpdate

-- Called on mouseMove, closeField, exitField, escapeKey, returnInField.
command ideAutocompleteCancelFind pEditorId
   cancel sUpdateTimer
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

private command ideAutocomplete_BuildWordList pObjectId
   local tRuggedId
   put revRuggedId(pObjectId) into tRuggedId

   -- Already cached for this object
   if sWordListCache[tRuggedId] is not empty then
      exit ideAutocomplete_BuildWordList
   end if

   local tWords

   -- Built-in commands, functions and properties
   put commandNames() into tWords
   put return & functionNames() after tWords
   put return & propertyNames() after tWords

   -- Handler names from the object's full message path
   if there is a pObjectId then
      local tHandlers
      put the effective revAvailableHandlers of pObjectId into tHandlers
      local tLine
      repeat for each line tLine in tHandlers
         local tName
         put word 2 of tLine into tName
         if tName is not empty then
            put return & tName after tWords
         end if
      end repeat
   end if

   -- Sort and deduplicate
   sort lines of tWords ascending text
   put ideAutocomplete_Deduplicate(tWords) into tWords

   put tWords into sWordListCache[tRuggedId]
end ideAutocomplete_BuildWordList

private function ideAutocomplete_Deduplicate pList
   local tResult, tPrev
   repeat for each line tLine in pList
      if tLine is not empty and tLine is not tPrev then
         put tLine & return after tResult
         put tLine into tPrev
      end if
   end repeat
   -- Remove trailing return
   if tResult ends with return then
      delete the last char of tResult
   end if
   return tResult
end ideAutocomplete_Deduplicate

--------------------------------------------------------------------------------
-- Private: debounced update
--------------------------------------------------------------------------------

-- Not private: timer-fired sends cannot reach private handlers.
command ideAutocomplete_DoUpdate
   -- Read from the mailbox set by ideAutocompleteUpdate
   local pEditorId, pObjectId
   put sPendingEditorId into pEditorId
   put sPendingObjectId into pObjectId

   -- Get the partial word at the cursor
   dispatch "getCaretToken" to pEditorId
   local tToken
   put the result into tToken

   if the number of chars of tToken < kMinChars then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if

   -- Ensure the word list exists for this object
   ideAutocomplete_BuildWordList pObjectId

   local tRuggedId
   put revRuggedId(pObjectId) into tRuggedId

   -- Filter: only words that begin with the typed token (case-insensitive)
   local tMatches
   repeat for each line tLine in sWordListCache[tRuggedId]
      if tLine begins with tToken then
         put tLine & return after tMatches
         if the number of lines of tMatches > kMaxCompletions then
            exit repeat
         end if
      end if
   end repeat
   if tMatches ends with return then
      delete the last char of tMatches
   end if

   -- Nothing to show, or the only match is exactly the token already typed
   if tMatches is empty or tMatches is tToken then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if

   -- Snapshot caret position now, while the editor field has focus.
   -- AcceptCompletion reads these saved values so a popup click (which may
   -- shift OS focus away from the editor) doesn't corrupt the offset.
   dispatch function "getScriptField" to pEditorId
   local tSnapField, tSnapCursor
   put the result into tSnapField
   if tSnapField is not empty and there is a tSnapField then
      put word 2 of the selectedChunk of tSnapField into tSnapCursor
      put tSnapCursor - the number of chars of tToken into sSavedTokenStart
   else
      put 0 into sSavedTokenStart
   end if
   put tToken into sSavedToken

   ideAutocomplete_ShowPopup pEditorId, tMatches
end ideAutocomplete_DoUpdate

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
   try
      set the hilitedLine of field "completions" of stack kPopupName to 1
   catch tErr
   end try

   -- Calculate screen position below the current caret line
   local tX, tY
   ideAutocomplete_GetCaretScreenPos pEditorId, tX, tY

   -- Keep the popup on screen
   local tScreenRect
   put the screenRect into tScreenRect
   if tX + kPopupWidth > item 3 of tScreenRect then
      put item 3 of tScreenRect - kPopupWidth into tX
   end if
   if tY + tPopupHeight > item 4 of tScreenRect then
      -- Flip above the current line
      subtract tPopupHeight + kItemHeight from tY
   end if

   set the topleft of stack kPopupName to tX, tY

   -- Suppress cancelFind while show stack + refocus fire focus-change events.
   -- On Windows, palette windows still activate when shown and generate
   -- additional async focus events, so we keep the flag set longer there.
   put true into sSuppressCancel
   if not sPopupShowing then
      show stack kPopupName
      put true into sPopupShowing
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

private command ideAutocomplete_GetCaretScreenPos pEditorId, @rX, @rY
   dispatch function "getScriptField" to pEditorId
   local tField
   put the result into tField

   if tField is empty or not (there is a tField) then
      put item 1 of the rect of pEditorId + 20 into rX
      put item 4 of the rect of pEditorId - 30 into rY
      exit ideAutocomplete_GetCaretScreenPos
   end if

   local tLineNum
   put word 2 of the selectedLine of tField into tLineNum
   if tLineNum < 1 then
      put 1 into tLineNum
   end if

   local tFieldRect
   put the rect of tField into tFieldRect

   local tVScroll
   put the vScroll of tField into tVScroll

   -- Use the same constant-multiplier method as lineNumberToVerticalLoc in the
   -- behavior: formattedHeight requires LiveCode to have measured every preceding
   -- line, which is unreliable for long scripts. effectiveTextHeight is constant.
   local tLineHeight
   put the effective textHeight of tField into tLineHeight
   if tLineHeight < 1 then
      put kItemHeight into tLineHeight
   end if

   -- Top of the current line relative to the field's content area
   local tAboveHeight
   put (tLineNum - 1) * tLineHeight into tAboveHeight

   -- Field top-margin offset (mirrors the kFudge = -4 the behavior uses)
   local tMarginTop
   if the number of items of the margins of tField = 4 then
      put item 2 of the margins of tField into tMarginTop
   else
      put the margins of tField into tMarginTop
   end if
   subtract 4 from tMarginTop

   -- Convert card-space coordinates to screen coordinates.
   -- Extract the enclosing stack reference from the field's long id
   -- (everything from " of stack " to the end of the string) and
   -- use its rect as the window's screen origin.  Fall back to raw
   -- card coordinates (origin 0,0) if the lookup fails for any reason.
   local tWinX, tWinY
   put 0 into tWinX
   put 0 into tWinY
   local tLongId, tStackPos
   put the long id of tField into tLongId
   put offset(" of stack ", tLongId) into tStackPos
   if tStackPos > 0 then
      local tStackRef
      put char (tStackPos + 4) to -1 of tLongId into tStackRef
      try
         local tWRect
         put the rect of tStackRef into tWRect
         put item 1 of tWRect into tWinX
         put item 2 of tWRect into tWinY
      catch tErr
         -- leave tWinX/tWinY as 0 — card coords become the fallback
      end try
   end if

   -- X: align with the start of the token being typed.
   -- formattedRect of char N returns card-space coordinates for that character,
   -- so we get the exact horizontal position without any font-measurement guesswork.
   local tTokenRect
   if sSavedTokenStart > 0 then
      put the formattedRect of char sSavedTokenStart of tField into tTokenRect
   end if
   if tTokenRect is not empty then
      put tWinX + item 1 of tTokenRect into rX
   else
      put tWinX + item 1 of tFieldRect + 4 into rX
   end if

   put tWinY + item 2 of tFieldRect + tMarginTop + tAboveHeight + tLineHeight - tVScroll into rY
end ideAutocomplete_GetCaretScreenPos

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
