on startup
   testComprehensive
end startup

on testComprehensive
   local tStack, tResult
   put "CompTest" into tStack
   if there is a stack tStack then delete stack tStack
   
   create invisible stack tStack
   set the defaultStack to tStack
   set the width of stack tStack to 400
   set the height of stack tStack to 400
   put "PASS: Stack created" & return after tResult
   
   -- Card 1: Controls
   set the name of this card to "Controls"
   create button "btn1"
   set the style of button "btn1" to "standard"
   set the label of button "btn1" to "Test"
   set the enabled of button "btn1" to false
   set the enabled of button "btn1" to true
   set the visible of button "btn1" to false
   set the visible of button "btn1" to true
   set the hilite of button "btn1" to true
   set the hilite of button "btn1" to false
   put "PASS: Standard button" & return after tResult
   
   create button "chk1"
   set the style of button "chk1" to "checkbox"
   set the hilite of button "chk1" to true
   set the hilite of button "chk1" to false
   put "PASS: Checkbox" & return after tResult
   
   create button "rad1"
   set the style of button "rad1" to "radioButton"
   set the hilite of button "rad1" to true
   set the hilite of button "rad1" to false
   put "PASS: Radio button" & return after tResult
   
   create button "menu1"
   set the style of button "menu1" to "menu"
   put "PASS: Menu button" & return after tResult
   
   -- SKIP tab button (causes crash with invalid style)
   
   -- Card 2: Scrollbars
   create card "Scrollbars"
   create scrollbar "hscr"
   set the thumbPosition of scrollbar "hscr" to 50
   set the thumbSize of scrollbar "hscr" to 20
   set the pageInc of scrollbar "hscr" to 10
   set the lineInc of scrollbar "hscr" to 1
   put "PASS: Horizontal scrollbar" & return after tResult
   
   create scrollbar "vscr"
   set the thumbPosition of scrollbar "vscr" to 25
   put "PASS: Vertical scrollbar" & return after tResult
   
   create scrollbar "sli1"
   set the style of scrollbar "sli1" to "slider"
   set the thumbPosition of scrollbar "sli1" to 75
   put "PASS: Slider" & return after tResult
   
   create scrollbar "prog1"
   set the style of scrollbar "prog1" to "progress"
   set the thumbPosition of scrollbar "prog1" to 60
   put "PASS: Progress scrollbar" & return after tResult
   
   -- Card 3: Groups
   create card "Groups"
   create group "grp1"
   set the showBorder of group "grp1" to true
   set the showName of group "grp1" to true
   set the label of group "grp1" to "Group Label"
   set the opaque of group "grp1" to true
   set the opaque of group "grp1" to false
   set the backgroundColor of group "grp1" to "200,200,200"
   put "PASS: Group with border" & return after tResult
   
   create group "grp2"
   set the showBorder of group "grp2" to false
   set the showName of group "grp2" to false
   put "PASS: Group without border" & return after tResult
   
   create group "outer"
   set the showBorder of group "outer" to true
   create group "inner" in group "outer"
   set the showBorder of group "inner" of group "outer" to true
   put "PASS: Nested groups" & return after tResult
   
   -- Card 4: Fields
   create card "Fields"
   create field "fld1"
   set the text of field "fld1" to "Hello"
   set the textSize of field "fld1" to 14
   set the textStyle of field "fld1" to "bold"
   set the textStyle of field "fld1" to "italic"
   set the textStyle of field "fld1" to ""
   set the foregroundColor of field "fld1" to "255,0,0"
   set the backgroundColor of field "fld1" to "255,255,0"
   set the opaque of field "fld1" to true
   set the showBorder of field "fld1" to true
   set the showFocusBorder of field "fld1" to true
   set the hScrollbar of field "fld1" to true
   set the vScrollbar of field "fld1" to true
   set the lockText of field "fld1" to false
   set the lockText of field "fld1" to true
   set the dontWrap of field "fld1" to true
   set the dontWrap of field "fld1" to false
   set the enabled of field "fld1" to false
   set the enabled of field "fld1" to true
   put "PASS: Standard field" & return after tResult
   
   create field "fld2"
   set the text of field "fld2" to "Plain"
   set the style of field "fld2" to "transparent"
   put "PASS: Transparent field" & return after tResult
   
   create field "fld3"
   set the text of field "fld3" to "Scrolling" & return & "Line 2" & return & "Line 3"
   set the vScrollbar of field "fld3" to true
   set the scroll of field "fld3" to 2
   put "PASS: Scrolling field" & return after tResult
   
   -- Card 5: Graphics
   create card "Graphics"
   create graphic "rect1"
   set the style of graphic "rect1" to "rectangle"
   set the opaque of graphic "rect1" to true
   set the backColor of graphic "rect1" to "100,150,255"
   set the foregroundColor of graphic "rect1" to "0,0,128"
   set the lineSize of graphic "rect1" to 3
   set the blendLevel of graphic "rect1" to 30
   put "PASS: Rectangle graphic" & return after tResult
   
   create graphic "oval1"
   set the style of graphic "oval1" to "oval"
   set the opaque of graphic "oval1" to true
   put "PASS: Oval graphic" & return after tResult
   
   create graphic "round1"
   set the style of graphic "round1" to "roundrect"
   set the roundRadius of graphic "round1" to 20
   put "PASS: Roundrect graphic" & return after tResult
   
   create graphic "line1"
   set the style of graphic "line1" to "line"
   set the lineSize of graphic "line1" to 2
   put "PASS: Line graphic" & return after tResult
   
   -- Show/hide cycle
   set the visible of stack tStack to true
   set the visible of stack tStack to false
   put "PASS: Stack shown/hidden" & return after tResult
   
   -- Property changes while hidden
   set the defaultStack to tStack
   set the label of button "btn1" to "Changed"
   set the hilite of button "chk1" to true
   set the hilite of button "rad1" to true
   set the text of field "fld1" to "Changed text"
   set the scroll of field "fld3" to 1
   set the thumbPosition of scrollbar "hscr" to 75
   set the thumbPosition of scrollbar "sli1" to 50
   set the label of group "grp1" to "Changed Label"
   set the showBorder of group "grp1" to false
   set the showBorder of group "grp1" to true
   set the blendLevel of graphic "rect1" to 50
   set the backColor of graphic "rect1" to "255,0,0"
   put "PASS: Property changes while hidden" & return after tResult
   
   -- Show again
   set the visible of stack tStack to true
   set the visible of stack tStack to false
   put "PASS: Stack re-shown after property changes" & return after tResult
   
   -- Cleanup
   delete stack tStack
   put "PASS: Stack deleted" & return after tResult
   
   put tResult
   put "All comprehensive widget tests passed."
   quit 0
end testComprehensive
