on startup
   testCardScrollbar
end startup

on testCardScrollbar
   local tStack
   put "CardScrollbarTest" into tStack
   if there is a stack tStack then delete stack tStack
   
   create invisible stack tStack
   set the defaultStack to tStack
   
   set the name of this card to "Controls"
   create button "btn1"
   put "PASS: Button on Controls" & return
   
   create card "Scrollbars"
   put "PASS: Scrollbars card created" & return
   
   create scrollbar "hscr"
   put "PASS: Scrollbar on Scrollbars card" & return
   
   delete stack tStack
   put "PASS: Done" & return
   quit 0
end testCardScrollbar
