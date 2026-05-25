script "revfileassociations.livecodescript"

-- create a Yes and No button
-- create a checkbox "Do not show this dialog again"
-- create a label field

constant kGroup = "Associations"
-- set the field contents
constant kText = "HyperXTalk files (those with extension .hyperxtalk, .livecodescript and .rev) are not currently associated with this version. Would you like to make this association?"
constant kNoShow = "Don't show this dialog again"

on preOpenStack
   revInternal__ConstrainStack the short name of me
   set the width of me to 426
   set the height of me to 113
   set the resizable of me to false
   set the destroyStack of me to true
   
   -- Make sure we use the screen of the menubar
   get line (the screen of stack "revMenubar") of the working screenRects
   set the loc of me to (item 3 of it + item 1 of it) div 2, (item 4 of it + item 2 of it) div 2
   
   lock messages

   create field "Documentation"
   set the locktext of it to true
   set the width of it to 426
   set the height of it to 55
   set the topleft of it to 0,5
   
   create button "Show"
   set the style of it to "checkbox"
   set the topleft of it to 5,84
   --set the top of button "Show" to the bottom of field "Documentation"
   
   set the width of the templatebutton to 82
   set the height of the templatebutton to 23
   set the style of the templatebutton to "standard"
   
   create button "Yes"
   create button "No"
   set the top of button "Yes" to the top of button "Show"
   set the top of button "No" to the top of button "Show"
   reset the templatebutton
   unlock messages
end preOpenStack

on OpenCard
   local tTopLeft
   set the label of button "Show" to kNoShow
   put the topLeft of button "Show" into tTopLeft
   set the width of button "Show" to the formattedWidth of button "Show"
   set the topLeft of button "Show" to tTopLeft
   
   set the text of field "Documentation" to kText
   
   set the left of button "Yes" to the right of button "Show" + 30
   set the left of button "No" to the right of button "Yes"
   --respect the HIG for the OS
   local tOldLoc
   switch (the platform)
      case "linux"
      case "macos"
         if the left of button "Yes" < the left of button "No" then
            put the loc of button "Yes" into tOldLoc
            set the loc of button "Yes" to the loc of button "No"
            set the loc of button "No" to tOldLoc
         end if
         break
      case "win32"
         if the left of button "No" < the left of button "Yes" then
            put the loc of button "No" into tOldLoc
            set the loc of button "No" to the loc of button "Yes"
            set the loc of button "Yes" to tOldLoc
         end if
         break
      default
         break
   end switch
   
   set the hilite of button "Show" to (the cDontShowFileAssociationsDialog of stack "revPreferences" is true)
   set the cantModify of me to true
   set the visible of me to true
end OpenCard

on answerQuestion pAnswer
  if pAnswer is "Yes" then
    revInternal__InstallFileAssociations
  end if
  close this stack
end answerQuestion

--button "Yes" or "No"
on mouseUp
   if "button" is word 1 of the name of the target then
      if "Show" is word 2 of the name of the target then
         # We need this done as soon as the user clicks the button. In case they close the window for example.
         set the cDontShowFileAssociationsDialog of stack "revPreferences" to the hilite of me
      else
         answerQuestion the label of me
      end if
   end if
end mouseUp
