script "GrabStressTest"
-- Test rapid focus switching, cursor updates, and grabbing loops
-- Specifically stresses GdkSeat pointer grab and ungrab pathways under the engine

on openStack
   grabStressTest
end openStack

on grabStressTest
   put "GrabStressTest" into tStack

   -- Create stack
   create stack tStack
   set the defaultStack to tStack

   -- Create controls
   create button "btn1"
   set the rect of button "btn1" to "10,10,100,40"
   
   create field "fld1"
   set the rect of field "fld1" to "10,50,200,100"

   create button "btn2"
   set the style of button "btn2" to "menu" -- option menu style
   set the rect of button "btn2" to "10,160,150,200"

   -- Stress loop: 100 iterations of cursor updates and focus swaps
   repeat with i = 1 to 100
      -- Swap focus
      focus on button "btn1"
      focus on field "fld1"
      
      -- Rapid cursor modifications (forces GdkCursor transitions)
      set the cursor to hand
      set the cursor to watch
      set the cursor to arrow
      set the cursor to crosshair

      -- Simulate click selection
      click at the loc of button "btn1"
   end repeat

   -- Cleanup
   close stack tStack

   put "=== Grab/Focus Stress Test COMPLETE ===" & return
   put "If you're reading this, no crash occurred." & return
   quit 0
end grabStressTest
