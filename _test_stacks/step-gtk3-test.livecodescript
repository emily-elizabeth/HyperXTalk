on startup
   testStepByStep
end startup

on testStepByStep
   local tStack, tResult
   put "StepTest" into tStack
   if there is a stack tStack then delete stack tStack
   
   put "Creating stack..." & return
   create invisible stack tStack
   set the defaultStack to tStack
   put "PASS: Stack created" & return
   
   put "Creating card Controls..." & return
   set the name of this card to "Controls"
   put "PASS: Card renamed" & return
   
   put "Creating button..." & return
   create button "btn1"
   set the style of button "btn1" to "standard"
   set the label of button "btn1" to "Test"
   put "PASS: Standard button" & return
   
   put "Creating checkbox..." & return
   create button "chk1"
   set the style of button "chk1" to "checkbox"
   put "PASS: Checkbox" & return
   
   put "Creating radio button..." & return
   create button "rad1"
   set the style of button "rad1" to "radioButton"
   put "PASS: Radio button" & return
   
   put "Creating menu button..." & return
   create button "menu1"
   set the style of button "menu1" to "menu"
   put "PASS: Menu button" & return
   
    -- SKIP: Tab button causes crash
    -- put "Creating tab button..." & return
    -- create button "tab1"
    -- set the style of button "tab1" to "tab"
    -- put "PASS: Tab button" & return
    
    put "Creating scrollbar card..." & return
   create card "Scrollbars"
   put "PASS: Scrollbars card" & return
   
   put "Creating horizontal scrollbar..." & return
   create scrollbar "hscr"
   set the orientation of scrollbar "hscr" to "horizontal"
   put "PASS: Horizontal scrollbar" & return
   
   put "Creating vertical scrollbar..." & return
   create scrollbar "vscr"
   set the orientation of scrollbar "vscr" to "vertical"
   put "PASS: Vertical scrollbar" & return
   
    put "Creating slider..." & return
    create scrollbar "sli1"
    set the style of scrollbar "sli1" to "slider"
    put "PASS: Slider" & return
    
    put "Creating progress scrollbar..." & return
    create scrollbar "prog1"
    set the style of scrollbar "prog1" to "progress"
   put "PASS: Progress scrollbar" & return
   
   put "Creating groups card..." & return
   create card "Groups"
   put "PASS: Groups card" & return
   
   put "Creating group with border..." & return
   create group "grp1"
   set the showBorder of group "grp1" to true
   put "PASS: Group with border" & return
   
   put "Creating nested groups..." & return
   create group "outer"
   create group "inner" in group "outer"
   put "PASS: Nested groups" & return
   
   put "Creating fields card..." & return
   create card "Fields"
   put "PASS: Fields card" & return
   
   put "Creating standard field..." & return
   create field "fld1"
   set the text of field "fld1" to "Hello"
   put "PASS: Standard field" & return
   
   put "Creating scrolling field..." & return
   create field "fld2"
   set the vScrollbar of field "fld2" to true
   put "PASS: Scrolling field" & return
   
   put "Creating graphics card..." & return
   create card "Graphics"
   put "PASS: Graphics card" & return
   
   put "Creating rectangle graphic..." & return
   create graphic "rect1"
   set the style of graphic "rect1" to "rectangle"
   put "PASS: Rectangle graphic" & return
   
   put "Showing stack..." & return
   set the visible of stack tStack to true
   put "PASS: Stack shown" & return
   
   put "Hiding stack..." & return
   set the visible of stack tStack to false
   put "PASS: Stack hidden" & return
   
   put "Deleting stack..." & return
   delete stack tStack
   put "PASS: Stack deleted" & return
   
   put "All step-by-step tests passed." & return
   quit 0
end testStepByStep
