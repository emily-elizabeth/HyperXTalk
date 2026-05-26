on startup
   testShowHide
end startup

on testShowHide
   local tStack
   put "ShowHideTest" into tStack
   if there is a stack tStack then delete stack tStack
   
   create invisible stack tStack
   set the defaultStack to tStack
   
   create button "btn1"
   put "PASS: Button created" & return
   
   set the visible of stack tStack to true
   put "PASS: Stack shown" & return
   
   set the visible of stack tStack to false
   put "PASS: Stack hidden" & return
   
   create card "Card2"
   put "PASS: Card2 created" & return
   
   create button "btn2"
   put "PASS: Button on Card2 created" & return
   
   set the visible of stack tStack to true
   put "PASS: Stack shown again" & return
   
   set the visible of stack tStack to false
   put "PASS: Stack hidden again" & return
   
   delete stack tStack
   put "PASS: Stack deleted" & return
   
   quit 0
end testShowHide
