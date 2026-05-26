on startup
   testMinimal
end startup

on testMinimal
   local tStack, tResult
   put "MinimalTest" into tStack

   if there is a stack tStack then
      delete stack tStack
   end if

   put "Creating invisible stack..." & return
   create invisible stack tStack
   set the defaultStack to tStack
   set the width of stack tStack to 400
   set the height of stack tStack to 400

   put "PASS: Stack created" & return

   put "Creating button..." & return
   create button "btn1"
   set the style of button "btn1" to "standard"
   set the label of button "btn1" to "Test"
   put "PASS: Button created" & return

   put "Creating field..." & return
   create field "fld1"
   set the text of field "fld1" to "Hello"
   put "PASS: Field created" & return

   put "Creating scrollbar..." & return
   create scrollbar "scr1"
   set the thumbPosition of scrollbar "scr1" to 50
   put "PASS: Scrollbar created" & return

   put "Cleaning up..." & return
   delete stack tStack
   put "PASS: Stack deleted" & return

   put "All minimal tests passed." & return
   quit 0
end testMinimal
