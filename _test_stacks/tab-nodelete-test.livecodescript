on startup
   testTabNoDelete
end startup

on testTabNoDelete
   local tStack
   put "TabTestNoDelete" into tStack
   if there is a stack tStack then delete stack tStack
   
   create invisible stack tStack
   set the defaultStack to tStack
   
   put "Creating button..." & return
   create button "tab1"
   put "PASS: Button created" & return
   
   put "Setting style to tab..." & return
   set the style of button "tab1" to "tab"
   put "PASS: Tab style set" & return
   
   put "Not deleting stack, just quitting..." & return
   quit 0
end testTabNoDelete
