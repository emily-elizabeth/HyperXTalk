on startup
   testBrowserWidget
end startup

on testBrowserWidget
   local tStack
   put "BrowserTest" into tStack
   if there is a stack tStack then delete stack tStack
   
   create stack tStack
   set the defaultStack to tStack
   set the width of stack tStack to 640
   set the height of stack tStack to 480
   
   put "Creating browser widget..." & return
   create widget "browser1" as "com.livecode.widget.browser"
   set the rect of widget "browser1" to "20,20,620,460"
   put "PASS: Browser widget created" & return
   
   put "Setting URL..." & return
   set the URL of widget "browser1" to "about:blank"
   put "PASS: URL set" & return
   
   put "Setting HTML..." & return
   set the htmlText of widget "browser1" to "<html><body><h1>Test</h1></body></html>"
   put "PASS: HTML set" & return
   
   put "Testing properties..." & return
   set the vScrollbar of widget "browser1" to false
   set the hScrollbar of widget "browser1" to false
   set the allowUserInteraction of widget "browser1" to true
   put "PASS: Properties set" & return
   
   put "Cleanup..." & return
   delete stack tStack
   put "PASS: Stack deleted" & return
   
   put "Browser widget test complete."
   quit 0
end testBrowserWidget
