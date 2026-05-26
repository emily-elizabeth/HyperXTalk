on startup
   testMinimalCrash
end startup

on testMinimalCrash
   local tStack, tResult
   put "MinimalCrashTest" into tStack
   if there is a stack tStack then delete stack tStack
   
   create invisible stack tStack
   set the defaultStack to tStack
   
   create button "btn1"
   set the style of button "btn1" to "standard"
   put "PASS: Standard button" & return after tResult
   
   create button "chk1"
   set the style of button "chk1" to "checkbox"
   put "PASS: Checkbox" & return after tResult
   
   create button "rad1"
   set the style of button "rad1" to "radioButton"
   put "PASS: Radio button" & return after tResult
   
   create scrollbar "hscr"
   put "PASS: Scrollbar" & return after tResult
   
   create group "grp1"
   put "PASS: Group" & return after tResult
   
   create field "fld1"
   put "PASS: Field" & return after tResult
   
   create graphic "rect1"
   put "PASS: Graphic" & return after tResult
   
   put tResult
   quit 0
end testMinimalCrash
