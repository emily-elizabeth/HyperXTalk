script "ideautocompletelibrary"

-- Language words (commands, functions, properties, messages, keywords...),
-- one per line.  Taken from the dictionary where possible so the names keep
-- their camelCase, which is what the "hump" matching works on.
-- They are kept by where they can be used: sBuiltinWords["command"] for the
-- start of a statement, ["value"] (functions, properties, constants) for
-- anywhere after it, ["message"] for after on, before or after, and
-- ["function"] and ["property"] on their own for after "the".
local sBuiltinWords
-- Keywords, operators and object types (into, of, the, is, me, field ...) and
-- commands of three letters or fewer (put, set, get), keyed by lower-case
-- word.  They are typed constantly and are quicker to type than to pick, so
-- they are never offered, and typing one in full puts the popup away.
local sQuietWordsA
local sBuiltinFromDocs
local sBuiltinBuiltAt
local sBuiltinBuilds

-- Message path handler names: ruggedId -> ["command"] and ["value"], each a
-- return-separated list (see ideAutocomplete_KindOfHandler).  Refreshed
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

-- Parameter hint: a one-off line above the caret showing the parameters of
-- the handler being called.  sPathHandlerInfo is ruggedId -> lower-case
-- handler name -> type, start line and owner (tab-separated), so a handler's
-- declaration can be read from its owner's script when a hint is wanted.
-- sSyntaxCache holds dictionary syntax already looked up.
local sHintShowing
-- What the hint on show belongs to, so it can be kept in step with the caret
-- (see ideAutocomplete_WatchHint)
local sHintEditorId
local sHintObjectId
local sHintField
local sHintCaret
local sHintScroll
local sHintWatchTimer
local sHintFocusMisses
local sPathHandlerInfo
local sSyntaxCache

constant kPopupName = "ideAutoCompletePopup"
-- Named so that the IDE's "*ideAutocompletePopup*" filters cover it too
constant kHintName = "ideAutoCompletePopupHint"
constant kMaxHintLines = 4
-- How often (ms) a showing hint checks that the caret is still in its call
constant kHintWatchInterval = 250
constant kMinChars = 2
-- How many times the word lists are built while waiting for the dictionary
constant kMaxWordListBuilds = 4
constant kMaxCompletions = 50
constant kMaxVisible = 10
constant kPopupWidth = 220
constant kItemHeight = 18
-- How long (ms) to wait before hiding the popup after a cancel event.
-- Increase this if the popup disappears before you can click an item.
constant kHideDelay = 400
-- Where the list and the hint wait, out of sight, on Windows
constant kParkedLoc = "-20000,-20000"

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
   cancel sHintWatchTimer
   -- Hide rather than delete: explicitly destroying a GTK window during engine
   -- shutdown causes a segfault on Linux.  The engine will clean up all stacks
   -- when it exits, so hiding is sufficient here.
   if there is a stack kPopupName then
      hide stack kPopupName
   end if
   if there is a stack kHintName then
      hide stack kHintName
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
   delete variable sPathHandlerInfo[revRuggedId(pObject)]
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
      ideAutocomplete_HideHint
      exit ideAutocompleteCancelFind
   end if
   -- Suppressed while show stack + focus-return are in flight to prevent the
   -- focus-change events those operations fire from immediately closing the popup.
   if sSuppressCancel then
      exit ideAutocompleteCancelFind
   end if
   -- (A parameter hint is not put away here: it stays until the caret leaves
   -- the call it describes, which ideAutocomplete_WatchHint looks after.)
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
   -- Up or down leaves the line a parameter hint belongs to (unless the keys
   -- are moving through the completion list).  Left and right stay on it:
   -- ideAutocomplete_WatchHint then updates the hint for the new position.
   if sHintShowing is true and not sPopupShowing and \
         pKeyName is among the items of "up,down" then
      ideAutocomplete_HideHint
   end if
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
   -- The dictionary may not be ready early in startup: retry now and then,
   -- but not for ever.  Some builds have no dictionary at all, and building
   -- the lists again every few seconds makes for a pause as you type.
   if sBuiltinWords is an array and \
         (sBuiltinBuilds >= kMaxWordListBuilds or the seconds - sBuiltinBuiltAt < 10) then
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
   -- Sort the dictionary entries by where they can be used.
   local tListsA, tQuietA, tStructures, tLine, tName, tDocCount
   put 0 into tDocCount
   set the itemDelimiter to tab
   repeat for each line tLine in tDocNames
      put item 1 of tLine into tName
      if not matchText(tName, "^[A-Za-z_][A-Za-z0-9_]*$") then next repeat
      add 1 to tDocCount
      switch item 2 of tLine
         case "command"
            put tName & return after tListsA["command"]
            break
         case "control structure"
            put tName & return after tStructures
            break
         case "function"
         case "property"
            put tName & return after tListsA[item 2 of tLine]
            -- no break: these are values as well
         case "constant"
            put tName & return after tListsA["value"]
            break
         case "message"
            put tName & return after tListsA["message"]
            break
         default
            -- keyword, operator, object
            put true into tQuietA[toLower(tName)]
      end switch
   end repeat
   
   -- The engine's own lists fill in anything the dictionary lacks
   put commandNames() after tListsA["command"]
   put functionNames() & return & propertyNames() after tListsA["value"]
   put functionNames() after tListsA["function"]
   put propertyNames() after tListsA["property"]
   put ideAutocomplete_StandardMessages() after tListsA["message"]
   
   local tKind, tKeywordsA
   put tQuietA into tKeywordsA
   repeat for each item tKind in "command" & tab & "value" & tab & "message" & tab & "function" & tab & "property"
      filter lines of tListsA[tKind] with regex pattern "^[A-Za-z_][A-Za-z0-9_]*$"
      put ideAutocomplete_MergeWords(tListsA[tKind], empty) into tListsA[tKind]
      -- A keyword that is also a command, function or property stays on offer
      repeat for each line tLine in tListsA[tKind]
         delete variable tQuietA[toLower(tLine)]
      end repeat
   end repeat
   
   -- Control structures (repeat, switch, try) start a statement like a
   -- command.  Those that are keywords as well (after, function, on) are quiet.
   repeat for each line tLine in tStructures
      if not tQuietA[toLower(tLine)] then
         put return & tLine after tListsA["command"]
      end if
   end repeat
   put ideAutocomplete_MergeWords(tListsA["command"], empty) into tListsA["command"]
   
   -- Words of three letters or fewer are quicker to type than to pick from a
   -- list, so none is offered.  Short commands (put, set, get) and short
   -- keywords (me, it) are quiet as well; other short properties and functions
   -- are not, or "vis" and "sec" would put the popup away on the way to
   -- "visible" and "seconds".
   local tKept
   repeat for each key tKind in tListsA
      put empty into tKept
      repeat for each line tLine in tListsA[tKind]
         if the number of chars of tLine <= 3 then
            if tKind is "command" or tKeywordsA[toLower(tLine)] then
               put true into tQuietA[toLower(tLine)]
            end if
         else
            put tLine & return after tKept
         end if
      end repeat
      delete the last char of tKept
      put tKept into tListsA[tKind]
   end repeat
   
   put tListsA into sBuiltinWords
   put tQuietA into sQuietWordsA
   put (tDocCount > 1000) into sBuiltinFromDocs
   put the seconds into sBuiltinBuiltAt
   add 1 to sBuiltinBuilds
end ideAutocomplete_EnsureBuiltinWords

-- Returns the entries in a dictionary database that can appear in script, one
-- per line as name, tab, type.
-- The messages the engine sends, for when the dictionary cannot be read (a
-- development build without the documentation database, for one): the
-- engine can list its commands, functions and properties but not these.
private function ideAutocomplete_StandardMessages
   local tNames
   put "appleEvent,arrowKey,backspaceKey,cancelButtonClicked,closeBackground,closeCard," & \
         "closeControl,closeField,closeStack,closeStackRequest,commandKeyDown," & \
         "controlKeyDown,copyKey,currentTimeChanged,cutKey,deleteAudioclip," & \
         "deleteBackground,deleteButton,deleteCard,deleteEPS,deleteField,deleteGraphic," & \
         "deleteGroup,deleteImage,deleteKey,deletePlayer,deleteScrollbar,deleteStack," & \
         "deleteVideoclip,deleteWidget,desktopChanged,dragDrop,dragEnd,dragEnter," & \
         "dragLeave,dragMove,dragStart,editScript,enterInField,enterKey,errorDialog," & \
         "escapeKey,exitField,fieldValidationFailed,focusIn,focusOut,functionKey,help," & \
         "iconifyStack,IDChanged,idle,keyDown,keyUp,libraryStack,linkClicked," & \
         "listenTimeoutExpired,mainStackChanged,menuPick,mouseDoubleDown,mouseDoubleUp," & \
         "mouseDown,mouseDownInBackdrop,mouseEnter,mouseLeave,mouseMove,mouseRelease," & \
         "mouseStillDown,mouseUp,mouseUpInBackdrop,mouseWithin,moveControl,moveStack," & \
         "moveStopped,msgChanged,nameChanged,newAudioclip,newBackground,newButton,newCard," & \
         "newEPS,newField,newGraphic,newGroup,newImage,newPlayer,newScrollbar,newStack," & \
         "newTool,newVideoclip,newWidget,notificationClicked,notificationPermissionDenied," & \
         "notificationPermissionGranted,objectSelectionEnded,objectSelectionStarted," & \
         "openBackground,openCard,openControl,openField,openStack,optionKeyDown," & \
         "passwordToggleClicked,pasteKey,playPaused,playStarted,playStopped," & \
         "preOpenBackground,preOpenCard,preOpenControl,preOpenStack,queryRecordChanged," & \
         "rawKeyDown,rawKeyUp,relaunch,releaseStack,reloadStack,resizeControl,resizeStack," & \
         "resume,resumeStack,returnInField,returnKey,revEndXMLNode,revStartXMLData," & \
         "revStartXMLNode,revXMLEndTree,revXMLStartTree,saveStackRequest,savingStandalone," & \
         "scriptExecutionError,scrollbarBeginning,scrollbarDrag,scrollbarEnd," & \
         "scrollbarLineDec,scrollbarLineInc,scrollbarPageDec,scrollbarPageInc," & \
         "selectedObjectChanged,selectionChanged,shutdown,shutdownRequest,signal," & \
         "socketClosed,socketError,socketTimeout,standaloneSaved,startup,statusIconClick," & \
         "statusIconDoubleClick,statusIconMenuOpening,statusIconMenuPick,suspend," & \
         "suspendStack,systemAppearanceChanged,tabKey,textChanged,themeChanged," & \
         "toolbarHidden,toolbarItemClicked,toolbarShown,undoChanged,undoKey," & \
         "unIconifyStack,unrecognizedVoiceCommand,voiceCommand,wakeWordDetected" into tNames
   replace comma with return in tNames
   return tNames
end ideAutocomplete_StandardMessages

private function ideAutocomplete_DocNames pConnection
   if pConnection is not a number then
      return empty
   end if
   local tNames
   put revDataFromQuery(tab, return, pConnection, \
         "SELECT DISTINCT entry_name, entry_type FROM dictionary_data WHERE library_name NOT LIKE '%Builder' " & \
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
      local tOwner, tSkip, tCurrentOwner, tInfoA, tKey
      put false into tSkip
      repeat for each line tLine in tHandlers
         put word 5 to -1 of tLine into tOwner
         if tOwner is not empty then
            put tOwner into tCurrentOwner
            try
               put revIDEObjectIsOnIDEStack(tOwner) into tSkip
            catch tError
               put false into tSkip
            end try
         end if
         if tSkip or char 1 of word 1 of tLine is "P" then next repeat
         if word 2 of tLine is not empty then
            -- Remember where the nearest handler of this name is declared
            put toLower(word 2 of tLine) into tKey
            if tInfoA[tKey] is empty then
               put word 1 of tLine & tab & word 3 of tLine & tab & tCurrentOwner into tInfoA[tKey]
            end if
            -- F function, G getProp, S setProp; anything else is called
            -- like a command (M message, C command, B before, A after)
            if char 1 of word 1 of tLine is among the items of "F,G,S" then
               put word 2 of tLine & return after tNames["value"]
            else
               put word 2 of tLine & return after tNames["command"]
            end if
         end if
      end repeat
   end if
   put tNames into sPathHandlers[revRuggedId(pObjectId)]
   put tInfoA into sPathHandlerInfo[revRuggedId(pObjectId)]
end ideAutocomplete_RefreshPathHandlers

-- Handler names declared in the text currently in the editor, applied or not,
-- as an array: ["command"] for on / command handlers and ["value"] for
-- function / getProp / setProp ones.
-- This is what makes a handler completable from elsewhere in its own script.
private function ideAutocomplete_ScriptHandlers pField
   local tScript, tNames, tName, tType
   put the text of pField into tScript
   -- A line of a block comment can begin "on the ..." or "after any ...":
   -- take those out first, or "the" and "any" turn up as handlers.
   if tScript contains "/*" then
      put replaceText(tScript, "(?s)/\*.*?\*/", empty) into tScript
   end if
   filter lines of tScript with regex pattern \
         "(?i)^\s*(private\s+)?(on|command|function|getprop|setprop|before|after)\s+[A-Za-z_]"
   repeat for each line tLine in tScript
      if word 1 of tLine is "private" then
         put word 2 of tLine into tType
         put word 3 of tLine into tName
      else
         put word 1 of tLine into tType
         put word 2 of tLine into tName
      end if
      -- (a keyword is no handler's name: this is a continued line such as
      -- "after the last item", not a declaration)
      if matchText(tName, "^([A-Za-z_][A-Za-z0-9_]*)", tName) and not sQuietWordsA[toLower(tName)] then
         if tType is among the items of "function,getprop,setprop" then
            put tName & return after tNames["value"]
         else
            put tName & return after tNames["command"]
         end if
      end if
   end repeat
   return tNames
end ideAutocomplete_ScriptHandlers

-- Splits pScript, the script up to the caret, into lines (rLinesA, 1 to
-- rCount, without the unfinished line the caret is on) and returns the number
-- of the line declaring the handler the caret is in, or 0 if it is in none.
private function ideAutocomplete_HandlerStart pScript, @rLinesA, @rCount
   put "x" after pScript
   delete the last line of pScript
   put pScript into rLinesA
   split rLinesA by return
   put the number of lines of pScript into rCount
   
   local tIndex, tWord
   repeat with tIndex = rCount down to 1
      put word 1 of rLinesA[tIndex] into tWord
      if tWord is "private" then
         put word 2 of rLinesA[tIndex] into tWord
      end if
      if tWord is among the items of "on,command,function,getProp,setProp,before,after" then
         return tIndex
      end if
      if tWord is "end" and word 2 of rLinesA[tIndex] is not among the items of "if,repeat,switch,try" then
         -- the end of an earlier handler: the caret is between handlers
         return 0
      end if
   end repeat
   return 0
end ideAutocomplete_HandlerStart

-- Returns the name declared by a handler's first line
private function ideAutocomplete_HandlerName pLine
   local tName
   if word 1 of pLine is "private" then
      put word 3 of pLine into tName
   else
      put word 2 of pLine into tName
   end if
   if matchText(tName, "^([A-Za-z_][A-Za-z0-9_]*)", tName) then
      return tName
   end if
   return empty
end ideAutocomplete_HandlerName

-- Returns the blocks (if, repeat, switch, try) still open at the caret, given
-- the script up to it: one per line, innermost last.  rHandler is given the
-- name of the handler the caret is in, or empty if it is in none.
private function ideAutocomplete_OpenBlocks pScript, @rHandler
   local tLinesA, tCount, tStart
   put empty into rHandler
   put ideAutocomplete_HandlerStart(pScript, tLinesA, tCount) into tStart
   if tStart is 0 then
      return empty
   end if
   put ideAutocomplete_HandlerName(tLinesA[tStart]) into rHandler
   
   -- Follow the blocks opened and closed between the handler's first line
   -- and the caret.  tOpen is the list of blocks open, one per line with the
   -- innermost last.
   local tOpen, tIndex, tLine, tFirst, tDepth, tOpens
   repeat with tIndex = tStart + 1 to tCount
      put tLinesA[tIndex] into tLine
      put word 1 of tLine into tFirst
      put empty into tOpens
      switch tFirst
         case "repeat"
         case "switch"
         case "try"
            put toLower(tFirst) into tOpens
            break
         case "if"
            -- only an "if" whose line ends in "then" opens a block
            if the last word of tLine is "then" then
               put "if" into tOpens
            end if
            break
         case "else"
            if the number of words of tLine is 1 or \
                  (word 2 of tLine is "if" and the last word of tLine is "then") then
               -- "else" on its own continues a block; after a one-line
               -- "if x then y" it starts one
               if line -1 of tOpen is not "if" then
                  put "if" into tOpens
               end if
            else if line -1 of tOpen is "if" then
               -- "else doThis" finishes the if without an "end if"
               put line 1 to -2 of tOpen into tOpen
            end if
            break
         case "end"
            if word 2 of tLine is among the items of "if,repeat,switch,try" then
               -- close the nearest block of that kind, and any left open in it
               put the number of lines of tOpen into tDepth
               repeat while tDepth > 0 and line tDepth of tOpen is not word 2 of tLine
                  subtract 1 from tDepth
               end repeat
               if tDepth > 0 then
                  put line 1 to (tDepth - 1) of tOpen into tOpen
               end if
            end if
            break
      end switch
      if tOpens is not empty then
         if tOpen is empty then
            put tOpens into tOpen
         else
            put return & tOpens after tOpen
         end if
      end if
   end repeat
   
   return tOpen
end ideAutocomplete_OpenBlocks

-- Returns what can follow end, exit, pass or next (pKeyword) at the caret,
-- given the script up to it, one per line:
--   end    the innermost block still open (if, repeat, switch, try), or the
--          handler's name once every block is closed
--   exit   "repeat" inside a loop, and the handler's name
--   next   "repeat" inside a loop
--   pass   the handler's name
private function ideAutocomplete_BlockWords pKeyword, pScript
   local tOpen, tName
   put ideAutocomplete_OpenBlocks(pScript, tName) into tOpen
   if tName is empty then
      return empty
   end if
   switch pKeyword
      case "end"
         if tOpen is not empty then
            return line -1 of tOpen
         end if
         return tName
      case "exit"
         if "repeat" is among the lines of tOpen then
            return "repeat" & return & tName
         end if
         return tName
      case "next"
         if "repeat" is among the lines of tOpen then
            return "repeat"
         end if
         return empty
      case "pass"
         return tName
   end switch
   return empty
end ideAutocomplete_BlockWords

-- Returns the variables that can be used at the caret, given the script up
-- to it, one per line: the handler's parameters and the variables it has
-- declared or put something into so far, then the locals, globals and
-- constants declared at the top level of the script.
private function ideAutocomplete_Variables pScript
   local tLinesA, tCount, tStart, tNames, tName, tLine, tFirst, tItem, tIndex
   put ideAutocomplete_HandlerStart(pScript, tLinesA, tCount) into tStart
   
   if tStart > 0 then
      -- parameters
      local tParams
      if matchText(tLinesA[tStart], "(?i)^\s*(?:private\s+)?[a-z]+\s+[A-Za-z_][A-Za-z0-9_]*(.*)$", tParams) then
         replace "(" with space in tParams
         replace ")" with space in tParams
         replace "@" with empty in tParams
         repeat for each item tItem in tParams
            if matchText(tItem, "^\s*([A-Za-z_][A-Za-z0-9_]*)", tName) then
               put tName & return after tNames
            end if
         end repeat
      end if
      
      -- declared, or simply used as somewhere to put something
      repeat with tIndex = tStart + 1 to tCount
         put tLinesA[tIndex] into tLine
         put word 1 of tLine into tFirst
         if tFirst is among the items of "local,global,constant" then
            repeat for each item tItem in word 2 to -1 of tLine
               if matchText(tItem, "^\s*([A-Za-z_][A-Za-z0-9_]*)", tName) then
                  put tName & return after tNames
               end if
            end repeat
         else if tFirst is "repeat" then
            if matchText(tLine, "(?i)^\s*repeat\s+with\s+([A-Za-z_][A-Za-z0-9_]*)\s*=", tName) or \
                  matchText(tLine, "(?i)^\s*repeat\s+for\s+each\s+[a-z]+\s+([A-Za-z_][A-Za-z0-9_]*)\s+in\s", tName) then
               put tName & return after tNames
            end if
         else if matchText(tLine, "(?i)\s(?:into|after|before)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[.*)?$", tName) then
            -- (not "into it", "into msg", "into field ...": keywords are quiet)
            if not sQuietWordsA[toLower(tName)] then
               put tName & return after tNames
            end if
         end if
      end repeat
   end if
   
   -- Top-level declarations start in the first column; a handler's own are
   -- indented.
   filter lines of pScript with regex pattern "(?i)^(local|global|constant)\s"
   repeat for each line tLine in pScript
      repeat for each item tItem in word 2 to -1 of tLine
         if matchText(tItem, "^\s*([A-Za-z_][A-Za-z0-9_]*)", tName) then
            put tName & return after tNames
         end if
      end repeat
   end repeat
   
   return ideAutocomplete_MergeWords(tNames, empty)
end ideAutocomplete_Variables

-- Returns true if the word being typed is inside a comment.  pBefore is the
-- line up to the word, pPreceding a stretch of script up to the same point.
private function ideAutocomplete_InComment pBefore, pPreceding
   -- Line comments: --, # or // outside a string literal
   local tCode, tIndex
   set the itemDelimiter to quote
   repeat for each item tPart in pBefore
      add 1 to tIndex
      if tIndex mod 2 is 1 then
         put tPart & space after tCode
      end if
   end repeat
   if tCode contains "--" or tCode contains "#" or tCode contains "//" then
      return true
   end if
   
   -- Block comments: a /* with no */ after it
   if pPreceding contains "/*" then
      replace "/*" with numToChar(1) in pPreceding
      set the itemDelimiter to numToChar(1)
      if not (the last item of pPreceding contains "*/") then
         return true
      end if
   end if
   return false
end ideAutocomplete_InComment

-- Works out what kind of word belongs after pBefore, the text of the line up
-- to the word being typed:
--   "command"   at the start of a statement (line start, after then, else or ;)
--   "message"   after on, before or after
--   "handler"   after end, exit, pass or next starting a statement
--   "property"  after "set the"
--   "the"       after "the" anywhere else: a property or a built-in function
--   "type"      after "is a", "is an", "is not a", "is not an"
--   "value"     anywhere else: functions, properties, constants
private function ideAutocomplete_Context pBefore
   local tCount
   put the number of words of pBefore into tCount
   if tCount is 0 then
      return "command"
   end if
   if tCount is 1 then
      if word 1 of pBefore is among the items of "on,before,after" then
         return "message"
      end if
   end if
   
   -- end, exit, pass and next may also follow then, else or ; on the same
   -- line ("if x = 9 then exit repeat"): look at the statement, not the line
   local tStatement, tSeparator, tAt, tSkip, tFrom
   put 1 into tFrom
   repeat for each item tSeparator in " then , else ,;"
      put 0 into tSkip
      repeat
         put offset(tSeparator, pBefore, tSkip) into tAt
         if tAt is 0 then exit repeat
         add tAt to tSkip
         put max(tFrom, tSkip + the number of chars of tSeparator) into tFrom
      end repeat
   end repeat
   put char tFrom to -1 of pBefore into tStatement
   if the number of words of tStatement is 1 and \
         word 1 of tStatement is among the items of "end,exit,pass,next" then
      return "handler"
   end if
   if the last word of pBefore is among the items of "then,else" or \
         the last char of word -1 of pBefore is ";" then
      return "command"
   end if
   if the last word of pBefore is "the" then
      if tCount is 2 and word 1 of pBefore is "set" then
         return "property"
      end if
      return "the"
   end if
   if the last word of pBefore is among the items of "a,an" and tCount >= 3 then
      if word -2 of pBefore is "is" or word -3 to -2 of pBefore is "is not" then
         return "type"
      end if
   end if
   return "value"
end ideAutocomplete_Context

-- Whether abbreviations may complete long names: the "Match abbreviations"
-- preference of the script editor, on unless it has been switched off.
private function ideAutocomplete_HumpsEnabled
   local tPropName, tValue
   put "cScriptEditor,editor,autocompletehumps" into tPropName
   put the tPropName of stack "revPreferences" into tValue
   return tValue is not false
end ideAutocomplete_HumpsEnabled

-- Returns pWords without any that are quiet (keywords, operators ...)
private function ideAutocomplete_WithoutQuietWords pWords
   local tKept, tLine
   repeat for each line tLine in pWords
      if tLine is not empty and not sQuietWordsA[toLower(tLine)] then
         put tLine & return after tKept
      end if
   end repeat
   delete the last char of tKept
   return tKept
end ideAutocomplete_WithoutQuietWords

-- Returns the regex a word must match to complete pToken.  The first letter
-- must start the word; each later letter either follows on directly or starts
-- a later "hump" (a capital, or a letter after an underscore or space).  So "poc" and
-- "prOpCa" both find preOpenCard, as well as everything a plain prefix would.
private function ideAutocomplete_MatchPattern pToken, pHumpsFrom
   -- Hump matching starts at pHumpsFrom letters, or sooner with a capital
   -- after the first letter ("oC").  Below that only a plain prefix matches.
   -- pHumpsFrom 0 switches hump matching off altogether.
   local tHumps
   put false into tHumps
   if pHumpsFrom > 0 then
      set the caseSensitive to true
      put the number of chars of pToken >= pHumpsFrom or \
            toLower(char 2 to -1 of pToken) is not char 2 to -1 of pToken into tHumps
      set the caseSensitive to false
   end if
   
   local tPattern, tLower, tUpper
   repeat for each char tChar in pToken
      put toLower(tChar) into tLower
      put toUpper(tChar) into tUpper
      if tPattern is empty then
         put "^[" & tLower & tUpper & "]" into tPattern
      else if not tHumps then
         if matchText(tChar, "[A-Za-z]") then
            put "[" & tLower & tUpper & "]" after tPattern
         else
            put tChar after tPattern
         end if
      else if not matchText(tChar, "[A-Za-z]") then
         -- digit or underscore
         put "(?:" & tChar & "|[a-z0-9]*" & tChar & ")" after tPattern
      else
         put "(?:[" & tLower & tUpper & "]|[a-z0-9]*(?:" & tUpper & "|[_ ][" & tLower & tUpper & "]))" after tPattern
      end if
   end repeat
   return tPattern
end ideAutocomplete_MatchPattern

-- Returns the words that complete pToken, best first: an exact match, then
-- plain prefixes, then hump matches.  Within each of those the script's own
-- handlers (pHandlers) come before language words (pBuiltins), alphabetically.
private function ideAutocomplete_Matches pToken, pHandlers, pBuiltins, pHumpsFrom
   -- The pattern tells capitals from small letters, so match case-sensitively
   local tPattern
   put ideAutocomplete_MatchPattern(pToken, pHumpsFrom) into tPattern
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
      ideAutocomplete_HideHint
      exit ideAutocomplete_DoUpdate
   end if
   
   dispatch function "getScriptField" to pEditorId
   local tField
   put the result into tField
   if tField is empty or there is not a tField then
      ideAutocomplete_HidePopup
      ideAutocomplete_HideHint
      exit ideAutocomplete_DoUpdate
   end if
   
   -- The parameter hint is worked out first and on its own: it depends on the
   -- call the caret is inside, not on the word being typed.
   ideAutocomplete_UpdateHint pEditorId, pObjectId, tField
   
   -- Get the partial word at the cursor
   dispatch "getCaretToken" to pEditorId
   local tToken
   put the result into tToken
   if tToken is not empty and not matchText(tToken, "^[A-Za-z_][A-Za-z0-9_]*$") then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   
   -- Only complete when the caret sits at the end of the word being typed.
   -- With nothing typed yet, only directly after a space (see below for the
   -- few places where that is enough to offer something).
   local tCursor, tTokenStart, tBefore
   put word 2 of the selectedChunk of tField into tCursor
   if tCursor is not a number then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   put tCursor - the number of chars of tToken into tTokenStart
   if tToken is empty then
      if tCursor < 2 or the text of char (tCursor - 1) of tField is not space then
         ideAutocomplete_HidePopup
         exit ideAutocomplete_DoUpdate
      end if
   else if tTokenStart < 1 or the text of char tTokenStart to (tCursor - 1) of tField is not tToken then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   
   -- ... and not inside a string literal
   local tPreceding
   put empty into tBefore
   if tTokenStart > 1 then
      put the text of char max(1, tTokenStart - 4000) to (tTokenStart - 1) of tField into tPreceding
      -- (the "x" keeps a trailing return from being ignored, which would
      -- hand back the line above when the word starts its own line)
      put the last line of (tPreceding & "x") into tBefore
      delete the last char of tBefore
   end if
   set the itemDelimiter to quote
   if tBefore is not empty and the number of items of (tBefore & "x") mod 2 is 0 then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   set the itemDelimiter to comma
   -- ... nor inside a comment
   if ideAutocomplete_InComment(tBefore, tPreceding) then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   
   -- What can be completed depends on where in the line the word is: a
   -- command only ever starts a statement and a function never does.
   local tContext, tContainer
   put ideAutocomplete_Context(tBefore) into tContext
   -- "put x into", "put x after", "put x before": a container comes next
   put false into tContainer
   if tContext is "value" and the number of words of tBefore >= 2 then
      put the last word of tBefore is among the items of "into,after,before" into tContainer
   end if
   
   -- Normally two letters must be typed before anything is offered.  After
   -- end, exit, pass and next the choice is so narrow, and after into, after
   -- and before so relevant (the variables in reach), that the space is enough.
   if tContext is not "handler" and not tContainer and the number of chars of tToken < kMinChars then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   
   -- A keyword typed in full (into, it, me): nothing to complete.  Not after
   -- "the", though, where a property is being typed and "for" is merely the
   -- start of "foreColor".
   ideAutocomplete_EnsureBuiltinWords
   if tToken is not empty and sQuietWordsA[toLower(tToken)] and \
         tContext is not among the items of "the,property" then
      ideAutocomplete_HidePopup
      exit ideAutocomplete_DoUpdate
   end if
   
   -- Gather the candidate words.  The message path is only re-read when a
   -- new popup is about to appear, not on every character.
   local tRuggedId
   put revRuggedId(pObjectId) into tRuggedId
   if not sPopupShowing or tRuggedId is not among the keys of sPathHandlers then
      ideAutocomplete_RefreshPathHandlers pObjectId
   end if
   
   local tScriptHandlers, tHandlers, tBuiltins, tVariables
   switch tContext
      case "message"
         -- on, before, after: the name of a message
         put sBuiltinWords["message"] into tBuiltins
         break
      case "handler"
         -- end, exit, pass, next: the handler or block the caret is in
         -- (the keyword is the word just before the one being typed)
         put ideAutocomplete_BlockWords(word -1 of tBefore, the text of char 1 to (tTokenStart - 1) of tField) into tHandlers
         break
      case "property"
         -- set the ...: a property
         put sBuiltinWords["property"] into tBuiltins
         break
      case "the"
         -- the ...: a property or a built-in function, never a handler
         put sBuiltinWords["property"] & return & sBuiltinWords["function"] into tBuiltins
         break
      case "type"
         -- is a / is an: one of the types that can be tested for
         put "array,boolean,color,date,integer,number,point,rect" into tBuiltins
         replace comma with return in tBuiltins
         break
      case "value"
         -- the variables in reach come first, then function handlers
         put ideAutocomplete_Variables(the text of char 1 to (tTokenStart - 1) of tField) into tVariables
         put ideAutocomplete_ScriptHandlers(tField) into tScriptHandlers
         put tVariables & return & tScriptHandlers["value"] & return & sPathHandlers[tRuggedId]["value"] into tHandlers
         put sBuiltinWords["value"] into tBuiltins
         break
      default
         put ideAutocomplete_ScriptHandlers(tField) into tScriptHandlers
         put tScriptHandlers[tContext] & return & sPathHandlers[tRuggedId][tContext] into tHandlers
         put sBuiltinWords[tContext] into tBuiltins
         if tContext is "command" then
            -- "break" only means something inside a switch
            local tOpenBlocks, tInHandler
            put ideAutocomplete_OpenBlocks(the text of char 1 to (tTokenStart - 1) of tField, tInHandler) into tOpenBlocks
            if "switch" is not among the lines of tOpenBlocks then
               filter lines of tBuiltins without "break"
            end if
         end if
   end switch
   
   -- Whatever a script or the message path may throw up, a keyword is never
   -- a command or a function ("the", "and", "after" ...)
   if tContext is among the items of "command,value" then
      put ideAutocomplete_WithoutQuietWords(tHandlers) into tHandlers
   end if
   
   -- "put x after msg" and "put x before msg" are everyday debugging lines, so
   -- after those two words the message box is offered.  ("after" or "before"
   -- as the first word of a line declares a handler instead.)
   local tOfferMessageBox
   put false into tOfferMessageBox
   if tContainer and the last word of tBefore is not "into" then
      put true into tOfferMessageBox
      put return & "message box" after tBuiltins
   end if
   
   -- Two letters are enough for a hump match where the choice is small: "oc"
   -- after "on" finds openCard, "sh" at the start of a line finds sayHello.
   -- Mid-line there are some 1500 functions and properties to choose from,
   -- and two letters would bring up a list for nearly every word typed, so
   -- there it takes three ("spf" finds specialFolderPath).
   local tHumpsFrom
   set the itemDelimiter to comma
   if not ideAutocomplete_HumpsEnabled() then
      put 0 into tHumpsFrom
   else if tContext is among the items of "value,the,property" then
      put 3 into tHumpsFrom
   else
      put 2 into tHumpsFrom
   end if
   
   local tMatches
   if tToken is not empty then
      put ideAutocomplete_Matches(tToken, tHandlers, tBuiltins, tHumpsFrom) into tMatches
   else if tContext is "handler" then
      -- straight after the space: the handler or block, as worked out above
      put tHandlers into tMatches
   else
      -- straight after into, after or before: the variables in reach
      put line 1 to kMaxCompletions of tVariables into tMatches
      if tMatches is not empty and tOfferMessageBox then
         put return & "message box" after tMatches
      end if
   end if
   
   -- "ms" is the start of "msg": offer the message box for that too, spelled
   -- out, since "msg" itself is too short to be worth picking from a list.
   if tToken is not empty and tOfferMessageBox and "msg" begins with tToken and \
         "message box" is not among the lines of tMatches then
      put "message box" & return before tMatches
      if the last char of tMatches is return then
         delete the last char of tMatches
      end if
   end if
   
   -- "ms" is the start of "msg": offer the message box for that too, spelled
   -- out, since "msg" itself is too short to be worth picking from a list.
   if tToken is not empty and tOfferMessageBox and "msg" begins with tToken and \
         "message box" is not among the lines of tMatches then
      put "message box" & return before tMatches
      if the last char of tMatches is return then
         delete the last char of tMatches
      end if
   end if
   
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
-- Private: parameter hint
--------------------------------------------------------------------------------

-- Shows the parameters of the handler being called at the caret, or puts the
-- hint away if the caret is not in a call that has any.
private command ideAutocomplete_UpdateHint pEditorId, pObjectId, pField
   local tFrom, tTo
   put word 2 of the selectedChunk of pField into tFrom
   put word 4 of the selectedChunk of pField into tTo
   if tFrom is not a number or tTo >= tFrom then
      -- no caret, or a selection
      ideAutocomplete_HideHint
      exit ideAutocomplete_UpdateHint
   end if
   
   local tPreceding, tBefore
   if tFrom > 1 then
      put the text of char max(1, tFrom - 4000) to (tFrom - 1) of pField into tPreceding
      put the last line of (tPreceding & "x") into tBefore
      delete the last char of tBefore
   end if
   if tBefore is empty or ideAutocomplete_InComment(tBefore, tPreceding) then
      ideAutocomplete_HideHint
      exit ideAutocomplete_UpdateHint
   end if
   
   -- Innermost call first: the first one that has parameters to show wins
   local tCalls, tCall, tSignature
   put ideAutocomplete_CallsAtCaret(tBefore) into tCalls
   if tCalls is empty then
      ideAutocomplete_HideHint
      exit ideAutocomplete_UpdateHint
   end if
   
   -- Handlers in other scripts may have changed since they were last read:
   -- look again when a new hint is about to appear, not on every character.
   local tRuggedId
   put revRuggedId(pObjectId) into tRuggedId
   if sHintShowing is not true or tRuggedId is not among the keys of sPathHandlerInfo then
      ideAutocomplete_RefreshPathHandlers pObjectId
   end if
   set the itemDelimiter to tab
   repeat for each line tCall in tCalls
      put ideAutocomplete_Signature(item 1 of tCall, item 2 of tCall, pField, tRuggedId) into tSignature
      if tSignature is not empty then
         -- item 3 is which argument the caret is in, item 4 where the name
         -- starts in the line
         put pEditorId into sHintEditorId
         put pObjectId into sHintObjectId
         put pField into sHintField
         put tFrom into sHintCaret
         put the vScroll of pField into sHintScroll
         ideAutocomplete_ShowHint pEditorId, pField, tSignature, item 3 of tCall, \
               tFrom - the number of chars of tBefore + item 4 of tCall - 1
         exit ideAutocomplete_UpdateHint
      end if
   end repeat
   ideAutocomplete_HideHint
end ideAutocomplete_UpdateHint

-- Finds the calls the caret is inside, given the line up to the caret.
-- Returns one line per call, innermost first, each being: name, kind
-- ("function" or "command"), the number of the argument being typed, and the
-- char of the line the name starts at (tab-separated).
--   put foo(1, bar(|        ->  bar function 1, foo function 2
--   answer "Hi" with |      ->  answer command 1
private function ideAutocomplete_CallsAtCaret pBefore
   -- Blank out string literals, keeping every char in place, so brackets and
   -- commas inside them are not counted
   local tCode, tInString, tChar
   put false into tInString
   repeat for each char tChar in pBefore
      if tChar is quote then
         put not tInString into tInString
         put quote after tCode
      else if tInString then
         put "x" after tCode
      else
         put tChar after tCode
      end if
   end repeat
   
   -- The statement the caret is in starts after the last then, else or ;
   local tStart, tSeparator, tAt, tSkip
   put 1 into tStart
   repeat for each item tSeparator in " then , else ,;"
      put 0 into tSkip
      repeat
         put offset(tSeparator, tCode, tSkip) into tAt
         if tAt is 0 then exit repeat
         add tAt to tSkip
         put max(tStart, tSkip + the number of chars of tSeparator) into tStart
      end repeat
   end repeat
   
   -- Walk the statement keeping track of open brackets and the commas in each
   local tDepth, tOpenA, tCommasA, tIndex
   put 0 into tDepth
   put 0 into tCommasA[0]
   repeat with tIndex = tStart to the number of chars of tCode
      switch char tIndex of tCode
         case "("
            add 1 to tDepth
            put tIndex into tOpenA[tDepth]
            put 0 into tCommasA[tDepth]
            break
         case ")"
            if tDepth > 0 then subtract 1 from tDepth
            break
         case ","
            add 1 to tCommasA[tDepth]
            break
      end switch
   end repeat
   
   local tCalls, tName, tLead
   repeat with tIndex = tDepth down to 1
      if matchText(char tStart to (tOpenA[tIndex] - 1) of tCode, "([A-Za-z_][A-Za-z0-9_]*)$", tName) then
         put tName & tab & "function" & tab & (tCommasA[tIndex] + 1) & tab & \
               (tOpenA[tIndex] - the number of chars of tName) & return after tCalls
      end if
   end repeat
   -- A command takes its parameters after a space
   if matchText(char tStart to -1 of tCode, "^(\s*)([A-Za-z_][A-Za-z0-9_]*)\s", tLead, tName) then
      put tName & tab & "command" & tab & (tCommasA[0] + 1) & tab & \
            (tStart + the number of chars of tLead) & return after tCalls
   end if
   delete the last char of tCalls
   return tCalls
end ideAutocomplete_CallsAtCaret

-- Returns what to show for a call to pName: its declaration with parameters
-- for a handler, or its syntax from the dictionary for a built-in.  Empty if
-- it is not known or takes no parameters.
private function ideAutocomplete_Signature pName, pKind, pField, pRuggedId
   -- 1. a handler in the script being edited, applied or not
   local tScript
   put the text of pField into tScript
   filter lines of tScript with regex pattern \
         "(?i)^\s*(private\s+)?(on|command|function|getprop|setprop|before|after)\s+" & pName & "([^A-Za-z0-9_]|$)"
   if tScript is not empty then
      return ideAutocomplete_SignatureFromLine(line 1 of tScript)
   end if
   
   -- 2. a handler elsewhere in the message path
   local tInfo, tLine
   put sPathHandlerInfo[pRuggedId][toLower(pName)] into tInfo
   if tInfo is not empty then
      set the itemDelimiter to tab
      try
         put line (item 2 of tInfo) of the script of (item 3 of tInfo) into tLine
      catch tError
         -- a password-protected stack keeps its scripts to itself
         put empty into tLine
      end try
      return ideAutocomplete_SignatureFromLine(tLine)
   end if
   
   -- 3. a built-in.  Short commands (put, set, get) and keywords are typed
   -- far too often to want their syntax every time.
   if the number of chars of pName <= 3 or sQuietWordsA[toLower(pName)] then
      return empty
   end if
   -- Nor do declarations: anyone typing "local" knows what follows it.
   if pName is among the items of "local,global,constant" then
      return empty
   end if
   return ideAutocomplete_DocSyntax(pName, pKind)
end ideAutocomplete_Signature

-- Turns a handler's declaration line into what the hint shows:
--   function area pWidth, pHeight   ->  area(pWidth, pHeight)
--   command greet pName, pTimes     ->  greet pName, pTimes
-- Empty if the handler takes no parameters.
private function ideAutocomplete_SignatureFromLine pLine
   local tType, tName, tParams
   if not matchText(pLine, "(?i)^\s*(?:private\s+)?(on|command|function|getprop|setprop|before|after)\s+([A-Za-z_][A-Za-z0-9_]*)(.*)$", \
         tType, tName, tParams) then
      return empty
   end if
   -- drop a trailing comment and tidy the spacing
   local tComment
   repeat for each item tComment in "--,#,//"
      if offset(tComment, tParams) > 0 then
         put char 1 to offset(tComment, tParams) - 1 of tParams into tParams
      end if
   end repeat
   replace "(" with space in tParams
   replace ")" with space in tParams
   replace "@" with empty in tParams
   local tList, tParam
   repeat for each item tParam in tParams
      if word 1 of tParam is not empty then
         put word 1 of tParam & ", " after tList
      end if
   end repeat
   delete char -2 to -1 of tList
   if tList is empty then
      return empty
   end if
   if tType is "function" then
      return tName & "(" & tList & ")"
   end if
   return tName && tList
end ideAutocomplete_SignatureFromLine

-- Returns the syntax of a built-in command or function from the dictionary,
-- one form per line.  Looks in the database shipped with the IDE, then in
-- the documentation cache (extensions and libraries).
private function ideAutocomplete_DocSyntax pName, pKind
   local tKey
   put toLower(pName) & comma & pKind into tKey
   if tKey is among the keys of sSyntaxCache then
      return sSyntaxCache[tKey]
   end if
   
   local tSyntax, tConnection, tPath
   try
      put revIDESpecialFolderPath("API") & slash & "api.sqlite" into tPath
      if there is a file tPath then
         put revOpenDatabase("sqlite", tPath) into tConnection
         if tConnection is a number then
            put ideAutocomplete_DocSyntaxFrom(tConnection, pName, pKind) into tSyntax
            revCloseDatabase tConnection
         end if
      end if
   catch tError
   end try
   if tSyntax is empty then
      try
         ideDocsInitialize
         put the result into tConnection
         if tConnection is a number then
            put ideAutocomplete_DocSyntaxFrom(tConnection, pName, pKind) into tSyntax
         end if
      catch tError
      end try
   end if
   
   put tSyntax into sSyntaxCache[tKey]
   return tSyntax
end ideAutocomplete_DocSyntax

private function ideAutocomplete_DocSyntaxFrom pConnection, pName, pKind
   -- pName has already been checked to be a plain identifier
   local tCursor, tData, tEntryA, tSyntax, tForm, tCount, tIndex
   put revQueryDatabase(pConnection, "SELECT entry_data FROM dictionary_data WHERE entry_name = '" & pName & \
         "' COLLATE NOCASE AND entry_type = '" & pKind & "' AND library_name NOT LIKE '%Builder'") into tCursor
   if tCursor is not a number then
      return empty
   end if
   if not revQueryIsAtEnd(tCursor) then
      get revDatabaseColumnNamed(tCursor, "entry_data", "tData")
      put arrayDecode(tData) into tEntryA
   end if
   revCloseCursor tCursor
   
   if tEntryA["syntax"] is not an array then
      return empty
   end if
   put 0 into tCount
   repeat with tIndex = 1 to the number of elements of tEntryA["syntax"]
      put tEntryA["syntax"][tIndex] into tForm
      replace return with space in tForm
      -- for a function, the form with brackets is the one being typed
      if tForm is empty or (pKind is "function" and "(" is not in tForm) then
         next repeat
      end if
      put tForm & return after tSyntax
      add 1 to tCount
      if tCount >= kMaxHintLines then
         exit repeat
      end if
   end repeat
   delete the last char of tSyntax
   return tSyntax
end ideAutocomplete_DocSyntaxFrom

private command ideAutocomplete_CreateHint
   if there is a stack kHintName then
      delete stack kHintName
   end if
   create invisible stack kHintName
   set the style of stack kHintName to "palette"
   set the decorations of stack kHintName to empty
   set the resizable of stack kHintName to false
   
   create field "hint" in card 1 of stack kHintName
   set the lockText of field "hint" of stack kHintName to true
   try
      set the traversalOn of field "hint" of stack kHintName to false
   catch tErr
   end try
   set the dontWrap of field "hint" of stack kHintName to true
   set the showBorder of field "hint" of stack kHintName to true
   set the vScrollbar of field "hint" of stack kHintName to false
   set the hScrollbar of field "hint" of stack kHintName to false
   set the textSize of field "hint" of stack kHintName to 12
   set the margins of field "hint" of stack kHintName to 6
end ideAutocomplete_CreateHint

-- Shows pText just above the line being typed, starting over the name of the
-- handler being called (char pNameChar of pField).  For a handler, the
-- parameter being typed (number pArgument) is picked out in bold.
private command ideAutocomplete_ShowHint pEditorId, pField, pText, pArgument, pNameChar
   if there is not a stack kHintName then
      ideAutocomplete_CreateHint
      put false into sHintShowing
   end if
   
   local tHint
   put the long id of field "hint" of stack kHintName into tHint
   if the text of tHint is not pText then
      set the text of tHint to pText
   end if
   set the textStyle of char 1 to -1 of tHint to "plain"
   
   -- One line with no optional parts is a handler's own declaration: its
   -- parameters are separated by commas and can be counted off.  Dictionary
   -- syntax ([optional], {a | b}) cannot, so it is left plain.
   if the number of lines of pText is 1 and "[" is not in pText and "{" is not in pText \
         and "<" is not in pText then
      local tListStart, tSkip, tComma, tParamStart, tParamEnd, tCount
      put offset("(", pText) into tListStart
      if tListStart is 0 then
         put offset(space, pText) into tListStart
      end if
      put tListStart + 1 into tParamStart
      put 0 into tCount
      repeat
         add 1 to tCount
         put offset(",", pText, tParamStart - 1) into tComma
         if tComma is 0 then
            put the number of chars of pText into tParamEnd
            if the last char of pText is ")" then
               subtract 1 from tParamEnd
            end if
         else
            put tParamStart - 1 + tComma - 1 into tParamEnd
         end if
         if tCount is pArgument then
            set the textStyle of char tParamStart to tParamEnd of tHint to "bold"
            exit repeat
         end if
         if tComma is 0 then exit repeat
         put tParamEnd + 3 into tParamStart
      end repeat
   end if
   
   -- Size to fit
   local tWidth, tHeight
   put the formattedWidth of tHint + 4 into tWidth
   put the formattedHeight of tHint into tHeight
   set the width of stack kHintName to tWidth
   set the height of stack kHintName to tHeight
   set the rect of tHint to 0, 0, tWidth, tHeight
   
   -- Above the line, starting over the name of the handler
   local tNameRect, tLoc, tX, tY, tScreenRect
   put the formattedRect of char pNameChar of pField into tNameRect
   if tNameRect is empty then
      ideAutocomplete_HideHint
      exit ideAutocomplete_ShowHint
   end if
   put ideAutocomplete_GlobalLoc(pField, item 1 of tNameRect, item 2 of tNameRect) into tLoc
   put item 1 of tLoc into tX
   put item 2 of tLoc - tHeight - 2 into tY
   put ideAutocomplete_ScreenRectFor(pEditorId, item 1 of tLoc, item 2 of tLoc) into tScreenRect
   if tX + tWidth > item 3 of tScreenRect then
      put item 3 of tScreenRect - tWidth into tX
   end if
   if tX < item 1 of tScreenRect then
      put item 1 of tScreenRect into tX
   end if
   if tY < item 2 of tScreenRect then
      -- no room above: go below the line instead
      put item 2 of tLoc + (item 4 of tNameRect - item 2 of tNameRect) + 2 into tY
   end if
   set the topLeft of stack kHintName to tX, tY
   
   -- Same care as the completion list: showing a window stirs up focus
   -- changes that must neither take the keyboard nor dismiss what is shown.
   put true into sSuppressCancel
   if sHintShowing is not true then
      ideAutocomplete_RevealWindow kHintName, tX, tY
      put true into sHintShowing
   end if
   ideAutocomplete_RefocusEditor pEditorId
   if the platform contains "win" then
      send "ideAutocomplete_ClearSuppressCancel" to me in 150 milliseconds
   else
      send "ideAutocomplete_ClearSuppressCancel" to me in 0 milliseconds
   end if
   
   cancel sHintWatchTimer
   put 0 into sHintFocusMisses
   send "ideAutocomplete_WatchHint" to me in kHintWatchInterval milliseconds
   put the result into sHintWatchTimer
end ideAutocomplete_ShowHint

private command ideAutocomplete_HideHint
   cancel sHintWatchTimer
   if sHintShowing is true then
      ideAutocomplete_ConcealWindow kHintName
   end if
   put false into sHintShowing
end ideAutocomplete_HideHint

-- Not private: timer-fired sends cannot reach private handlers.
-- Keeps a showing hint in step with the caret.  Nothing tells the library
-- when the caret is moved by a click or an arrow key, so while a hint is up
-- this looks every so often: the hint goes when the caret leaves the call
-- (another line, or past its closing bracket), when the script is scrolled,
-- or when the caret goes to another field; and it is redrawn when the caret
-- moves to another parameter.  Moving the mouse does not disturb it.
command ideAutocomplete_WatchHint
   if sHintShowing is not true then
      exit ideAutocomplete_WatchHint
   end if
   if sHintField is empty or there is not a sHintField then
      ideAutocomplete_HideHint
      exit ideAutocomplete_WatchHint
   end if
   if the vScroll of sHintField is not sHintScroll then
      ideAutocomplete_HideHint
      exit ideAutocomplete_WatchHint
   end if
   
   -- The caret can leave the editor for an instant when a popup is shown or
   -- clicked, so only two looks in a row without it count.
   local tFocused
   try
      put the long id of the selectedField into tFocused
   catch tError
      put empty into tFocused
   end try
   if tFocused is the long id of sHintField then
      put 0 into sHintFocusMisses
   else
      add 1 to sHintFocusMisses
      if sHintFocusMisses >= 2 then
         ideAutocomplete_HideHint
         exit ideAutocomplete_WatchHint
      end if
   end if
   
   -- An edit brings its own update; this is for a caret moved without one.
   if sUpdatePending is not true and word 2 of the selectedChunk of sHintField is not sHintCaret then
      ideAutocomplete_UpdateHint sHintEditorId, sHintObjectId, sHintField
      -- (that has either hidden the hint or shown it again, restarting this)
      exit ideAutocomplete_WatchHint
   end if
   
   send "ideAutocomplete_WatchHint" to me in kHintWatchInterval milliseconds
   put the result into sHintWatchTimer
end ideAutocomplete_WatchHint

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
      ideAutocomplete_RevealWindow kPopupName, tX, tY
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
      ideAutocomplete_ConcealWindow kPopupName
      put false into sPopupShowing
   end if
   put empty into sCurrentEditorId
end ideAutocomplete_HidePopup

-- Puts the list or the hint on screen at pX,pY, and takes it off again.
--
-- On Windows a window is activated when it is shown, which takes the title
-- bar highlight from the script editor until focus is handed back: a flicker
-- every time the list appears.  Moving a window does not activate it, so
-- there the window is shown once, out of sight, and from then on is moved
-- into place and back out instead of being shown and hidden.
private command ideAutocomplete_RevealWindow pName, pX, pY
   if the platform contains "win" then
      if not the visible of stack pName then
         set the topLeft of stack pName to kParkedLoc
         show stack pName
      end if
      set the topLeft of stack pName to pX, pY
      exit ideAutocomplete_RevealWindow
   end if
   
   show stack pName
   -- Some window managers place a window themselves when it is first
   -- mapped, ignoring where it was put while hidden: position it again.
   if the topLeft of stack pName is not (pX,pY) then
      set the topLeft of stack pName to pX, pY
   end if
end ideAutocomplete_RevealWindow

private command ideAutocomplete_ConcealWindow pName
   if the platform contains "win" then
      set the topLeft of stack pName to kParkedLoc
   else
      hide stack pName
   end if
end ideAutocomplete_ConcealWindow

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
   if sSavedToken is empty and sSavedTokenStart > 1 then
      -- nothing typed yet: line up with the caret, just after the space
      put the formattedRect of char (sSavedTokenStart - 1) of tField into tTokenRect
      if tTokenRect is not empty then
         put item 3 of tTokenRect into item 1 of tTokenRect
      end if
   else if sSavedTokenStart > 0 then
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
   -- (sSavedToken is empty when the list came up straight after a space: the
   -- completion is then simply inserted at the caret.)
   if sSavedTokenStart is not a number or sSavedTokenStart < 1 then
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
