script "FontDialogTest"
-- Test font dialog command for GTK3 crash safety
-- Uses non-blocking variants or try/catch to avoid test hangs in headless environments

on openStack
   fontDialogTest
end openStack

on fontDialogTest
   put "FontDialogTest" into tStack

   -- Create stack
   create stack tStack
   set the defaultStack to tStack

   -- Test: answer font (invokes the GTK3 font chooser dialog)
   try
      answer font
      put "PASS: answer font dialog executed" & return
   catch tError
      put "INFO: answer font error (non-fatal): " & tError & return
   end try

   -- Cleanup
   close stack tStack

   put "=== Font Dialog Test COMPLETE ===" & return
   put "If you're reading this, no crash occurred." & return
   quit 0
end fontDialogTest
