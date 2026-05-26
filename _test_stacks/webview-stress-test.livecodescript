script "WebViewStressTest"
-- Test concurrent multi-instance browser widgets, rapid property updates, and lifecycle transitions
-- Specifically verifies that WebKitGTK process recycling and destruction do not crash the engine

on openStack
   webViewStressTest
end openStack

on webViewStressTest
   put "WebViewStressTest" into tStack

   -- Create stack
   create stack tStack
   set the defaultStack to tStack

   -- Stress loop: 3 iterations
   repeat with tLoop = 1 to 3
      put "Iteration " & tLoop & " starting..." & return
      
      -- Create 5 concurrent browser widgets
      repeat with i = 1 to 5
         put "browser_" & i into tName
         create widget tName as "com.livecode.widget.browser"
         set the rect of widget tName to (i*10) & "," & (i*10) & "," & (i*10 + 200) & "," & (i*10 + 150)
         set the htmlText of widget tName to "<html><body>Browser " & i & "</body></html>"
      end repeat
      
      -- Set URL / evaluate JS concurrently
      repeat with i = 1 to 5
         put "browser_" & i into tName
         try
            set the url of widget tName to "about:blank"
            do "document.body.innerHTML = 'Stressed " & i & "'" in widget tName
         catch tError
            -- Non-fatal
         end try
      end repeat

      -- Rapid show/hide cycle
      repeat with i = 1 to 5
         put "browser_" & i into tName
         hide widget tName
         show widget tName
      end repeat

      -- Destroy widgets in reverse order
      repeat with i = 5 downTo 1
         put "browser_" & i into tName
         delete widget tName
      end repeat
      
      put "Iteration " & tLoop & " complete" & return
   end repeat

   -- Cleanup
   close stack tStack

   put "=== WebView Stress Test COMPLETE ===" & return
   put "If you're reading this, no crash occurred." & return
   quit 0
end webViewStressTest
