script "com.hyperxtalk.library.macscrollenhancement2"

on extensionInitialize
   if the target is not me then pass extensionInitialize
   insert the script of me into back
   if the platform is "MacOS" then
      send "seScrollEnhancementEnable" to me in 0 milliseconds
   end if
   if the environment contains "development" then
      set the _ideoverride of me to true
   end if
end extensionInitialize

on extensionFinalize
   if the target is not me then pass extensionFinalize
   remove the script of me from back
   seScrollEnhancementDisable
end extensionFinalize


/**

Title: Mac Scroll Enhancement

Author: HyperXTalk

Type: library

Version: 1.0.1

Description:
  Generic smooth-scroll library for HyperXTalk.

  Intercepts precise trackpad scroll events application-wide via a C
  NSEvent monitor and applies accumulated pixel deltas to whichever
  scrollable HyperXTalk control is currently under the mouse - fields,
  groups with scrollbars, etc.

  Call seScrollEnhancementEnable from any openStack handler that should
  benefit from smooth scrolling. Call seScrollEnhancementDisable from the
  matching closeStack. Reference-counted - the poll keeps running until
  every caller has disabled it.
*/

-- ---------------------------------------------------------------------------
-- Script-local state
-- ---------------------------------------------------------------------------

local sRefCount         -- number of active callers; poll runs while > 0
local sMonitorInstalled -- true when the C NSEvent monitor is active

-- ---------------------------------------------------------------------------
-- Public API
-- ---------------------------------------------------------------------------

/**
seScrollEnhancementEnable pWindowId
Start smooth scrolling. Call from openStack (or equivalent).
pWindowId is accepted for backward compatibility; no longer used internally.
Safe to call from multiple windows ‚Äî reference-counted.
*/
command seScrollEnhancementEnable
   if the platform is not "MacOS" then exit seScrollEnhancementEnable
   add 1 to sRefCount
   if sRefCount > 1 then exit seScrollEnhancementEnable
   macScrollEnhancementRemove
   macScrollEnhancementInstall 0   -- pre-install so monitor is ready before mouseControl updates
   put true into sMonitorInstalled
   send "seScrollPoll" to me in 16 milliseconds
end seScrollEnhancementEnable

/**
seScrollEnhancementDisable
Stop smooth scrolling. Call from closeStack (or equivalent).
Reference-counted - poll stops only when every caller has disabled.
*/
command seScrollEnhancementDisable
   if the platform is not "MacOS" then exit seScrollEnhancementDisable
   subtract 1 from sRefCount
   if sRefCount > 0 then exit seScrollEnhancementDisable  -- others still active
   if sMonitorInstalled then macScrollEnhancementRemove
   put false into sMonitorInstalled
   put 0 into sRefCount   -- clamp in case of unbalanced calls
end seScrollEnhancementDisable

/**
seGetScrollTarget
Returns the long id of the best scrollable control under the mouse,
or empty if there is none. Walks up the owner chain from mouseControl
to find the first eligible control:
- A field (always potentially scrollable) - except fields inside a
group named "Gutter" (line-number display in the script editor).
- A group that has vScrollbar or hScrollbar set to true.
*/
function seGetScrollTarget
   local tControl
   try
      put the long id of the mouseControl into tControl
   catch tError
      put empty into tControl
   end try

   -- Data views like the Project Browser need special handling since 
   -- the content is not a static group and is generated dynamically
   if tControl is not empty and the dvcontrol of tControl is not empty then
      return tControl
   end if

   -- Fallback when mouseControl hasn't updated yet (e.g. right after window opens)
   if tControl is empty and the mouseStack is not empty then
      -- Get the selected field on the mousestack.  If mouse is over 
      -- another control then tControl will get updated.
      if the selectedfield is not empty and \
            the long name of the mousestack is the topstack and \
            the long id of the selectedfield ends with the topstack then
         put the long id of the selectedfield into tControl
      end if
      local tN
      put the number of controls of the mouseStack into tN
      local tMouseLoc
      put the mouseLoc into tMouseLoc
      repeat with i = tN down to 1
         local tCandidate, tRect
         put the long id of control i of the mouseStack into tCandidate
         put the rect of tCandidate into tRect
         if tMouseLoc is within tRect then
            put tCandidate into tControl
            exit repeat
         end if
      end repeat
   end if
   
   if tControl is empty then return empty
   repeat
      local tKind
      put word 1 of tControl into tKind
      
      if tKind is "field" then
         -- Skip fields inside the line-number gutter.
         local tParent
         put the long id of the owner of tControl into tParent
         if word 1 of tParent is "group" and the short name of tParent is "Gutter" then
            put tParent into tControl   -- continue walking up from the gutter group
         else
            return tControl
         end if
         
      else if tKind is "group" then
         local tHasScrollbar
         try
            put the vScrollbar of tControl into tHasScrollbar
            if tHasScrollbar then return tControl
         end try
         try
            put the hScrollbar of tControl into tHasScrollbar
            if tHasScrollbar then return tControl
         end try
         
         -- Not a scrolling group - walk up.
         put the long id of the owner of tControl into tControl
         
      else
         -- card, stack, or unrecognised - stop.
         return empty
      end if
      
      if tControl is empty then return empty
   end repeat
end seGetScrollTarget

/**
seSyncGutter pScrolledControl, pNewV
If the scrolled control is the Script field of a script editor, syncs
the line-number gutter to the same vertical scroll position.
*/
command seSyncGutter pScrolledControl, pNewV
   if the short name of pScrolledControl is not "Script" then exit seSyncGutter
   -- Owner chain: field -> card -> stack (the script editor).
   local tStackName
   put trueword 14 of pScrolledControl into tStackName
   if tStackName is empty then exit seSyncGutter
   if there is not a group "Gutter" of stack tStackName then exit seSyncGutter
   local tOldDefault
   put the defaultStack into tOldDefault
   set the defaultStack to tStackName
   set the vScroll of field "Numbers" of group "Gutter" to pNewV
   set the defaultStack to tOldDefault
end seSyncGutter

-- ---------------------------------------------------------------------------
-- Internal: momentum poll loop
-- ---------------------------------------------------------------------------

on seScrollPoll
   -- Stop naturally when all callers have disabled.
   if sRefCount < 1 then exit seScrollPoll
   
   -- Find whatever scrollable control is under the mouse right now.
   local tScrollTarget
   put seGetScrollTarget() into tScrollTarget
   
   -- Install or remove the C monitor based on whether there is a target.
   -- Keeping the monitor installed when there is a target also blocks
   -- HyperXTalk's own coarse line-quantised scroll from reaching the control.
   if tScrollTarget is not empty then
      if not sMonitorInstalled then
         macScrollEnhancementInstall the windowId of stack (the mouseStack)
         put true into sMonitorInstalled
      end if
   else
      if sMonitorInstalled then
         macScrollEnhancementRemove
         put false into sMonitorInstalled
      end if
   end if
   
   -- Always drain both accumulators every tick so stale delta never carries
   -- over to a new scroll target.
   local tDeltaY, tDeltaX
   put macScrollEnhancementGetDelta() into tDeltaY
   put macScrollEnhancementGetDeltaX() into tDeltaX
   
   if tScrollTarget is not empty then
      -- Special case data views like the Project Browser
      -- Initial use case only implements vscroll
      if the dvcontrol of tScrollTarget is not empty then
         // bounds checking performed by data view so just use raw delta
         if abs(tDeltaY) > 0.5 then
            set the viewprop["vscroll"] of tScrollTarget to \
                  (the viewprop["vscroll"] of tScrollTarget) + tDeltaY
         end if
         send "seScrollPoll" to me in 16 milliseconds
         exit seScrollPoll
      end if
      
      -- Suppress scrolling while the script editor handler list popup is
      -- visible. The popup is drawn inside the same NSWindow as the field
      -- so the OS-level window check in the C layer cannot detect it; we
      -- handle it here instead.
      local tHandlerListObj
      try
         put seHandlerListObject() into tHandlerListObj
      catch tError
         put empty into tHandlerListObj
      end try
      if tHandlerListObj is not empty and the visible of tHandlerListObj then
         send "seScrollPoll" to me in 16 milliseconds
         exit seScrollPoll
      end if
      
      -- Apply vertical scroll.
      if abs(tDeltaY) > 0.5 then
         local tCurrentV, tMaxV, tNewV
         put the vScroll of tScrollTarget into tCurrentV
         put the formattedHeight of tScrollTarget - the height of tScrollTarget into tMaxV
         if tMaxV < 0 then put 0 into tMaxV
         put tCurrentV + tDeltaY into tNewV
         if tNewV < 0 then put 0 into tNewV
         if tNewV > tMaxV then put tMaxV into tNewV
         set the vScroll of tScrollTarget to tNewV
         seSyncGutter tScrollTarget, tNewV
      end if
      
      -- Apply horizontal scroll.
      if abs(tDeltaX) > 0.5 then
         local tCurrentH, tMaxH, tNewH
         put the hScroll of tScrollTarget into tCurrentH
         put the formattedWidth of tScrollTarget - the width of tScrollTarget into tMaxH
         if tMaxH < 0 then put 0 into tMaxH
         put tCurrentH + tDeltaX into tNewH
         if tNewH < 0 then put 0 into tNewH
         if tNewH > tMaxH then put tMaxH into tNewH
         set the hScroll of tScrollTarget to tNewH
      end if
   end if
   
   send "seScrollPoll" to me in 16 milliseconds
end seScrollPoll
