on startup
   testMinimalScrollbar
end startup

on testMinimalScrollbar
   create stack "TestStack"
   set the defaultStack to "TestStack"
   
   create scrollbar "scr1"
   put "PASS: Default scrollbar created" & return
   
   put "Quitting..." & return
   delete stack "TestStack"
   quit 0
end testMinimalScrollbar
