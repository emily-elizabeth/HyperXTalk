script "revSplashStackBehavior"
setProp statusMessage pMessage
   put pMessage into field "status" of me
end statusMessage


on PreOpenStack
   -- Try to read the last saved menubar position from the preferences stack
   -- so the splash opens on the correct screen immediately, with no visual hop.
   -- The prefs file is loaded by referencing it by path; the normal startup
   -- sequence will find it already in memory and continue as usual.
   local tPrefsPath, tSavedRect
   put the homeFolderPath & "/.hyperxtalk/preferences/hyperxtalk7.rev" into tPrefsPath

   if there is a file tPrefsPath then
      put the palette_rect_revMenubar of stack tPrefsPath into tSavedRect
   end if

   local tRefH, tRefV
   if tSavedRect is not empty then
      -- Use the centre of the saved menubar rect to identify the target screen.
      put (item 1 of tSavedRect + item 3 of tSavedRect) / 2 into tRefH
      put (item 2 of tSavedRect + item 4 of tSavedRect) / 2 into tRefV
   else
      -- First run or prefs missing — fall back to the mouse position.
      put item 1 of the mouseLoc into tRefH
      put item 2 of the mouseLoc into tRefV
   end if

   -- Walk screenRects to find which screen contains the reference point.
   local tTargetRect
   put line 1 of the screenRects into tTargetRect
   repeat for each line tRect in the screenRects
      if item 1 of tRect <= tRefH and tRefH < item 3 of tRect and \
            item 2 of tRect <= tRefV and tRefV < item 4 of tRect then
         put tRect into tTargetRect
         exit repeat
      end if
   end repeat

   local tCX, tCY
   put (item 1 of tTargetRect + item 3 of tTargetRect) / 2 into tCX
   put (item 2 of tTargetRect + item 4 of tTargetRect) / 2 into tCY
   set the location of this stack to tCX, tCY
end PreOpenStack
