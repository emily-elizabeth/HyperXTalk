script "revStandaloneSettingsMacBehavior"
on preOpenCard   
   local tStandaloneSettingsA
   put getSettings() into tStandaloneSettingsA
   
   -- MM-2014-03-21: [[ PPC Support Dropped ]] Remove any references to Universal or PPC builds in standalone settings
   --   and replace with the best available architecture (arm64 preferred on Apple Silicon).
   if tStandaloneSettingsA["MacOSX"] then
      delete variable tStandaloneSettingsA["MacOSX"]
      if revEngineCheck("MacOSX arm64") then
         put true into tStandaloneSettingsA["MacOSX arm64"]
      else
         put true into tStandaloneSettingsA["MacOSX x86-64"]
      end if
   end if

   if tStandaloneSettingsA["MacOSX PowerPC-32"] then
      delete variable tStandaloneSettingsA["MacOSX PowerPC-32"]
      if revEngineCheck("MacOSX arm64") then
         put true into tStandaloneSettingsA["MacOSX arm64"]
      else
         put true into tStandaloneSettingsA["MacOSX x86-64"]
      end if
   end if
   
   if tStandaloneSettingsA["MacOSX x86-32"] then
      delete variable tStandaloneSettingsA["MacOSX x86-32"]
      -- HyperXTalk: on Apple Silicon promote x86-32 to arm64 instead of x86-64
      if revEngineCheck("MacOSX arm64") then
         put true into tStandaloneSettingsA["MacOSX arm64"]
      else
         put true into tStandaloneSettingsA["MacOSX x86-64"]
      end if
   end if

   -- Similarly migrate old Universal/PPC entries to arm64 if the arm64 engine is present
   -- (they were already set to x86-64 above; clear x86-64 if arm64 takes over).
   -- Also handles the case where both arm64 and x86-64 are set from saved settings.
   if tStandaloneSettingsA["MacOSX arm64"] and tStandaloneSettingsA["MacOSX x86-64"] then
      put false into tStandaloneSettingsA["MacOSX x86-64"]
   end if

   setSettings tStandaloneSettingsA

   -- Repurpose the obsolete x86-32 button as the arm64 button on Apple Silicon.
   -- The x86-32 Mac target has been unsupported for years; we reuse its GUI slot
   // rather than modifying the binary stack file.
   repeat with x = 1 to the number of buttons of group "Flavors"
      if the cSettingTag of button x of group "Flavors" is "MacOSX x86-32" then
         set the cSettingTag of button x of group "Flavors" to "MacOSX arm64"
         set the label of button x of group "Flavors" to "arm64"
         exit repeat
      end if
   end repeat

   set the enabled of group "OSX" to false

   local tHiliteArch, tAnyHilited, tFirstButton
   put false into tAnyHilited
   repeat with x = 1 to the number of buttons of group "Flavors"
      local tTag
      put the cSettingTag of button x of group "Flavors" into tTag
      // PM-2016-04-01: Using a stack created with LC < 8 has empty cSettingTag for button MacOSX x86_64
      put computeDefault(tStandaloneSettingsA[tTag], false) into tHiliteArch
      set the hilite of button x of group "Flavors" to tHiliteArch
      if the hilite of button x of group "Flavors" is true then
         set the enabled of group "OSX" to true
         put true into tAnyHilited
      end if
      if tFirstButton is empty then put x into tFirstButton
   end repeat

   // If no architecture was previously selected, auto-select the best default so
   // the group is enabled and the user can interact with it.  On Apple Silicon
   // prefer arm64 if present; otherwise fall back to the first available flavor.
   if not tAnyHilited and tFirstButton is not empty then
      local tDefaultBtn
      repeat with x = 1 to the number of buttons of group "Flavors"
         if the cSettingTag of button x of group "Flavors" contains "arm64" then
            put x into tDefaultBtn
            exit repeat
         end if
      end repeat
      if tDefaultBtn is empty then put tFirstButton into tDefaultBtn
      set the hilite of button tDefaultBtn of group "Flavors" to true
      local tDefaultTag
      put the cSettingTag of button tDefaultBtn of group "Flavors" into tDefaultTag
      put true into tStandaloneSettingsA[tDefaultTag]
      setSettings tStandaloneSettingsA
      set the enabled of group "OSX" to true
   end if
   
   put tStandaloneSettingsA["OSX,appicon"] into fld "appicon"
   put tStandaloneSettingsA["OSX,smallappicon"] into fld "smallappicon"
   put tStandaloneSettingsA["OSX,signature"] into fld "signature"
   put tStandaloneSettingsA["OSX,identifier"] into fld "identifier"
   put tStandaloneSettingsA["OSX,documentType"] into fld "documentType"
   put tStandaloneSettingsA["OSX,iconFile"] into fld "iconFile"
   put tStandaloneSettingsA["OSX,documenticonFile"] into fld "documenticonFile"
   put tStandaloneSettingsA["OSX,name"] into fld "name"
   put tStandaloneSettingsA["OSX,documentExtension"] into fld "documentExtension"
   put tStandaloneSettingsA["OSX,shortVersion"] into fld "shortVersion"
   put tStandaloneSettingsA["OSX,info"] into fld "info"
   put tStandaloneSettingsA["OSX,longVersion"] into fld "longVersion"
   put tStandaloneSettingsA["OSX,copyright"] into fld "copyright"
   put tStandaloneSettingsA["OSX,plist"] into fld "plist"
   
   local tSize
   put the cSize of this cd into tSize
   if tStandaloneSettingsA["OSX,plist"] = "" then
      hide group "choose"
      show group "enter"
      set the hilitedButtonName of group "plist" to "enter"
      #MJVH ~ 1208/22/05/2006
      put the cSize of group "enter" into item 2 of tSize
   else
      show group "choose"
      hide group "enter"
      set the hilitedButtonName of group "plist" to "choose"
      put the cSize of group "choose" into item 2 of tSize
   end if
   set the cSize of this cd to tSize
   pass preOpenCard
end preOpenCard
