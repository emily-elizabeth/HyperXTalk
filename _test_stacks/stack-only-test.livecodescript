on startup
   testStackOnly
end startup

on testStackOnly
   local tStack
   put "TestStackOnly" into tStack
   if there is a stack tStack then delete stack tStack
   create invisible stack tStack
   set the defaultStack to tStack
   put "PASS: Stack created" & return
   delete stack tStack
   put "PASS: Stack deleted" & return
   quit 0
end testStackOnly
