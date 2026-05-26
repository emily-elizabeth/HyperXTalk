script "IMEInputTest"
-- Test IME-related functionality for GTK3 crash safety
-- Tests text input properties that interact with GtkIMContext

on openStack
   imeInputTest
end openStack

on imeInputTest
   put "IMEInputTest" into tStack

   -- Create stack
   create stack tStack
   set the defaultStack to tStack

   -- Create a field to test IME
   create field "testField"

   -- Test 1: unicodeText (triggers IME-related code paths)
   try
      set the unicodeText of field "testField" to "日本語テスト"
      put "PASS: unicodeText set" & return
   catch tError
      put "INFO: unicodeText error (non-fatal): " & tError & return
   end try

   -- Test 2: Set text and read it back
   set the text of field "testField" to "test"
   put the text of field "testField" into tText
   put "PASS: text get/set: " & tText & return

   -- Test 3: textFont property
   try
      set the textFont of field "testField" to "Arial"
      put the textFont of field "testField" into tFont
      put "PASS: textFont: " & tFont & return
   catch tError
      put "INFO: textFont error (non-fatal): " & tError & return
   end try

   -- Test 4: textSize property
   try
      set the textSize of field "testField" to 14
      put the textSize of field "testField" into tSize
      put "PASS: textSize: " & tSize & return
   catch tError
      put "INFO: textSize error (non-fatal): " & tError & return
   end try

   -- Test 5: Keyboard focus
   focus on field "testField"
   put "PASS: focus field" & return

   -- Test 6: Select all text
   select all text of field "testField"
   put "PASS: select text" & return

   -- Cleanup
   close stack tStack

   put "=== IME Input Test COMPLETE ===" & return
   put "If you're reading this, no crash occurred." & return
   quit 0
end imeInputTest
