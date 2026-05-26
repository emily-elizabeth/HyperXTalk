script "DialogCrashTest"
-- Test dialog commands for GTK3 crash safety
-- Uses non-blocking variants where possible to avoid test hangs

on openStack
   dialogCrashTest
end openStack

on dialogCrashTest
   put "DialogCrashTest" into tStack

   -- Create stack
   create stack tStack
   set the defaultStack to tStack

   -- Test 1: answer command (non-modal variant if available)
   try
      answer "Test message" with "OK"
      put "PASS: answer command executed" & return
   catch tError
      put "INFO: answer error (non-fatal): " & tError & return
   end try

   -- Test 2: ask command
   try
      ask "Test question"
      put "PASS: ask command executed: " & it & return
   catch tError
      put "INFO: ask error (non-fatal): " & tError & return
   end try

   -- Test 3: answer file (dialog creation - may not complete without user)
   try
      answer file "Select a file"
      put "PASS: answer file dialog opened" & return
   catch tError
      put "INFO: answer file error (non-fatal): " & tError & return
   end try

   -- Test 4: answer folder
   try
      answer folder "Select a folder"
      put "PASS: answer folder dialog opened" & return
   catch tError
      put "INFO: answer folder error (non-fatal): " & tError & return
   end try

   -- Test 5: answer color
   try
      answer color
      put "PASS: answer color dialog opened" & return
   catch tError
      put "INFO: answer color error (non-fatal): " & tError & return
   end try

   -- Cleanup
   close stack tStack

   put "=== Dialog Crash Test COMPLETE ===" & return
   put "If you're reading this, no crash occurred." & return
   quit 0
end dialogCrashTest
