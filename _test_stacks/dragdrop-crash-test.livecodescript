script "DragDropCrashTest"
-- Test drag & drop related functionality for GTK3 crash safety
-- Focus on dragData, dragImage, and related properties

on openStack
   dragDropCrashTest
end openStack

on dragDropCrashTest
   put "DragDropTest" into tStack

   -- Create stack
   create stack tStack
   set the defaultStack to tStack

   -- Create a button to test drag properties
   create button "dragSource"

   -- Test 1: dragData access (should not crash)
   try
      put the dragData into tData
      put "PASS: dragData read: " & the number of chars in tData & " chars" & return
   catch tError
      put "INFO: dragData error (non-fatal): " & tError & return
   end try

   -- Test 2: Set dragData
   try
      set the dragData to "test data"
      put "PASS: dragData set" & return
   catch tError
      put "INFO: dragData set error (non-fatal): " & tError & return
   end try

   -- Test 3: dragImage property
   try
      set the dragImage to 0
      put "PASS: dragImage set" & return
   catch tError
      put "INFO: dragImage error (non-fatal): " & tError & return
   end try

   -- Test 4: dragAction property
   try
      set the dragAction to "copy"
      put the dragAction into tAction
      put "PASS: dragAction: " & tAction & return
   catch tError
      put "INFO: dragAction error (non-fatal): " & tError & return
   end try

   -- Cleanup
   close stack tStack

   put "=== Drag & Drop Crash Test COMPLETE ===" & return
   put "If you're reading this, no crash occurred." & return
   quit 0
end dragDropCrashTest
