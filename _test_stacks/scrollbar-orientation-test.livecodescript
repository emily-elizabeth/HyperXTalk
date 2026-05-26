on startup
   testScrollbarOrientation
end startup

on testScrollbarOrientation
   create stack "TestStack"
   set the defaultStack to "TestStack"
   
   create scrollbar "scr1"
   put "PASS: Default scrollbar created" & return
   
   set the orientation of scrollbar "scr1" to "horizontal"
   put "PASS: Horizontal orientation set" & return
   
   delete stack "TestStack"
   quit 0
end testScrollbarOrientation
