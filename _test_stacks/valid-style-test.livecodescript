on startup
   testValidStyles
end startup

on testValidStyles
   local tStack
   put "ValidStyleTest" into tStack
   if there is a stack tStack then delete stack tStack
   
   create invisible stack tStack
   set the defaultStack to tStack
   
   put "Creating button..." & return
   create button "btn1"
   put "PASS: Button created" & return
   
   put "Setting checkbox style..." & return
   set the style of button "btn1" to "checkbox"
   put "PASS: Checkbox style set" & return
   
   put "Setting radioButton style..." & return
   set the style of button "btn1" to "radioButton"
   put "PASS: RadioButton style set" & return
   
   put "Setting standard style..." & return
   set the style of button "btn1" to "standard"
   put "PASS: Standard style set" & return
   
   quit 0
end testValidStyles
