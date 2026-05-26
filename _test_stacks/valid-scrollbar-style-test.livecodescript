on startup
   testValidScrollbarStyle
end startup

on testValidScrollbarStyle
   create stack "TestStack"
   set the defaultStack to "TestStack"
   
   create scrollbar "scr1"
   put "PASS: Default scrollbar created" & return
   
   set the style of scrollbar "scr1" to "scrollbar"
   put "PASS: Scrollbar style set" & return
   
   delete stack "TestStack"
   quit 0
end testValidScrollbarStyle
