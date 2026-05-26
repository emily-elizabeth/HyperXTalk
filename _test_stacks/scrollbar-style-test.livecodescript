on startup
   testScrollbarStyles
end startup

on testScrollbarStyles
   create stack "TestStack"
   set the defaultStack to "TestStack"
   
   create scrollbar "scr1"
   put "PASS: Default scrollbar created" & return
   
   set the style of scrollbar "scr1" to "slider"
   put "PASS: Slider style set" & return
   
   delete stack "TestStack"
   quit 0
end testScrollbarStyles
