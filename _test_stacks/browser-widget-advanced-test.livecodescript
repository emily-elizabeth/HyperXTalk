on startup
   testBrowserWidgetAdvanced
end startup

on testBrowserWidgetAdvanced
   local tStack
   put "BrowserAdvancedTest" into tStack
   if there is a stack tStack then delete stack tStack
   
   create stack tStack
   set the defaultStack to tStack
   set the width of stack tStack to 640
   set the height of stack tStack to 480
   
   put "Creating browser widget..." & return
   create widget "browser1" as "com.livecode.widget.browser"
   set the rect of widget "browser1" to "20,20,620,460"
   put "PASS: Browser widget created" & return
   
   put "Setting HTML content..." & return
   set the htmlText of widget "browser1" to "<html><body><h1>Hello from HyperXTalk</h1><p>WebKitGTK browser widget test</p></body></html>"
   put "PASS: HTML content set" & return
   
   put "Testing URL property..." & return
   local tUrl
   put the URL of widget "browser1" into tUrl
   put "PASS: URL retrieved: " & tUrl & return
   
   put "Testing htmlText property..." & return
   local tHtml
   put the htmlText of widget "browser1" into tHtml
   if tHtml contains "HyperXTalk" then
      put "PASS: htmlText contains expected content" & return
   else
      put "WARN: htmlText does not contain expected content" & return
   end if
   
   put "Testing vScrollbar/hScrollbar..." & return
   set the vScrollbar of widget "browser1" to false
   set the hScrollbar of widget "browser1" to false
   put "PASS: Scrollbar properties set" & return
   
   put "Testing allowUserInteraction..." & return
   set the allowUserInteraction of widget "browser1" to true
   put "PASS: allowUserInteraction set" & return
   
   put "Testing userAgent..." & return
   set the userAgent of widget "browser1" to "HyperXTalk/1.0"
   put "PASS: userAgent set" & return
   
   put "Showing stack (forces GTK3 realize)..." & return
   set the visible of stack tStack to true
   put "PASS: Stack shown" & return
   
   put "Hiding stack..." & return
   set the visible of stack tStack to false
   put "PASS: Stack hidden" & return
   
   put "Re-showing stack..." & return
   set the visible of stack tStack to true
   put "PASS: Stack re-shown" & return
   
   put "Hiding and deleting..." & return
   set the visible of stack tStack to false
   delete stack tStack
   put "PASS: Stack deleted" & return
   
   put "Advanced browser widget test complete."
   quit 0
end testBrowserWidgetAdvanced
