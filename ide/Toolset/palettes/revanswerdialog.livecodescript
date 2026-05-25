script "Answer Dialog"

// Scriptified 2026.05.21 by Mark Wieder mwieder@ahsoftware.net

constant kIconTypes="information error warning question"
constant kMacIcons="210093 210092 210094 210097"
constant kWinIcons="210005 210011 210003 210004"
constant kLinuxIcons="210005 210011 210003 210004"

on preOpenStack
   global gAPKLargeFonts, gREVSmallAppIcon, gREVAppIcon
   local tIcons, tMacOSX, tMargin, tNumber, tIconWord, tIconAllowance
   local tButtonNames, tNoOfButtons, tTotalBtnWidth, tMnemonicString, tNo
   local tFieldSubtract, t, tWidth, tNewWidth, tAddition
   local tStartEdge, tHeight
   
   // HXT dark mode
   set the backgroundColor of me to (the systemWindowColor)
   set the foregroundColor of me to (the systemTextColor)
   
   if there is not a button "icon" of me then
      create button "icon" in this card
      set the style of it to "transparent"
      set the showName of it to false
      set the autoHilite of it to false
      set the sharedHilite of it to false
      set the traversalOn of it to false
      set the width of it to 64
      set the height of it to 64
   end if
   
   /* Creates the prompt field if there is not one already created */
   if the number of fields of me is 0 then
      create field "prompt" in this card
      set fixedLineHeight of it to false
      set sharedText of it to true
      set dontWrap of it to false
      set lockText of it to true
      set opaque of it to false
      set traversalOn of it to false
      set showBorder of it to false
      set vScrollbar of it to true
      set width of it to 293
      set height of it to 342
      set the fixedLineHeight of it to false
   end if
   
   /* Creates Icon 2 for the dialog - Secondary icon used in MacOS (currently
   * broken) */
   if there is not a button "icon 2" of me then
      create button "icon 2" in this card
      set the style of it to "transparent"
      set the showName of it to false
      set the autoHilite of it to false
      set the sharedHilite of it to false
      set the traversalOn of it to false
      set the showFocusBorder of it to false
      set the width of it to 47
      set the height of it to 38
   end if
   
   if there is not a button "OK" of me then
      create button "OK" in this card
      set the style of it to "standard"
      set the showName of it to true
      set the autoHilite of it to true
      set the sharedHilite of it to false
      set the traversalOn of it to false
      set the width of it to 64
      set the height of it to 64
   end if
   
   // AL-2014-05-21: [[ Bug 12074 ]] Tidied up and rearranged to support 
   //  RTL layout, as determined by item 5 of the dialogData
   
   --platform specific font adjustments
   set the default of button "OK" to false
   set the bottomRight of button "icon 2" to -100,-100
   
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
         put true into tMacOSX
         set the loc of this stack to tStackLoc
         set the top of this stack to item 2 of tScreenRect + round(tScreenHeight/5)
         repeat with i = 3 to the number of btns
            set the margins of button i of me to 4
            set the height of button i of me to 23
         end repeat
         set the textFont of this stack to "Lucida Grande"
         set the textSize of this stack to 13
         set the textSize of field 1 of me to 13
         put kMacIcons into tIcons
         break
      case "Win32"
         if gAPKLargeFonts then
            put 26 into tNumber
            put 7 into tMargin
         else
            put 21 into tNumber
            put 5 into tMargin
         end if
         repeat with i = 3 to the number of btns
            set the bottomMargin of button i of me to tMargin
            set the height of button i of me to tNumber
            set the traversalOn of button i of me to false
            set the mnemonic of button i of me to 0
         end repeat
         set the textFont of this stack to empty
         set the textSize of this stack to empty
         
         set the textFont of field 1 of me to empty
         put kWinIcons into tIcons
         break
      default
         repeat with i = 3 to the number of btns
            set the bottomMargin of button i of me to 4
            set the height of button i of me to 22
            set the traversalOn of button i of me to false
            set the mnemonic of button i of me to 0
         end repeat
         set the textFont of this stack to "Helvetica"
         if gAPKLargeFonts then
            set the textSize of this stack to 14
         else
            set the textSize of this stack to 12
         end if
         set the textFont of field "prompt" of me to empty
         put kLinuxIcons into tIcons
         break
   end switch
   
   set the itemDelimiter to numToChar(0)
   
   local tRTL
   get item 5 of the dialogdata 
   if it is empty then
      put false into tRTL
   else
      put it is true into tRTL
   end if
   
   -- field alignment
   if tRTL then
      set the textAlign of field "prompt" of me to "right"
   else
      set the textAlign of field "prompt" of me to "left"
   end if
   
   --is there an icon to display or not?
   local tHasIcon, tIconX, tIconTop
   if word 2 of item 1 of the dialogData is "plain" and (not tMacOSX and gREVAppIcon is not empty) then
      set the loc of button "icon" of me to -100,21 -- offscreen
      put 0 into tIconAllowance
      put false into tHasIcon
   else
      put true into tHasIcon
      if tMacOSX then
         put 87 into tIconAllowance
         put 52 into tIconX
         put 18 into tIconTop
      else
         put 40 into tIconAllowance
         put 26 into tIconX
         put 6 into tIconTop
      end if
      
      put wordOffset(word 2 of item 1 of the dialogData, "information error warning question") into tIconWord
      if tMacOSX and tIconWord is 0 then --large app icon
         set the icon of button "icon" of me to gREVAppIcon
      else
         set the icon of button "icon" of me to word tIconWord of tIcons
      end if
   end if
   
   --title
   local tText
   if item 2 of the dialogData is empty then
      set the title of this stack to space
   else
      set the title of this stack to item 2 of the dialogData
   end if
   put item 3 of the dialogData into tText
   if "<p>" is in tText or matchChunk(tText, "<(.+>.+</.+)>") then
      set the htmlText of field "prompt" of me to tText
   else
      set the text of field "prompt" of me to tText
   end if
   
   --buttons
   
   # TH-2008-02-07 :: Bug 6427, answer dialog will display all buttons 
   # specified up to tButtonLimit
   local tButtonNamesList, tButtonLimit
   put item 4 of dialogData into tButtonNamesList
   --put 7 into tButtonLimit
   constant kButtonLimit=7
   repeat with x = 1 to min(kButtonLimit, the number of lines of tButtonNamesList)
      put line x of tButtonNamesList & cr after tButtonNames
   end repeat
   delete the last char of tButtonNames  
   
   if tButtonNames is empty then
      put "OK" into tButtonNames
   end if
   put the number of lines of tButtonNames into tNoOfButtons
   
   put 32 into tTotalBtnWidth
   --truncate more buttons to the maximum allowed (7)
   if tNoOfButtons > the number of buttons - 2 then
      put the number of buttons - 2 into tNoOfButtons
   end if
   put empty into tMnemonicString
   repeat with i = 3 to (tNoOfButtons + 3)
      if there is no button i of me then
         create button i in this card
         set the name of it to "hello" && i
         set the style of it to "transparent"
      end if
      set the name of button i of me to line tNoOfButtons - (i - 3) of tButtonNames
      if the platform is "Win32" and the short name of button i of me is not in "OKCancel" then
         put 0 into tNo
         repeat for each char x in the short name of button i of me
            add 1 to tNo
            if x is not in tMnemonicString then
               put x after tMnemonicString
               set the mnemonic of button i of me to tNo
               exit repeat
            end if
         end repeat
      end if
      set the width of button i of me to 82
      if the width of button i of me < the formattedWidth of button i of me +28 then
         set the width of button i of me to the formattedWidth of button i of me + 28
      end if
      add (the width of button i) + 12 to tTotalBtnWidth
   end repeat
   set the dontWrap of field "prompt" of me to true
   if tMacOSX then
      put 4 into tFieldSubtract
   else
      put 0 into tFieldSubtract
   end if
   
   put max(400, max(the formattedWidth of field 1 of me + tIconAllowance + 16, tTotalBtnWidth) + tFieldSubtract) into tWidth
   
   if tWidth > 400 then
      -- The nonwrapped text does not fit in a minimal message box, thus allow to wrap, recalculating the width.
      set the width of field 1 of me to 1
      set the dontWrap of field 1 of me to false
      put max(400, max(the formattedWidth of field 1 of me + tIconAllowance + 16, tTotalBtnWidth) + tFieldSubtract) into tWidth
   end if
   
   --put the width of this stack into oldwidth
   set the width of this stack to tWidth
   
   --set icon position in light of possible new stack width
   if tHasIcon then
      if tRTL then
         put tWidth - tIconX into tIconX
      end if
      set the loc of button "icon" of me to tIconX,0
      set the top of button "icon" of me to tIconTop
   end if
   
   set the itemDel to comma
   if tRTL then      
      set the topRight of field 1 of me to tWidth - (tIconAllowance + 8), 12
      get the rect of field 1 of me
      put 12 into item 1 of it
   else
      set the topLeft of field 1 of me to tIconAllowance + 8, 12
      get the rect of field 1 of me
      put tWidth - 12 into item 3 of it
   end if
   set the rect of field 1 of me to it
   
   if tMacOSX then
      put 5 into tAddition
   else
      put 0 into tAddition
   end if
   
   if tRTL then
      put 12 + tAddition into tStartEdge
   else
      put tWidth -12 - tAddition into tStartEdge
   end if
   
   -- [[ Bug 17001]] Ensure the dialog is tall enough for the icon + margin
   put the formattedHeight of field 1 of me + 58 into tHeight
   if tHasIcon then
      put max(tHeight, the bottom of button "icon" + tIconTop) into tHeight
   end if
   
   if tHeight > 400 then
      set the vScrollbar of field 1 of me to true
      put 400 into tHeight
   else
      set the vScrollbar of field 1 of me to false
   end if
   local tTop
   put the top of field 1 of me into tTop
   set the height of this stack to tHeight
   set the height of field 1 of me to tHeight - 58
   set the top of field 1 of me to tTop
   
   // AL-2014-05-21: [[ Bug 11493 ]] Linux confirmation button should be on the right
   if the platform is in "MacOSLinux" then
      if tMacOSX then
         put 6 into tAddition
      else
         put 0 into tAddition
      end if
      repeat with i = 3 to tNoOfButtons + 2
         if there is a button i of me then
            if tRTL then
               set the left of button i of me to tStartEdge
               add the width of button i of me + 10 to tStartEdge
            else
               set the right of button i of me to tStartEdge        
               subtract the width of button i of me + 10 from tStartEdge
            end if
            
            set the bottom of button i of me to the height of this stack - 12 - tAddition
            if i is 5 and tNoOfButtons is 3 and word 1 of the short name of button i of me is "Don't" then
               --move don't save to the side
               if tRTL then
                  set the right of button i of me to the right of field 1 of me - 5
               else
                  set the left of button i of me to the left of field 1 of me + 5
               end if
            end if
         end if
      end repeat
   else
      repeat with i = (tNoOfButtons + 2) down to 3
         if there is a button i of me then
            if tRTL then
               set the left of button i of me to tStartEdge
               add the width of button i of me + 12 to tStartEdge
            else
               set the right of button i to tStartEdge        
               subtract the width of button i of me + 12 from tStartEdge
            end if
            set the bottom of button i of me to the height of this stack - 12
         end if
      end repeat
   end if
   if the platform is "MacOS" then
      set the top of this stack to round(item 4 of the screenRect/5)
   end if
   
   -- Provide keyboard navigation on Linux and Windows but not OSX
   if the platform is not "MacOS" then
      -- Make sure that we don't make the icon buttons navigable
      repeat with i = (tNoOfButtons + 2) down to 3
         set the traversalOn of button i of me to true
      end repeat
   end if
   
   set the hiliteColor of me to the hilitecolor
	set the visible of me to true
end preOpenStack

on openCard
	select the text of field 1 of me
    set the clipboarddata["text"] to field 1 of me
end openCard

on mouseDown
end mouseDown

on commandKeyDown pKey
   if pKey is "." then
      if there is a button "Cancel" of me then
         click at the loc of button "Cancel" of me
         exit commandKeyDown
      end if
   end if
   repeat with i = 3 to the number of buttons of me
      if item 1 of the loc of button i of me < 1 then
         next repeat
      end if
      if char 1 of the short name of button i of me is pKey then
         set the hilite of button i of me to true
         wait 50 milliseconds
         set the hilite of button i of me to false
         dispatch "mouseUp" to button i of me
         exit repeat
      end if
   end repeat
end commandKeyDown

on escapeKey
   if there is a button "Cancel" of me then
      click at the loc of button "Cancel" of me
   end if
end escapeKey

on closeStack
   revCleanUpAnswerDialog
end closeStack

on revCleanUpAnswerDialog
   --show invisibles compatible removal of btns
   repeat with i = 3 to the number of buttons
      set the cPrevLoc of button i to the loc of button i of me
      set the loc of button i of me to -100,-100
   end repeat
   set the dialogData to empty
   
   if the environment is "development" then
      send "ideMessageSend" && "ideAnswerDialogClosed" to stack "revIDELibrary" in 0 millisecs
   end if
end revCleanUpAnswerDialog

on mouseUp
  if the name of the target contains "button" then
    lock messages--prevents user stack getting refocusing messages
    close this stack
    revCleanUpAnswerDialog--won't get closeStack
    unlock messages
    set the dialogData to the short name of the target
  end if
end mouseUp

on returnKey
  pass returnKey to top
end returnKey

on linkClicked pLink
   if "@" is in pLink then
      revMail pLink
   else
      revGoURL pLink
   end if
end linkClicked

on SystemAppearanceChanged pMode, pWindowColor, pTextColor
   set the backgroundColor of me to pWindowColor
   set the foregroundColor of me to pTextColor
end SystemAppearanceChanged

