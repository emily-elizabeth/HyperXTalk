script "PlayerControl"

on TestPlayerProperties
   local tStack
   create stack "temp_player_test_stack"
   put the name of stack "temp_player_test_stack" into tStack
   open stack tStack
   
   create player "test_player" in card 1 of stack tStack
   TestAssert "player created", there is a player "test_player" of card 1 of stack tStack
   
   -- Set up the test wav file
   local tWavPath
   put "tests/lcs/core/multimedia/silence.wav" into tWavPath
   set the filename of player "test_player" of card 1 of stack tStack to tWavPath
   TestAssert "filename set", the filename of player "test_player" of card 1 of stack tStack contains "silence.wav"

   -- Check default loudness and looping properties
   set the playLoudness of player "test_player" of card 1 of stack tStack to 50
   TestAssert "playLoudness set", the playLoudness of player "test_player" of card 1 of stack tStack is 50

   set the looping of player "test_player" of card 1 of stack tStack to true
   TestAssert "looping set to true", the looping of player "test_player" of card 1 of stack tStack is true

   set the looping of player "test_player" of card 1 of stack tStack to false
   TestAssert "looping set to false", the looping of player "test_player" of card 1 of stack tStack is false

   -- Test play/pause/stop commands
   -- Initially the player is paused/stopped on load
   TestAssert "initially paused", the paused of player "test_player" of card 1 of stack tStack is true

   -- Start the player
   start player "test_player" of card 1 of stack tStack
   wait 200 milliseconds
   TestAssert "is playing", the paused of player "test_player" of card 1 of stack tStack is false

   -- Pause the player
   set the paused of player "test_player" of card 1 of stack tStack to true
   wait 200 milliseconds
   TestAssert "is paused", the paused of player "test_player" of card 1 of stack tStack is true

   -- Start again
   start player "test_player" of card 1 of stack tStack
   wait 200 milliseconds
   TestAssert "is playing again", the paused of player "test_player" of card 1 of stack tStack is false

   -- Stop the player
   stop player "test_player" of card 1 of stack tStack
   wait 200 milliseconds
   TestAssert "is stopped", the paused of player "test_player" of card 1 of stack tStack is true

   -- Clean up
   delete player "test_player" of card 1 of stack tStack
   close stack tStack
   delete stack tStack
end TestPlayerProperties
