on startup
   testInvalidStyle
end startup

on testInvalidStyle
   local tStack
   put "InvalidStyleTest" into tStack
   if there is a stack tStack then delete stack tStack
   
   create invisible stack tStack
   set the defaultStack to tStack
   
   put "Creating button..." & return
   create button "btn1"
   put "PASS: Button created" & return
   
   put "Setting invalid style..." & return
   set the style of button "btn1" to "foobar"
   put "PASS: Invalid style set (or error ignored)" & return
   
   quit 0
end testInvalidStyle
