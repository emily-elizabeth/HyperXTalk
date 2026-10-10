script "revSplashStackBehavior"

-- Prevents repeated repositioning on every status message update.
local sPositioned

setProp statusMessage pMessage
   put pMessage into field "status" of me
   -- The behaviour is not yet loaded when preOpenStack fires, so we position
   -- on the first status message instead â by then it's definitely active.
   if not sPositioned then
      put true into sPositioned
      revSplashBehavior__CentreOnIDEScreen
   end if
end statusMessage


private command revSplashBehavior__CentreOnIDEScreen
   -- Try the saved menubar rect from the preferences stack first.
   local tPrefsPath, tSavedRect
   put revEnvironmentUserPreferencesPath() & "/hyperxtalk7.rev" into tPrefsPath
   if there is a file tPrefsPath then
      put the palette_rect_revMenubar of stack tPrefsPath into tSavedRect
   end if

   local tRefH, tRefV
   if tSavedRect is not empty then
      -- Use the centre of the saved menubar rect to identify the target screen.
      put (item 1 of tSavedRect + item 3 of tSavedRect) / 2 into tRefH
      put (item 2 of tSavedRect + item 4 of tSavedRect) / 2 into tRefV
   else
      -- First run or prefs missing â fall back to the mouse position.
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
end revSplashBehavior__CentreOnIDEScreen
