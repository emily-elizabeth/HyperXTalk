script "revSplashStackBehavior"
setProp statusMessage pMessage
   put pMessage into field "status" of me
end statusMessage


on PreOpenStack
   -- Centre the splash on whichever screen the mouse is on at launch time.
   -- The IDE stacks aren't open yet when the splash fires, so the mouse
   -- position is the only reliable indicator of the user's active screen.
   local tRefH, tRefV
   put item 1 of the mouseLoc into tRefH
   put item 2 of the mouseLoc into tRefV

   -- Walk screenRects to find which screen contains the mouse.
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
