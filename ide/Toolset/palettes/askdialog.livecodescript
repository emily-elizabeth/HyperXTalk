script "Ask Dialog"

// scriptified 2026.05.21 by mark wieder
mwieder@ahsoftware.net

private command doresize
   // AL-2014-05-21: [[ Bug 12074 ]] Support RTL layout when resizing ask dialog
   local tIsRightToLeft
   # This needs to go into revPreferences mdw
   --put the cRTL of this card into tIsRightToLeft
   put the cRTL of stack "revPreferences" into tIsRightToLeft
   
   local twidth, iwidth
   put the width of this stack into twidth
   if item 1 of the loc of button "icon" is not "-100" then 
      local tIconEdge
      if tIsRightToLeft then
         put the left of button "icon" into tIconEdge
         subtract 8 from tIconEdge
      else
         put the right of button "icon" into tIconEdge
         add 8 to tIconEdge
      end if
      subtract the width of button "icon" from tWidth
   else       
      if tIsRightToLeft then
         put twidth - 24 into tIconEdge
      else
         put 24 into tIconEdge
      end if
   end if
   set the width of field "prompt" to twidth - 48
   set the width of field "input" to twidth - 48
   
   if tIsRightToLeft then
      set the right of field "prompt" to tIconEdge
      set the right of field "input" to tIconEdge
   else
      set the left of field "prompt" to tIconEdge
      set the left of field "input" to tIconEdge
   end if
   set the top of field "input" to the bottom of field "prompt" + 4
   set the top of button "icon" to the height of field "prompt" div 2 - 8
   set the default of button "OK" to false
   
   local tFirstWidthOffset, tFirstBottomOffset, tSecondLeftOffset, tSecondBottomOffset
   local tFirst, tSecond, tFirstXOffset, tFirstYOffset, tSecondXOffset, tSecondYOffset
   put 21 into tFirstXOffset
   put 18 into tFirstYOffset
   put 14 into tSecondXOffset
   put 18 into tSecondYOffset
   
   set the textFont of button "OK" to "usesystemfont"
   set the textSize of button "OK" to 13
   set the textFont of button "Cancel" to "usesystemfont"
   set the textSize of button "Cancel" to 13
   
   // AL-2014-05-21: [[ Bug 11493 ]] Linux confirmation button should be on the right
   if the platform is in "MacOSLinux" then
      put "OK" into tFirst
      put "Cancel" into tSecond
   else
      put "Cancel" into tFirst
      put "OK" into tSecond
   end if
   set the default of button "OK" to true
   
   if tIsRightToLeft then
      set the bottomLeft of button tFirst to (tFirstXOffset, the bottom of this card - tFirstYOffset)
      set the bottomLeft of button tSecond to (the right of button tFirst + tSecondXOffset, the bottom of this card - tSecondYOffset)
   else
      set the bottomRight of button tFirst to (the width of this card - tFirstXOffset, the bottom of this card - tFirstYOffset)
      set the bottomRight of button tSecond to (the left of button tFirst - tSecondXOffset, the bottom of this card - tSecondYOffset)
   end if
end doresize

on star
   local tstring
   put empty into tstring
   repeat with i = 1 to the number of characters in field "passwordtext"
      put "*" after tstring
   end repeat
   put tstring into field "input"
end star

constant kIconTypes="information error warning question"
constant kMacIcons="210093 210092 210094 210097"
constant kWinIcons="210005 210011 210003 210004"
constant kLinuxIcons="210005 210011 210003 210004"

on dialogInitialize
   lock messages
   
   set the showName of the templateButton to false
   set the borderWidth of the templateButton to 0
   set the showBorder of the templateButton to false
   set the threed of the templateButton to false
   set the opaque of the templateButton to false
   set the width of the templateButton to 64
   set the height of the templateButton to 64
   set the visible of the templateButton to true
   
   // create button "icon"
   if there is no button "icon" of me then
      create button "icon"
      set the width of it to 76
      set the height of it to 65
   end if
   
   // create button "icon 2"
   if there is no button "icon 2" of me then
      create button "icon 2"
   end if
   
   reset the templateButton
   
   // create button "OK"
   if there is no button "OK" of me then
      create button "OK"
      set the width of it to 73
      set the script of it to "on mouseUp"&cr&tab&"OKButton"&cr&"end mouseUp"
   end if
   
   // create button "Cancel"
   if there is no button "Cancel" of me then
      create button "Cancel"
      set the width of it to 73
      set the script of it to "on mouseUp"&cr&tab&"CancelButton"&cr&"end mouseUp"
   end if
   
   set the width of the templateField to 326
   set the showBorder of the templateField to false
   set dontSearch of the templateField to true
   set the height of the templateField to 25
   
   // create field 1
   if there is no field "prompt" of me then
      create field "prompt"
      set the locktext of it to true
      set the opaque of it to false
      set the traversalon of it to false
      set the sharedtext of it to true
      
      set showBorder of it to false
      set dontSearch of it to true
      set the width of it to 326
      set the height of it to 25
   end if
   
   // create field 2
   if there is no field "input" of me then
      create field "input"
      set opaque of it to true
      set dontWrap of it to true
      
      set sharedText of it to true
      set lockText of it to false
      set traversalOn of it to true
      set autoTab of it to true
      set showBorder of it to true
      set dontSearch of it to true
      --set the script of it to "on enterInField"&cr&tab&"closeAccept"&cr&"end enterInField"
   end if
   
   // create invisible field 3 for passwords
   if there is no field "passwordtext" of me then
      create invisible field "passwordtext"
      set the width of it to 124
      set the height of it to 28
   end if
   
   reset the templateField
   unlock messages
end dialogInitialize

on preOpenStack
   // HXT dark mode
   set the backgroundColor of me to (the systemWindowColor)
   set the foregroundColor of me to (the systemTextColor)
   
   dialogInitialize
   
   -- PM-2015-07-23: [[ Bug 14224 ]] Remove the (old) focus to allow typing into the input fld
   focus on nothing
   
   global gREVAppIcon
   local twidth
   set the itemDelimiter to numToChar(0)
   set the fieldmode of me to word 2 of item 1 of the dialogData
   if item 2 of the dialogData is empty then
      set the title of this stack to space
   else
      set the title of this stack to item 2 of the dialogData
   end if
   
   -- MW-2007-12-11: [[ Bug 4974 ]] Support the use of htmlText in the prompt
   local tText
   put item 3 of the dialogData into tText
   if "<p>" is in tText or matchChunk(tText, "<(.+>.+</.+)>") then
      set the htmlText of field "prompt" of me to tText
   else
      put item 3 of the dialogData into field "prompt" of me
   end if
   
   set the itemDel to comma
   set the icon of button "icon 2" of me to 0
   set the loc of button "icon 2" of me to -200,-200
   
   local tStackLoc
   local tScreenRect
   put line (the screen of this stack) of the screenRects into tScreenRect
   
   local tScreenWidth
   put item 3 of tScreenRect - item 1 of tScreenRect into tScreenWidth
   
   local tScreenHeight
   put item 4 of tScreenRect - item 2 of tScreenRect into tScreenHeight
   
   put item 1 of tScreenRect + (tScreenWidth div 2),item 2 of tScreenRect + (tScreenHeight div 2) into tStackLoc
   
   switch the platform
      case "MacOS"
         local tMacOSX
         put true into tMacOSX
         set the loc of this stack to tStackLoc
         set the top of this stack to item 2 of tScreenRect + round(tScreenHeight/5)
         repeat with i = 3 to the number of buttons of me
            set the margins of button i of me to 4
            set the height of button i of me to 23
         end repeat
         set the textFont of this stack to "Lucida Grande"
         set the textSize of this stack to 13
         set the textFont of field "input" of me to empty
         set the textSize of field "input" of me to 13
         local tIcons
         put kMacIcons into tIcons
         break
      case "Win32"
         
         # OK-2007-11-05 : gREVLargeFonts was undeclared...
         global gREVLargeFonts
         if gREVLargeFonts then
            local tNumber
            put 26 into tNumber
            
            local tMargin
            put 7 into tMargin
         else
            put 23 into tNumber
            put 3 into tMargin
         end if
         repeat with i = 3 to the number of buttons of me
            set the bottomMargin of button i of me to tMargin
            set the height of button i of me to tNumber
            set the traversalOn of button i of me to false
            set the mnemonic of button i of me to 0
         end repeat         
         set the textFont of this stack to empty
         set the textSize of this stack to empty
         
         set the textFont of field "input" of me to empty
         put kWinIcons into tIcons
         break
      default
         repeat with i = 3 to the number of buttons
            set the bottomMargin of button i of me to 4
            set the height of button i of me to 22
            set the traversalOn of button i of me to false
            set the mnemonic of button i of me to 0
         end repeat
         set the textFont of this stack to "Helvetica"
         
         # OK-2007-11-05 : gAPKLargeFonts was undeclared...
         global gAPKLargeFonts
         
         if gAPKLargeFonts then 
            set the textSize of this stack to 14
         else 
            set the textSize of this stack to 12
         end if
         set the textFont of field "input" of me to empty
         put kLinuxIcons into tIcons
         break
   end switch
   
   # TH-11-01-2008: Bug fix for 4913, need to set the field size after the font sizes have been
   # adjusted to prevent clipping of text
   put max(the formattedWidth of field "prompt" of me + 48,\\
         the formattedWidth of field "input" of me + 64, 450) into twidth
   set the width of this stack to twidth
   set the height of field "prompt" to the formattedHeight of field "prompt" of me -- + 25
   set the top of field "prompt" of me to 8
   set the height of this stack to (the height of field "prompt" of me) + 88
   
   set the itemDel to numToChar(0)
   if the fieldmode of me is "password" or the fieldmode of me is "clear" then
      put item 4 of the dialogData into field "passwordtext" of me
      star
   else
      put item 4 of the dialogData into field "input" of me
   end if
   
   // AL-2014-05-21: [[ Bug 12074 ]] Support RTL layout for ask dialog
   local tIsRightToLeft
   get item 5 of the dialogdata 
   if it is empty then
      put false into tIsRightToLeft
   else
      put it is "true" into tIsRightToLeft
   end if
   
   -- field alignment
   if tIsRightToLeft then
      set the textAlign of field "input" of me to "right"
      set the textAlign of field "prompt" of me to "right"
   else
      set the textAlign of field "input" of me to "left"
      set the textAlign of field "prompt" of me to "left"
   end if
   
   set the cRTL of stack "revPreferences" to tIsRightToLeft
   
   local tIconX, tIconY
   if word 2 of item 1 of the dialogData is "plain" and (not tMacOSX and gREVAppIcon is not empty) then
      set the loc of button 1 of me to -100,21
   else
      if tMacOSX then
         put 52 into tIconX
         put 44 into tIconY
      else
         put 26 into tIconX
         put 33 into tIconY
      end if
      
      local tIconWord
      put wordOffset(word 2 of item 1 of the dialogData, kIconTypes) into tIconWord
      if tMacOSX and tIconWord is 0 then --large app icon
         set the icon of button "icon" to gREVAppIcon
      else
         set the icon of button "icon" to word tIconWord of tIcons
      end if
      
      // AL-2014-05-21: [[ Bug 4001 ]] Make sure icon button has autohilite false
      set the autohilite of button "icon" to false
      
      if tIsRightToLeft then
         put tWidth - tIconX into tIconX
      end if
      set the loc of button "icon" to tIconX, tIconY
   end if
   
   --focus on field "input" of me
   set the dialogData to numToChar(0)
   doresize
   set the visible of me to true
end preOpenStack

on keyDown which
   local tpos
   if the name of the target contains "field" and word 1 of the fieldmode of me is "password" or the fieldmode of me is "clear" then
      put the selectedChunk into tpos
      put which into character (word 2 of tpos) to (word 4 of tpos) of field "passwordtext"
      star
      select after character (word 2 of tpos) of field "input"
   else
      pass keyDown
   end if
end keyDown

on deleteKey
   -- PM-2015-07-24: [[ Bug 3962 ]] Pass deleteKey when 'ask plain/question/information/warning/error ..'
  if the fieldmode of me is "password" or the fieldmode of me is "clear" then
      if word 2 of the selectedchunk > word 4 of the selectedchunk then
         deleteone "forward"
      else
         # TH-2008-06-23 :: Bug 6491
         local tChunk
         put the selectedChunk into tChunk
         deleteChunk tChunk
      end if
   else 
      pass deleteKey
   end if
end deleteKey

# OK-2007-11-05 : Bug 3956
on escapeKey
   cancelButton
end escapeKey

on backspaceKey
   if the fieldmode of me is "password" or the fieldmode of me is "clear" then
      if word 2 of the selectedchunk > word 4 of the selectedchunk then
         deleteone "back"
      else
         # TH-2008-06-23 :: Bug 6491
         local tChunk
         put the selectedChunk into tChunk
         deleteChunk tChunk
      end if
   else 
      pass backSpaceKey
   end if
end backspaceKey
  
# TH-2008-06-23 :: Bug 6491, make the delete and backspace keys delete hilited text
private command deleteChunk pChunk
   local tStartChar, tEndChar
   put word 2 of pChunk into tStartChar
   put word 4 of pChunk into tEndChar
   put empty into char tStartChar to tEndChar of field "passwordtext"
   star
   select before char tStartChar of field "input"     
end deleteChunk
  
private command deleteone pDirection
   local tpos
   put word 2 of the selectedChunk into tpos
   
   if pDirection is "back" then    -- backSpace key is pressed
      put empty into character (tpos - 1) of field "passwordtext" of me
      star
      select before character (tpos - 1) of field "input" of me
   else if pDirection is "forward" then     -- delete key is pressed
      put empty into character (tpos) of field "passwordtext" of me
      star
      select before character (tpos) of field "input" of me
   end if
end deleteone

on commandKeyDown which
   if which is "." then
      #click at the loc of button "Cancel"
      closeReject
   else
      if the fieldmode of me is not "plain" then
         local tpos
         switch which
            case "D"
               put word 2 of the selectedChunk into tpos
               put empty into character tpos of field "passwordtext"
               star
               select before character tpos of field "input"
               break
            case "H"
               deleteone "back"
               break
            case "V"
               if the clipboard is "text" then
                  local tSelectedChunk
                  put the selectedChunk into tSelectedChunk
                  put the clipboardData["text"] \
                        into character (word 2 of tSelectedChunk) to (word 4 of tSelectedChunk) \
                        of field 3
                  star
                  local tPastedLength
                  put the number of chars in the clipboardData["text"] into tPastedLength
                  if word 2 of tSelectedChunk > word 4 of tSelectedChunk then
                     select after character (word 4 of tSelectedChunk + tPastedLength) \
                           of field 2
                  else
                     select after character (word 2 of tSelectedChunk + tPastedLength - 1) \
                           of field 2
                  end if
               end if
               break
            default
               pass commandKeyDown
         end switch
      else
         pass commandKeyDown
      end if
   end if
end commandKeyDown

on openField
   --select the text of the target
   --focus on the target
end openField

on resizeStack
   doresize
end resizeStack

on returnKey
   closeAccept
end returnKey

on returnInField
   closeAccept
end returnInField

on enterInField
   closeAccept
end enterInField

on closeStack
   cleanUpAskDialog
end closeStack

private command cleanUpAskDialog
   put empty into field "prompt"
   put empty into field "input"
   put empty into field "passwordtext"
end cleanUpAskDialog

# OK-2007-11-05 : Bug 3956

command closeAccept
   local tResult
   switch the fieldmode of me
      case "plain"
      case "question"
      case "error"
      case "information"
      case "warning"
      default
         put the text of field "input" of me into tResult
         break
      case "password"
      case "clear"
         put the text of field "passwordtext" of me into tResult
         break
   end switch
   
   set the dialogData to tResult
   closeReject
end closeAccept

private command closeReject
  lock messages
  close this stack
  cleanUpAskDialog
  unlock messages
end closeReject

on SystemAppearanceChanged pMode, pWindowColor, pTextColor
   set the backgroundColor of me to pWindowColor
   set the foregroundColor of me to pTextColor
end SystemAppearanceChanged

# TODO: this works but has an unfortunate side effect:
# normal keystrokes work here but action keys (backspace, return, etc)
# are not captured by the input field even though it has focus.
# Not sure why... seems to be an issue with script-only stacks.
# Seems less confusing not to do this at this time.
on openCard
   --focus on field "input" of me
   --select the text of field "input" of me #mikey h4x0rz t3h IDE
end openCard

on preOpenCard
end preOpenCard

on closeCard
end closeCard

on openStack
end openStack

on mouseDown
end mouseDown

on cancelButton
   set the dialogData to empty
   closeReject
end cancelButton

on OKButton
  closeAccept
end OKButton
