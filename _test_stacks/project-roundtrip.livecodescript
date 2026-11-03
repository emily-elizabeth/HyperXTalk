script "project-roundtrip"

/*
Headless round trip of a stack through the project file format, without the IDE.

It serialises a stack to project JSON with revIDEProjectBuildTree, parses the
JSON back, rebuilds the stack with revIDEProjectReconstructStack, serialises
that again and reports any difference between the two trees. The two JSON
files (a.json, b.json) and their media/scripts folders are left in RT_OUT.

Environment variables:
   PJ_LIB      path to photonjson.livecodescript
   HXT_LIB     path to ide/Toolset/libraries/revidelibrary.8.livecodescript
   RT_STACK    the stack file to round-trip
   RT_OUT      an empty output folder
   HXT_LIBS    optional newline-separated paths of further IDE library stacks to
               put in use first, e.g. ide/Toolset/palettes/revdatagridlibrary.rev
               for stacks that contain Data Grids
   RT_MODULES  newline-separated .lcm module paths to load first (widget kinds
               used by the stack and their library dependencies)

Example (from the repository root, dev build):
   PE=_build/mac/Release/packaged_extensions
   RT_MODULES="$(ls $PE/com.livecode.library.widgetutils/module.lcm \
      $PE/com.livecode.library.canvas/module.lcm $PE/com.livecode.library.iconsvg/module.lcm \
      $PE/com.livecode.library.scriptitems/module.lcm $PE/com.livecode.widget.*/module.lcm)" \
   PJ_LIB=$PWD/extensions/script-libraries/photonjson/photonjson.livecodescript \
   HXT_LIB=$PWD/ide/Toolset/libraries/revidelibrary.8.livecodescript \
   RT_STACK=$PWD/_test_stacks/allcontrols2.hyperxtalk RT_OUT=/tmp/rt \
   _build/mac/Release/HyperXTalk.app/Contents/MacOS/HyperXTalk -ui _test_stacks/project-roundtrip.livecodescript

Known differences that are not JSON-related: a scrollbar's lineInc is
recomputed by the engine when its other properties are set, script file
names for unnamed cards differ ("card_1002" vs "card_id_1002"), and the
stack id cannot be restored on a freshly created stack.
*/

on startup
   local tOut, tName, tTree, tTree2, tJSON, tDB, tDI, tDG, tNewName, tTreeB, tJSONB, tLine, e
   put $RT_OUT into tOut
   lock messages
   repeat for each line tLine in $RT_MODULES
      try
         load extension from file tLine
      catch e
         put "load extension failed: " & tLine & " : " & line 1 of e & return
      end try
   end repeat
   try
      start using stack $PJ_LIB
      start using stack $HXT_LIB
      -- further IDE library stacks the serialiser may need (e.g. the Data Grid library)
      repeat for each line tLine in $HXT_LIBS
         if tLine is not empty then start using stack tLine
      end repeat
   catch e
      put "library load error: " & e & return
      quit
   end try
   put the short name of stack $RT_STACK into tName
   put "stack loaded: " & tName & "; substacks: " & replaceText(the substacks of stack tName, "\n", ", ") & return
   create folder tOut & "/a"
   create folder tOut & "/a/media"
   create folder tOut & "/a/scripts"
   try
      revIDEProjectBuildTree (the long id of stack tName), tOut & "/a/media", tOut & "/a/scripts", tTree
      revIDEProjectSanitizeArray tTree
   catch e
      put "build tree error: " & e & return
      quit
   end try
   put JSONStringify(tTree, empty, true) into tJSON
   put textEncode(tJSON, "UTF-8") into url ("binfile:" & tOut & "/a.json")
   put "a.json written: " & the number of chars in tJSON & " chars" & return
   put JSONParser(textEncode(tJSON, "UTF-8")) into tTree2
   put "parsed back equals tree: " & (tTree2 is tTree) & return
   if tTree2 is not tTree then
      put diffArrays(tTree, tTree2, "", 0)
   end if

   -- rebuild the stack from the parsed tree, then serialise it again;
   -- the original and its substacks go first so the names are free
   local tSub
   repeat for each line tSub in the substacks of stack tName
      delete stack tSub
   end repeat
   delete stack tName
   try
      revIDEProjectReconstructStack tTree2["stack"], tOut & "/a", empty, tDB, tDI, tDG
      put the result into tNewName
   catch e
      put "reconstruct error: " & e & return
      quit
   end try
   put "reconstructed: " & tNewName & "; substacks: " & replaceText(the substacks of stack tNewName, "\n", ", ") & return
   create folder tOut & "/b"
   create folder tOut & "/b/media"
   create folder tOut & "/b/scripts"
   try
      revIDEProjectBuildTree (the long id of stack tNewName), tOut & "/b/media", tOut & "/b/scripts", tTreeB
      revIDEProjectSanitizeArray tTreeB
   catch e
      put "build tree B error: " & e & return
      quit
   end try
   put JSONStringify(tTreeB, empty, true) into tJSONB
   put textEncode(tJSONB, "UTF-8") into url ("binfile:" & tOut & "/b.json")
   put "b.json written: " & the number of chars in tJSONB & " chars" & return
   put "b equals parsed a: " & (tTreeB is tTree2) & return
   if tTreeB is not tTree2 then
      put diffArrays(tTree2, tTreeB, "", 0)
   end if
   quit
end startup

-- prints up to 25 differing leaves between two arrays
function diffArrays pA, pB, pPath, pCount
   local tOut, k, tKeys, tSub
   if pCount > 25 then return empty
   put the keys of pA & return & the keys of pB into tKeys
   sort lines of tKeys
   put empty into tSub
   repeat for each line k in tKeys
      if k is tSub then next repeat
      put k into tSub
      if pA[k] is an array or pB[k] is an array then
         if pA[k] is an array and pB[k] is an array then
            put diffArrays(pA[k], pB[k], pPath & "/" & k, pCount + the number of lines in tOut) after tOut
         else
            put "  " & pPath & "/" & k & ": array on one side only" & return after tOut
         end if
      else if pA[k] is not pB[k] or the number of chars in pA[k] is not the number of chars in pB[k] then
         put "  " & pPath & "/" & k & ": <" & char 1 to 60 of pA[k] & "> vs <" & char 1 to 60 of pB[k] & ">" & return after tOut
      end if
      if the number of lines in tOut + pCount > 25 then exit repeat
   end repeat
   return tOut
end diffArrays

-- stubs for the few IDE handlers the project code calls
function revIDELocalise pText
   return pText
end revIDELocalise

on revIDEAddRecentStack pPath
end revIDEAddRecentStack
