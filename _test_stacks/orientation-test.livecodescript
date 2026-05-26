on startup
   testOrientation
end startup

on testOrientation
   create stack "TestStack"
   set the defaultStack to "TestStack"
   
   create scrollbar "scr1"
   put "PASS: Scrollbar created" & return
   
   set the orientation of scrollbar "scr1" to "horizontal"
   put "PASS: Orientation set" & return
   
   delete stack "TestStack"
   quit 0
end testOrientation
