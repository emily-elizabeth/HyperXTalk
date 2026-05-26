on startup
   put "Hello from HyperXTalk GTK3 build" & return
   put "Testing basic operations..." & return
   
   create stack "TestStack"
   set the defaultStack to "TestStack"
   
   create button "testBtn"
   set the label of button "testBtn" to "Test"
   set the enabled of button "testBtn" to false
   set the enabled of button "testBtn" to true
   
   create field "testFld"
   set the text of field "testFld" to "Hello World"
   
   create scrollbar "testScr"
   set the thumbPosition of scrollbar "testScr" to 50
   
   put "PASS: Basic widget creation and property setting works" & return
   
   delete stack "TestStack"
   
   put "PASS: Stack cleanup works" & return
   put "All basic tests passed." & return
   
   quit 0
end startup
