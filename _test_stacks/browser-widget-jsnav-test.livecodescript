script "BrowserWidgetAdvancedTest"
-- Test browser widget JS eval and navigation - crash test only
-- Does NOT verify correctness, only verifies no crash

on openStack
   browserWidgetAdvancedTest
end openStack

on browserWidgetAdvancedTest
   put "Browser Advanced Test" into tStack

   -- Create stack
   create stack tStack
   set the defaultStack to tStack

   -- Create browser widget
   create widget "testBrowser" as "com.livecode.widget.browser"

   -- Test 1: Set HTML and execute JS
   set the htmlText of widget "testBrowser" to "<html><body><div id='test'>hello</div></body></html>"

   -- Execute JavaScript (may return error on some platforms, but should not crash)
   try
      do "document.getElementById('test').innerHTML = 'world'" in widget "testBrowser"
      put "PASS: JS eval executed" & return
   catch tError
      put "INFO: JS eval returned error (non-fatal): " & tError & return
   end try

   -- Test 2: Navigate to URL
   try
      set the url of widget "testBrowser" to "about:blank"
      put "PASS: Navigation to about:blank" & return
   catch tError
      put "INFO: Navigation error (non-fatal): " & tError & return
   end try

   -- Test 3: Get URL property
   try
      put the url of widget "testBrowser" into tUrl
      put "PASS: URL property accessible: " & tUrl & return
   catch tError
      put "INFO: URL property error (non-fatal): " & tError & return
   end try

   -- Test 4: Set and get browser rect
   set the rect of widget "testBrowser" to "10,10,300,200"
   put the rect of widget "testBrowser" into tRect
   put "PASS: Rect set/get: " & tRect & return

   -- Test 5: Show/hide cycle
   hide widget "testBrowser"
   show widget "testBrowser"
   put "PASS: Show/hide cycle" & return

   -- Test 6: Set HTML again after show
   set the htmlText of widget "testBrowser" to "<html><body>updated</body></html>"
   put "PASS: HTML update after show" & return

   -- Cleanup
   close stack tStack

   put "=== Browser Advanced Test COMPLETE ===" & return
   put "If you're reading this, no crash occurred." & return
   quit 0
end browserWidgetAdvancedTest
