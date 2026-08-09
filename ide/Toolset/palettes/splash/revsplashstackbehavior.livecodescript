script "revSplashStackBehavior"
setProp statusMessage pMessage
   put pMessage into field "status" of me
end statusMessage


on PreOpenStack
   -- Centre the splash on whichever screen the IDE menubar is on.
   -- Falls back to the tool palette, then the primary screen centre.
   local tRefLoc
   if exists(stack "revMenubar") then
      put the loc of stack "revMenubar" into tRefLoc
   else if exists(stack "revIDEPalette") then
      put the loc of stack "revIDEPalette" into tRefLoc
   else
      set the location of this stack to the screenLoc
      exit PreOpenStack
   end if

   -- Walk screenRects to find which screen contains the reference point.
   local tTargetRect
   put line 1 of the screenRects into tTargetRect
   repeat for each line tRect in the screenRects
      if item 1 of tRect <= item 1 of tRefLoc and item 1 of tRefLoc < item 3 of tRect and \
            item 2 of tRect <= item 2 of tRefLoc and item 2 of tRefLoc < item 4 of tRect then
         put tRect into tTargetRect
         exit repeat
      end if
   end repeat

   local tCX, tCY
   put (item 1 of tTargetRect + item 3 of tTargetRect) / 2 into tCX
   put (item 2 of tTargetRect + item 4 of tTargetRect) / 2 into tCY
   set the location of this stack to tCX, tCY
end PreOpenStack
