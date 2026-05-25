script "com.hyperxtalk.library.shakecontrol"

local sShakes   -- array indexed by integer key; holds per-animation state
local sNextKey  -- integer counter, gives each animation a unique key

on extensionInitialize
   if the target is me then
      insert the script of me into back
   end if
   
   if the environment contains "development" then
      set the _ideoverride of me to true
   end if
end extensionInitialize

on extensionFinalize
   if the target is me then
      remove the script of me from back
   end if
end extensionFinalize

/**

Title: Shake Control

Author: HyperXTalk

Type: library

Version: 1.0.0

Description:
Animates a control with a horizontal shake that mimics the macOS password
rejection animation — a short, damped oscillation that draws the user's
attention to the control without moving it from its original position.

*/

----------------------------------------------------------------------
-- Public API
----------------------------------------------------------------------

/**

Summary: Shakes a control horizontally to indicate an error.

Parameters:
pTarget (string): The long id of the control to animate.

Description:
Use <shakeControl> to provide a clear visual rejection cue, for example
when a user submits an incorrect password. The control oscillates left
and right with decreasing amplitude over approximately 400 milliseconds,
then returns exactly to its original position.

If the control is already being shaken when <shakeControl> is called, the
running animation is cancelled and a new one begins immediately.

When the animation completes, a <shakeControlDone> message is sent to the
target control.

Example:
on mouseUp
   if field "Password" is not sCorrectPassword then
      shakeControl the long id of field "Password"
   end if
end mouseUp

*/
command shakeControl pTarget
   -- Cancel any running shake on this control first
   stopShakeControl pTarget

   if sNextKey is empty then put 0 into sNextKey
   add 1 to sNextKey
   local tKey
   put sNextKey into tKey

   put the long id of pTarget into sShakes[tKey]["target"]
   put item 1 of the rect of pTarget into sShakes[tKey]["originLeft"]
   put 1 into sShakes[tKey]["step"]

   send "shakeStep" && tKey to me in 0 milliseconds
end shakeControl

/**

Summary: Cancels a running shake animation and restores the control's position.

Parameters:
pTarget (string): The long id of the control whose animation should be stopped.

Description:
Use <stopShakeControl> to interrupt a shake started by <shakeControl>. The
control is immediately restored to its original horizontal position. If the
control is not currently being shaken, this command has no effect.

Example:
on closeCard
   stopShakeControl the long id of field "Password"
end closeCard

*/
command stopShakeControl pTarget
   local tID
   put the long id of pTarget into tID
   repeat for each key tKey in sShakes
      if sShakes[tKey]["target"] is tID then
         --do "set the left of (" & tID & ") to " & sShakes[tKey]["originLeft"]
         set the left of tID to sShakes[tKey]["originLeft"]
         put empty into sShakes[tKey]
         exit repeat
      end if
   end repeat
end stopShakeControl

----------------------------------------------------------------------
-- Private: animation ticker
----------------------------------------------------------------------

-- Horizontal offsets (pixels from origin) applied at each step.
-- The pattern is a damped oscillation: large swings that taper off,
-- ending at 0 so the control returns exactly to its start position.
-- Timings: 40 ms per step → full animation ~400 ms.
private function shakeOffsets
   return "10,-10,8,-8,6,-6,4,-4,2,-2,0"
end shakeOffsets

on shakeStep pKey
   if sShakes[pKey] is empty then exit shakeStep

   local tOffsets
   put shakeOffsets() into tOffsets

   local tStep
   put sShakes[pKey]["step"] into tStep

   local tOriginLeft
   put sShakes[pKey]["originLeft"] into tOriginLeft

   local tTargetID
   put sShakes[pKey]["target"] into tTargetID

   -- Apply offset for this step
   local tOffset
   put item tStep of tOffsets into tOffset
   --do "set the left of (" & tTargetID & ") to " & (tOriginLeft + tOffset)
   set the left of tTargetID to (tOriginLeft + tOffset)

   if tStep < the number of items in tOffsets then
      add 1 to sShakes[pKey]["step"]
      send "shakeStep" && pKey to me in 40 milliseconds
   else
      -- Restore exact origin and clean up
      --do "set the left of (" & tTargetID & ") to " & tOriginLeft
      set the left of tTargetID to tOriginLeft
      put empty into sShakes[pKey]
      --do "send ""shakeControlDone"" to (" & tTargetID & ")"
      dispatch "shakeControlDone" to tTargetID
   end if
end shakeStep
