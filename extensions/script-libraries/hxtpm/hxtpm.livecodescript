script "com.hyperxtalk.library.hxtpm"
/*
Copyright (C) 2026 HyperXTalk

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

HyperXTalk is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
HyperXTalk. If not, see <http://www.gnu.org/licenses/>.
*/

-- ---------------------------------------------------------------------------
-- Script-local state
-- ---------------------------------------------------------------------------

-- In-memory cache of the sources list.  Loaded lazily from disk on first use.
-- Each element is an array with keys: name, url|path, pinned (optional bool).
-- The official HyperXTalk source is always element 1 and is pinned.
local sSources

-- State for in-progress async downloads, keyed by download URL.
-- Each element is an array with keys: name, version, versionData, sourceName,
-- tempPath, type (lcs|lce|lcp), callerTarget, completionMessage, and for lcp: filename.
-- libURLDownloadToFile callbacks look up this table by URL to resume the install.
local sPendingInstalls

-- ---------------------------------------------------------------------------
-- Extension lifecycle
-- ---------------------------------------------------------------------------

on extensionInitialize
   if the target is not me then
      pass extensionInitialize
   end if

   insert the script of me into back

   if the environment contains "development" then
      set the _ideoverride of me to true
   end if
end extensionInitialize

on extensionFinalize
   if the target is not me then
      pass extensionFinalize
   end if

   remove the script of me from back
end extensionFinalize

/**

Title: HyperXTalk Package Manager Library

Author: HyperXTalk

Type: library

Version: 1.2.0

Description:
Script library for installing, updating, and removing HyperXTalk packages
from the hxtpm registry.

Packages are hosted on GitHub or stored locally. Three package types are
supported:

  lcs — Script libraries. Installed to the user libraries folder as a named
        subfolder containing the .livecodescript file. Loaded immediately via
        start using stack. No IDE restart required.

  lce — Pre-compiled LiveCode Extension packages (.lce zip). Installed to the
        user extensions folder via the IDE extension manager. Loaded immediately
        via load extension from file. No IDE restart required.

  lcp — LiveCode IDE plugins. The plugin stack file is placed directly in the
        user plugins folder. The IDE plugin registry is refreshed via
        revIDEUpdatePlugins immediately after install or uninstall.
        No IDE restart required.

Multiple package sources are supported. A source can be a remote GitHub-hosted
registry or a local folder on disk. The official HyperXTalk source is always
present and cannot be removed; additional sources can be added by developers
or users. Packages are resolved in source order; the first source that
provides a package wins.

The sources list is stored in a JSON file at:
   <UserLibraries>/.hxtpm/sources.json

Package metadata (hxtpm.json) is written alongside each installed package:
   lcs  — <UserLibraries>/<name>/hxtpm.json
   lce  — <UserExtensions>/<name>.<version>/hxtpm.json
   lcp  — <UserPlugins>/.hxtpm/<name>/hxtpm.json

Install and update commands are asynchronous: they return immediately and
deliver their result via a message dispatched to the calling object.
Uninstall and source-management commands are synchronous and return empty on
success or an error message via the result.

Authoring checksums:
Use hxtpmComputeChecksum to get the correct sha256 value for a package file
URL before writing it into package.json. This downloads the file through
LiveCode's own network stack and hashes it identically to how the install
pipeline verifies it, guaranteeing the values will match.

Usage:
   -- Browse all packages available across all sources
   put hxtpmAvailable() into tAll
   repeat for each key tName in tAll
      put tName && tAll[tName]["latest"] & return after field "packageList"
   end repeat

   -- Install the latest version of a package (resolved from all sources)
   hxtpmInstall "com.example.library.myutils"

   on hxtpmInstallComplete pName, pError
      if pError is not empty then answer error pError
   end hxtpmInstallComplete

   -- Install a specific version
   hxtpmInstall "com.example.library.myutils", "1.2.0"

   -- Manage sources
   hxtpmSourceAdd "Acme Corp", "https://raw.githubusercontent.com/acme/hxtpm-packages/main"
   hxtpmSourceAdd "Local Dev", "/Users/me/dev/my-packages"
   hxtpmSourceRemove "Acme Corp"
   put hxtpmSourceList() into tSources

   -- Check what is installed
   put hxtpmInstalled() into tInstalled
   repeat for each key tName in tInstalled
      put tName && tInstalled[tName]["version"] & return after field "packageList"
   end repeat

   -- Uninstall / update
   hxtpmUninstall "com.example.library.myutils"
   if the result is not empty then answer error the result

   hxtpmUpdate "com.example.library.myutils"

   on hxtpmUpdateComplete pName, pError
      if pError is not empty then answer error pError
   end hxtpmUpdateComplete

   -- Compute the correct checksum for a URL before writing it into package.json
   hxtpmComputeChecksum "https://raw.githubusercontent.com/example/repo/main/MyLib.lcs"

   on hxtpmChecksumComplete pURL, pChecksum, pError
      if pError is not empty then answer error pError
      -- paste pChecksum directly into the "checksum" field of package.json
   end hxtpmChecksumComplete

*/

-- ---------------------------------------------------------------------------
-- Public API — source management
-- ---------------------------------------------------------------------------

/**

Summary: Add a new package source.

Parameters:
pName: A short human-readable name for the source, e.g. "Acme Corp Packages".
pURLOrPath: Either a base URL for a remote source (e.g.
   "https://raw.githubusercontent.com/acme/hxtpm-packages/main") or an
   absolute local folder path (e.g. "/Users/me/dev/my-packages").
   The source must expose an index.json at its root.

Description:
Validates the source by fetching its index.json, then appends it to the
sources list. Local paths are implicitly trusted. Remote sources added by
the user are trusted by act of adding them. The result is empty on success
or an error message on failure.

Example:
   hxtpmSourceAdd "Acme Corp", "https://raw.githubusercontent.com/acme/hxtpm-pkgs/main"
   if the result is not empty then answer error the result

   hxtpmSourceAdd "Local Dev", "/Users/me/dev/local-repo"
   if the result is not empty then answer error the result

*/
command hxtpmSourceAdd pName, pURLOrPath
   if pName is empty or pURLOrPath is empty then
      return "Both a name and a URL or path are required"
   end if

   local tSources
   put _loadSources() into tSources

   -- Reject duplicates
   repeat for each key tIdx in tSources
      if tSources[tIdx]["name"] is pName then
         return "A source named '" & pName & "' already exists"
      end if
   end repeat

   -- Build source entry and determine base for index fetch
   local tNewSource, tBase
   if pURLOrPath begins with "/" or char 2 of pURLOrPath is ":" then
      -- Looks like a local absolute path (Unix /… or Windows C:\…)
      if there is not a folder pURLOrPath then
         return "Local path does not exist: " & pURLOrPath
      end if
      put pURLOrPath into tNewSource["path"]
      put "file:" & pURLOrPath into tBase
   else
      put pURLOrPath into tNewSource["url"]
      put pURLOrPath into tBase
   end if
   put pName into tNewSource["name"]

   -- Validate: the source must serve a readable index.json
   local tTestRaw
   put URL (tBase & "/index.json") into tTestRaw
   if the result is not empty or tTestRaw is empty then
      return "Could not fetch index.json from source." && \
            "Check the URL or path and try again."
   end if
   local tTestIndex
   put JsonImport(tTestRaw) into tTestIndex
   if tTestIndex is not an array or tTestIndex["packages"] is empty then
      return "Source index.json is not a valid hxtpm index"
   end if

   -- Append and persist
   local tNext
   put the number of elements of tSources + 1 into tNext
   put tNewSource into tSources[tNext]
   put tSources into sSources
   _saveSources

   return empty
end hxtpmSourceAdd

/**

Summary: Remove a previously added source.

Parameters:
pName: The name of the source to remove (as it was given to hxtpmSourceAdd).

Description:
Removes the named source from the list. The official HyperXTalk source is
pinned and cannot be removed. Removing a source does not uninstall packages
that were installed from it. The result is empty on success or an error
message on failure.

Example:
   hxtpmSourceRemove "Acme Corp"
   if the result is not empty then answer error the result

*/
command hxtpmSourceRemove pName
   local tSources
   put _loadSources() into tSources

   local tNewSources, tCount, tFound
   put 1 into tCount
   repeat for each key tIdx in tSources
      if tSources[tIdx]["name"] is pName then
         put true into tFound
         if tSources[tIdx]["pinned"] then
            return "Cannot remove the pinned source '" & pName & "'"
         end if
         -- Skip: don't copy into tNewSources
      else
         put tSources[tIdx] into tNewSources[tCount]
         add 1 to tCount
      end if
   end repeat

   if not tFound then
      return "Source not found: " & pName
   end if

   put tNewSources into sSources
   _saveSources
   return empty
end hxtpmSourceRemove

/**

Summary: Return the current list of package sources.

Returns:
A numerically-keyed array. Each element contains: name, url or path,
and optionally pinned (true for the official source).

Example:
   put hxtpmSourceList() into tSources
   repeat for each key tIdx in tSources
      put tSources[tIdx]["name"] & return after field "sourceList"
   end repeat

*/
function hxtpmSourceList
   return _loadSources()
end hxtpmSourceList

-- ---------------------------------------------------------------------------
-- Public API — packages (commands)
-- ---------------------------------------------------------------------------

/**

Summary: Install a package.

Parameters:
pName: The package name, e.g. "com.example.library.myutils".
pVersion: A specific version string. If omitted, the latest version is installed.

Description:
Searches all configured sources in order and installs the package from the
first source that provides it. The install is asynchronous: hxtpmInstall
returns immediately and the caller receives an hxtpmInstallComplete message
when the install finishes (whether it succeeded or failed).

The package type determines how it is installed:

  lcs — Placed in the user libraries folder and loaded immediately via
        start using stack.
  lce — Installed via the IDE extension manager and loaded immediately.
  lcp — Stack file placed in the user plugins folder; the IDE plugin registry
        is refreshed immediately via revIDEUpdatePlugins.

No IDE restart is required for any package type.

Example:
   -- Trigger install
   hxtpmInstall "com.example.library.myutils"

   -- Handle the result
   on hxtpmInstallComplete pName, pError
      if pError is not empty then
         answer error pError
      else
         answer "Installed" && pName
      end if
   end hxtpmInstallComplete

*/
command hxtpmInstall pName, pVersion
   -- Capture the calling object now, while the message path is intact.
   -- The download callbacks fire asynchronously from a different context, so
   -- they must dispatch hxtpmInstallComplete explicitly to this object.
   local tCallerTarget
   put the long id of the target into tCallerTarget
   local tMsg
   put "hxtpmInstallComplete" into tMsg

   local tPackage
   put _fetchPackageInfo(pName) into tPackage
   if tPackage is empty then
      dispatch tMsg to tCallerTarget with pName, "Package not found in any configured source: " & pName
      exit hxtpmInstall
   end if

   if pVersion is empty then
      put _latestVersion(tPackage) into pVersion
   end if

   if tPackage["versions"][pVersion] is empty then
      dispatch tMsg to tCallerTarget with pName, "Version" && pVersion && "not found for" && pName
      exit hxtpmInstall
   end if

   local tType
   put tPackage["type"] into tType

   if tType is "lcs" then
      _installLCSBegin pName, pVersion, tPackage["versions"][pVersion], tPackage["source"], tPackage["sourceBase"], tCallerTarget, tMsg
   else if tType is "lce" then
      _installLCEBegin pName, pVersion, tPackage["versions"][pVersion], tPackage["source"], tPackage["sourceBase"], tCallerTarget, tMsg
   else if tType is "lcp" then
      _installLCPBegin pName, pVersion, tPackage["versions"][pVersion], tPackage["source"], tPackage["sourceBase"], tCallerTarget, tMsg
   else
      dispatch tMsg to tCallerTarget with pName, "Unknown package type: " & tType
   end if
end hxtpmInstall

/**

Summary: Uninstall an installed package.

Parameters:
pName: The package name to remove.

Description:
Removes the installed package and cleans up its files. Behaviour by type:

  lcs — Stops using the stack and deletes the package subfolder from the user
        libraries folder.
  lce — Unloads and removes the extension via the IDE extension manager.
  lcp — Deletes the plugin stack file from the user plugins folder and refreshes
        the IDE plugin registry via revIDEUpdatePlugins.

The result is empty on success or an error message on failure.

Example:
   hxtpmUninstall "com.example.library.myutils"
   if the result is not empty then answer error the result

*/
command hxtpmUninstall pName
   if not hxtpmIsInstalled(pName) then
      return "Package not installed: " & pName
   end if

   local tMeta
   put _readInstalledMeta(pName) into tMeta

   if tMeta["type"] is "lcs" then
      return _uninstallLCS(pName)
   else if tMeta["type"] is "lce" then
      return _uninstallLCE(pName)
   else if tMeta["type"] is "lcp" then
      return _uninstallLCP(pName)
   else
      return "Unknown installed package type: " & tMeta["type"]
   end if
end hxtpmUninstall

/**

Summary: Update an installed package to the latest version from its source.

Parameters:
pName: The package name to update.

Description:
Fetches the latest version from whichever source originally provided the
package, uninstalls the current version, and installs the latest. If already
at the latest version, hxtpmUpdateComplete is dispatched immediately with an
empty error.

The update is asynchronous: hxtpmUpdate returns immediately and the caller
receives an hxtpmUpdateComplete message when the update finishes. The message
signature is identical to hxtpmInstallComplete — pName and pError.

Example:
   -- Trigger update
   hxtpmUpdate "com.example.library.myutils"

   -- Handle the result
   on hxtpmUpdateComplete pName, pError
      if pError is not empty then
         answer error pError
      else
         answer "Updated" && pName
      end if
   end hxtpmUpdateComplete

*/
command hxtpmUpdate pName
   local tCallerTarget
   put the long id of the target into tCallerTarget

   if not hxtpmIsInstalled(pName) then
      dispatch "hxtpmUpdateComplete" to tCallerTarget with pName, "Package not installed: " & pName
      exit hxtpmUpdate
   end if

   local tLatest
   put hxtpmLatestVersion(pName) into tLatest
   if tLatest is empty then
      dispatch "hxtpmUpdateComplete" to tCallerTarget with pName, "Could not fetch registry info for: " & pName
      exit hxtpmUpdate
   end if

   if tLatest is hxtpmInstalledVersion(pName) then
      dispatch "hxtpmUpdateComplete" to tCallerTarget with pName, empty -- already at latest
      exit hxtpmUpdate
   end if

   hxtpmUninstall pName
   local tErr
   put the result into tErr
   if tErr is not empty then
      dispatch "hxtpmUpdateComplete" to tCallerTarget with pName, tErr
      exit hxtpmUpdate
   end if

   -- The download is async; begin functions will dispatch hxtpmUpdateComplete when done
   local tPackage
   put _fetchPackageInfo(pName) into tPackage
   if tPackage is empty then
      dispatch "hxtpmUpdateComplete" to tCallerTarget with pName, "Could not fetch registry info after uninstall: " & pName
      exit hxtpmUpdate
   end if

   local tType
   put tPackage["type"] into tType

   if tType is "lcs" then
      _installLCSBegin pName, tLatest, tPackage["versions"][tLatest], tPackage["source"], tPackage["sourceBase"], tCallerTarget, "hxtpmUpdateComplete"
   else if tType is "lce" then
      _installLCEBegin pName, tLatest, tPackage["versions"][tLatest], tPackage["source"], tPackage["sourceBase"], tCallerTarget, "hxtpmUpdateComplete"
   else if tType is "lcp" then
      _installLCPBegin pName, tLatest, tPackage["versions"][tLatest], tPackage["source"], tPackage["sourceBase"], tCallerTarget, "hxtpmUpdateComplete"
   else
      dispatch "hxtpmUpdateComplete" to tCallerTarget with pName, "Unknown package type: " & tType
   end if
end hxtpmUpdate

/**

Summary: Compute the hxtpm-compatible SHA-256 checksum for a package file URL.

Parameters:
pURL: The direct download URL of the package file.

Description:
Downloads the file via libURLDownloadToFile and computes its SHA-256 checksum
using exactly the same method as hxtpm's internal checksum verification — so
the value produced here is guaranteed to match at install time.

The result is delivered asynchronously via an hxtpmChecksumComplete message sent
to the calling object:

   on hxtpmChecksumComplete pURL, pChecksum, pError
      -- pChecksum is in the form "sha256:<hex>" on success, empty on failure
      -- pError is empty on success, or an error message on failure
   end hxtpmChecksumComplete

Use this command when authoring a new package.json to get the correct checksum
value to put in the "checksum" field for a given version's source URL.

Example:
   hxtpmComputeChecksum "https://raw.githubusercontent.com/example/repo/main/MyLib.livecodescript"

   on hxtpmChecksumComplete pURL, pChecksum, pError
      if pError is not empty then
         answer error pError
      else
         answer "Checksum for" && pURL & ":" & return & pChecksum
      end if
   end hxtpmChecksumComplete

*/
command hxtpmComputeChecksum pURL
   local tCallerTarget
   put the long id of the target into tCallerTarget

   local tTempPath
   put revEnvironmentUserLibraries() & slash & "hxtpm-checksum-" & the seconds & ".tmp" into tTempPath

   put pURL              into sPendingInstalls[pURL]["checksumURL"]
   put tTempPath         into sPendingInstalls[pURL]["tempPath"]
   put "checksum"        into sPendingInstalls[pURL]["type"]
   put tCallerTarget     into sPendingInstalls[pURL]["callerTarget"]

   libURLDownloadToFile pURL, tTempPath, "_hxtpmChecksumDownloadComplete"
end hxtpmComputeChecksum

on _hxtpmChecksumDownloadComplete pURL, pStatus
   local tState
   put sPendingInstalls[pURL] into tState
   delete variable sPendingInstalls[pURL]

   local tTempPath, tCallerTarget
   put tState["tempPath"]      into tTempPath
   put tState["callerTarget"]  into tCallerTarget

   if pStatus is not "downloaded" then
      delete file tTempPath
      dispatch "hxtpmChecksumComplete" to tCallerTarget with pURL, empty, "Download failed:" && pStatus
      exit _hxtpmChecksumDownloadComplete
   end if

   -- Compute hash the same way _verifyChecksum does so the value is guaranteed
   -- to match when used in a package.json checksum field
   local tData, tDigest, tHex, tByte
   put URL ("file:" & tTempPath) into tData
   delete file tTempPath
   put messageDigest(tData, "SHA-256") into tDigest

   repeat with i = 1 to the length of tDigest
      put baseConvert(charToNum(char i of tDigest), 10, 16) into tByte
      if the length of tByte is 1 then put "0" before tByte
      put tByte after tHex
   end repeat
   put toLower(tHex) into tHex

   dispatch "hxtpmChecksumComplete" to tCallerTarget with pURL, "sha256:" & tHex, empty
end _hxtpmChecksumDownloadComplete

-- ---------------------------------------------------------------------------
-- Public API — packages (functions)
-- ---------------------------------------------------------------------------

/**

Summary: Return registry metadata for a package.

Parameters:
pName: The package name.

Returns:
An array with keys: name, title, description, author, license, repository,
type, source (source name), versions. Returns empty if not found in any source.

Example:
   put hxtpmInfo("com.example.library.myutils") into tInfo
   put tInfo["title"] into field "packageTitle"
   put tInfo["source"] into field "packageSource"

*/
function hxtpmInfo pName
   return _fetchPackageInfo(pName)
end hxtpmInfo

/**

Summary: Return an array of packages available from a given source.

Parameters:
pSourceName (string): The name of the source to query. Pass empty to query
all sources (first source wins on name collision).

Returns:
An array keyed by package name. Each entry contains: title, description,
author, type, license, repository, latest (version string), versions (list),
source (source name). Returns empty if the source index could not be fetched.

Example:
   -- Browse a specific source
   put hxtpmAvailable("HyperXTalk Official") into tPkgs
   repeat for each key tName in tPkgs
      put tName && tPkgs[tName]["latest"] & return after field "packageList"
   end repeat

   -- Browse all sources (legacy / combined view)
   put hxtpmAvailable() into tAll

*/
function hxtpmAvailable pSourceName
   local tSources, tAll
   put _loadSources() into tSources

   repeat for each key tIdx in tSources
      -- If a specific source was requested, skip all others
      if pSourceName is not empty and tSources[tIdx]["name"] is not pSourceName then
         next repeat
      end if

      local tBase, tRaw, tIndex
      put _sourceBase(tSources[tIdx]) into tBase
      if tBase is empty then next repeat

      put URL (tBase & "/index.json") into tRaw
      if the result is not empty or tRaw is empty then next repeat

      put JsonImport(tRaw) into tIndex
      if tIndex is not an array then next repeat

      -- First source that lists a package name wins (only matters in all-sources mode)
      repeat for each key tPkgName in tIndex["packages"]
         if tAll[tPkgName] is empty then
            put tIndex["packages"][tPkgName] into tAll[tPkgName]
            put tSources[tIdx]["name"] into tAll[tPkgName]["source"]
         end if
      end repeat
   end repeat

   return tAll
end hxtpmAvailable

/**

Summary: Return the available versions for a package, newest first.

Parameters:
pName: The package name.

Returns:
A line-delimited list of version strings sorted newest-first, or empty if
the package is not found in any source.

Example:
   put hxtpmVersions("com.example.library.myutils") into tVersions

*/
function hxtpmVersions pName
   local tPackage
   put _fetchPackageInfo(pName) into tPackage
   if tPackage is empty then return empty

   local tVersions
   put the keys of tPackage["versions"] into tVersions
   sort tVersions descending
   return tVersions
end hxtpmVersions

/**

Summary: Return the latest available version of a package.

Parameters:
pName: The package name.

Returns:
The latest version string, or empty if the package is not found.

Example:
   put hxtpmLatestVersion("com.example.library.myutils") into tLatest

*/
function hxtpmLatestVersion pName
   local tPackage
   put _fetchPackageInfo(pName) into tPackage
   if tPackage is empty then return empty
   return _latestVersion(tPackage)
end hxtpmLatestVersion

/**

Summary: Return true if a package is currently installed.

Parameters:
pName: The package name.

Returns:
True if the package is installed, false otherwise.

Example:
   if hxtpmIsInstalled("com.example.library.myutils") then
      put "installed" into field "status"
   end if

*/
function hxtpmIsInstalled pName
   if there is a file _metaPath(pName, "lcs") then return true
   if _findLCEMetaPath(pName) is not empty then return true
   if there is a file _metaPath(pName, "lcp") then return true
   return false
end hxtpmIsInstalled

/**

Summary: Return the currently installed version of a package.

Parameters:
pName: The package name.

Returns:
The installed version string, or empty if the package is not installed.

Example:
   put hxtpmInstalledVersion("com.example.library.myutils") into tVersion

*/
function hxtpmInstalledVersion pName
   local tMeta
   put _readInstalledMeta(pName) into tMeta
   return tMeta["version"]
end hxtpmInstalledVersion

/**

Summary: Return an array describing all installed packages.

Returns:
An array keyed by package name. Each value is an array with keys: name,
version, type, source (source name), installed (date string).

Example:
   put hxtpmInstalled() into tPackages
   repeat for each key tName in tPackages
      put tName && tPackages[tName]["version"] & return after field "packageList"
   end repeat

*/
function hxtpmInstalled
   local tResult

   local tLibFolder
   put revEnvironmentUserLibraries() into tLibFolder
   _scanInstalledFolder tLibFolder, tResult

   local tExtFolder
   put revEnvironmentUserExtensionsPath() into tExtFolder
   _scanInstalledFolder tExtFolder, tResult

   -- Plugin metadata lives in a hidden .hxtpm subfolder inside the plugins folder
   -- so the IDE plugin scanner (which uses files()) doesn't see it
   local tPluginMetaFolder
   put revEnvironmentUserPluginsPath() & slash & ".hxtpm" into tPluginMetaFolder
   if there is a folder tPluginMetaFolder then
      _scanInstalledFolder tPluginMetaFolder, tResult
   end if

   return tResult
end hxtpmInstalled

-- ---------------------------------------------------------------------------
-- Private: sources management
-- ---------------------------------------------------------------------------

-- Return the in-memory sources array, loading from disk if needed.
private function _loadSources
   if sSources is an array and the number of elements of sSources > 0 then
      return sSources
   end if

   local tPath
   put _sourcesFilePath() into tPath

   if there is a file tPath then
      local tRaw
      put URL ("file:" & tPath) into tRaw
      if tRaw is not empty then
         local tParsed
         put JsonImport(tRaw) into tParsed
         if tParsed is an array and the number of elements of tParsed > 0 then
            -- Ensure the official source is still element 1 and still pinned.
            -- If someone edited the file and removed it, we add it back.
            if tParsed[1]["name"] is not _officialSourceName() then
               local tRepaired, tOld
               put _defaultSources() into tRepaired
               put 1 into tOld
               repeat with tOld = 1 to the number of elements of tParsed
                  if tParsed[tOld]["name"] is not _officialSourceName() then
                     local tNext
                     put the number of elements of tRepaired + 1 into tNext
                     put tParsed[tOld] into tRepaired[tNext]
                  end if
               end repeat
               put tRepaired into tParsed
            end if
            put tParsed into sSources
            return sSources
         end if
      end if
   end if

   -- No file or unreadable: initialise with the official source only
   put _defaultSources() into sSources
   _saveSources
   return sSources
end _loadSources

-- Write the in-memory sources array to disk.
private command _saveSources
   local tPath
   put _sourcesFilePath() into tPath

   -- Ensure the config folder exists
   local tDir
   set the itemDelimiter to slash
   put item 1 to -2 of tPath into tDir
   set the itemDelimiter to comma
   if there is not a folder tDir then create folder tDir

   put JsonExport(sSources) into URL ("file:" & tPath)
end _saveSources

-- Absolute path to the sources.json config file.
private function _sourcesFilePath
   return revEnvironmentUserLibraries() & slash & ".hxtpm" & slash & "sources.json"
end _sourcesFilePath

-- The canonical name for the official pinned source.
private function _officialSourceName
   return "HyperXTalk Official"
end _officialSourceName

-- Return the default single-element sources array (official source only).
private function _defaultSources
   local tSrc
   put _officialSourceName() into tSrc[1]["name"]
   put "https://raw.githubusercontent.com/emily-elizabeth/hxtpm/main" into tSrc[1]["url"]
   put true into tSrc[1]["pinned"]
   return tSrc
end _defaultSources

-- Return the base URL string used to construct resource URLs for a source.
-- For remote sources this is the url field; for local sources it is file:<path>.
private function _sourceBase pSource
   if pSource["url"] is not empty then
      return pSource["url"]
   else if pSource["path"] is not empty then
      -- Normalise: ensure no trailing slash
      local tPath
      put pSource["path"] into tPath
      if the last char of tPath is "/" or the last char of tPath is "\" then
         delete the last char of tPath
      end if
      return "file:" & tPath
   end if
   return empty
end _sourceBase

-- ---------------------------------------------------------------------------
-- Private: registry resolution (multi-source)
-- ---------------------------------------------------------------------------

-- Search all sources in order for pName. Returns the package info array from
-- the first source that has it, with extra keys "source" (source name) and
-- "sourceBase" (base URL string) added for use by the installer.
private function _fetchPackageInfo pName
   local tSources
   put _loadSources() into tSources

   repeat for each key tIdx in tSources
      local tBase, tRaw
      put _sourceBase(tSources[tIdx]) into tBase
      if tBase is empty then next repeat

      -- Unload any cached copy so we always get the current version from the source
      unload URL (tBase & "/packages/" & pName & "/package.json")
      put URL (tBase & "/packages/" & pName & "/package.json") into tRaw
      if the result is not empty or tRaw is empty then next repeat

      local tPackage
      put JsonImport(tRaw) into tPackage
      if tPackage is an array then
         put tSources[tIdx]["name"] into tPackage["source"]
         put tBase into tPackage["sourceBase"]
         return tPackage
      end if
   end repeat

   return empty
end _fetchPackageInfo

private function _latestVersion pPackageArray
   local tVersions
   put the keys of pPackageArray["versions"] into tVersions
   sort tVersions descending
   return line 1 of tVersions
end _latestVersion

-- ---------------------------------------------------------------------------
-- Private: install helpers
-- ---------------------------------------------------------------------------

-- Resolve a package file URL: if pFileRef is already an absolute URL or
-- file: path, use it as-is. Otherwise treat it as relative to pSourceBase.
private function _resolveFileURL pFileRef, pSourceBase
   if pFileRef begins with "http://" or \
         pFileRef begins with "https://" or \
         pFileRef begins with "file:" then
      return pFileRef
   end if
   -- Relative reference: append to source base
   if the last char of pSourceBase is "/" then
      return pSourceBase & pFileRef
   else
      return pSourceBase & "/" & pFileRef
   end if
end _resolveFileURL

private command _installLCSBegin pName, pVersion, pVersionData, pSourceName, pSourceBase, pCallerTarget, pCompletionMessage
   local tFileURL
   put _resolveFileURL(pVersionData["source"], pSourceBase) into tFileURL

   -- Unique temp filename avoids stale state from previous failed attempts
   local tTempPath
   put revEnvironmentUserLibraries() & slash & pName & "-" & pVersion & "-" & the seconds & ".livecodescript" into tTempPath

   -- Record state so the download callback can resume the install
   put pName              into sPendingInstalls[tFileURL]["name"]
   put pVersion           into sPendingInstalls[tFileURL]["version"]
   put pVersionData       into sPendingInstalls[tFileURL]["versionData"]
   put pSourceName        into sPendingInstalls[tFileURL]["sourceName"]
   put tTempPath          into sPendingInstalls[tFileURL]["tempPath"]
   put "lcs"              into sPendingInstalls[tFileURL]["type"]
   put pCallerTarget      into sPendingInstalls[tFileURL]["callerTarget"]
   put pCompletionMessage into sPendingInstalls[tFileURL]["completionMessage"]

   libURLDownloadToFile tFileURL, tTempPath, "_hxtpmLCSDownloadComplete"
end _installLCSBegin

on _hxtpmLCSDownloadComplete pURL, pStatus
   local tState
   put sPendingInstalls[pURL] into tState
   delete variable sPendingInstalls[pURL]

   local tName, tVersion, tVersionData, tSourceName, tTempPath, tCallerTarget, tMsg
   put tState["name"]              into tName
   put tState["version"]           into tVersion
   put tState["versionData"]       into tVersionData
   put tState["sourceName"]        into tSourceName
   put tState["tempPath"]          into tTempPath
   put tState["callerTarget"]      into tCallerTarget
   put tState["completionMessage"] into tMsg

   if pStatus is not "downloaded" then
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, "Download failed for" && tName & ":" && pStatus
      exit _hxtpmLCSDownloadComplete
   end if

   -- Verify checksum
   local tErr
   put _verifyChecksum(tTempPath, tVersionData["checksum"]) into tErr
   if tErr is not empty then
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, tErr
      exit _hxtpmLCSDownloadComplete
   end if

   -- Create the package subfolder
   local tFileURL, tFilename, tDestFolder, tDestPath
   put tVersionData["source"] into tFileURL
   put _filenameFromURL(tFileURL) into tFilename
   put revEnvironmentUserLibraries() & slash & tName into tDestFolder
   create folder tDestFolder
   if the result is not empty then
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, "Could not create package folder:" && the result
      exit _hxtpmLCSDownloadComplete
   end if

   -- Move downloaded file into the package folder
   put tDestFolder & slash & tFilename into tDestPath
   rename file tTempPath to tDestPath
   if the result is not empty then
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, "Could not save package file:" && the result
      exit _hxtpmLCSDownloadComplete
   end if

   -- Write hxtpm metadata (includes source so updates hit the right repo)
   put _writeMeta(tDestFolder, tName, tVersion, "lcs", tSourceName) into tErr
   if tErr is not empty then
      dispatch tMsg to tCallerTarget with tName, tErr
      exit _hxtpmLCSDownloadComplete
   end if

   -- Load the library immediately
   try
      start using stack tDestPath
   catch tError
      dispatch tMsg to tCallerTarget with tName, "Package installed but failed to load:" && tError
      exit _hxtpmLCSDownloadComplete
   end try

   dispatch tMsg to tCallerTarget with tName, empty
end _hxtpmLCSDownloadComplete

private command _installLCEBegin pName, pVersion, pVersionData, pSourceName, pSourceBase, pCallerTarget, pCompletionMessage
   local tFileURL
   put _resolveFileURL(pVersionData["source"], pSourceBase) into tFileURL

   -- Temp file must be in the Extensions folder: extensionValidateLCEPackage
   -- uses revZipOpenArchive internally and cannot read from macOS-managed
   -- TemporaryItems paths. Unique name avoids stale extension manager cache entries
   -- (revIDEExtensionInstall caches by download_package_path).
   local tTempPath
   put revEnvironmentUserExtensionsPath() & slash & pName & "-" & pVersion & "-" & the seconds & ".lce" into tTempPath

   -- Record state so the download callback can resume the install
   put pName              into sPendingInstalls[tFileURL]["name"]
   put pVersion           into sPendingInstalls[tFileURL]["version"]
   put pVersionData       into sPendingInstalls[tFileURL]["versionData"]
   put pSourceName        into sPendingInstalls[tFileURL]["sourceName"]
   put tTempPath          into sPendingInstalls[tFileURL]["tempPath"]
   put "lce"              into sPendingInstalls[tFileURL]["type"]
   put pCallerTarget      into sPendingInstalls[tFileURL]["callerTarget"]
   put pCompletionMessage into sPendingInstalls[tFileURL]["completionMessage"]

   libURLDownloadToFile tFileURL, tTempPath, "_hxtpmLCEDownloadComplete"
end _installLCEBegin

on _hxtpmLCEDownloadComplete pURL, pStatus
   local tState
   put sPendingInstalls[pURL] into tState
   delete variable sPendingInstalls[pURL]

   local tName, tVersion, tVersionData, tSourceName, tTempPath, tCallerTarget, tMsg
   put tState["name"]              into tName
   put tState["version"]       into tVersion
   put tState["versionData"]   into tVersionData
   put tState["sourceName"]        into tSourceName
   put tState["tempPath"]          into tTempPath
   put tState["callerTarget"]      into tCallerTarget
   put tState["completionMessage"] into tMsg

   if pStatus is not "downloaded" then
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, "Download failed for" && tName & ":" && pStatus
      exit _hxtpmLCEDownloadComplete
   end if

   -- Verify checksum before handing off to the IDE
   local tErr
   put _verifyChecksum(tTempPath, tVersionData["checksum"]) into tErr
   if tErr is not empty then
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, tErr
      exit _hxtpmLCEDownloadComplete
   end if

   -- Validate the package structure before handing off to the IDE.
   -- revIDEExtensionInstall swallows the real error (shows a dialog with no detail);
   -- calling extensionValidateLCEPackage directly lets us surface the actual reason.
   local tManifest
   extensionValidateLCEPackage tTempPath, tManifest
   if the result is not empty then
      local tValidateErr
      put the result into tValidateErr
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, "LCE package validation failed for" && tName & ":" && tValidateErr
      exit _hxtpmLCEDownloadComplete
   end if

   -- Delegate to the IDE extension manager.
   -- revIDEExtensionInstall extracts the .lce zip, copies the .lci interface
   -- file to the interfaces folder, then calls 'load extension from file' on
   -- the compiled module — no IDE restart required.
   revIDEExtensionInstall tTempPath
   delete file tTempPath

   -- Confirm success by checking for the versioned extension folder the IDE
   -- creates at <userExtensions>/<name>.<version>/
   local tExtFolder
   put revEnvironmentUserExtensionsPath() & slash & tName & "." & tVersion into tExtFolder
   if there is not a folder tExtFolder then
      dispatch tMsg to tCallerTarget with tName, "Extension install failed. Check the Extension Manager for details."
      exit _hxtpmLCEDownloadComplete
   end if

   -- Write hxtpm metadata into the installed extension folder
   put _writeMeta(tExtFolder, tName, tVersion, "lce", tSourceName) into tErr
   if tErr is not empty then
      dispatch tMsg to tCallerTarget with tName, tErr
      exit _hxtpmLCEDownloadComplete
   end if

   dispatch tMsg to tCallerTarget with tName, empty
end _hxtpmLCEDownloadComplete

private command _installLCPBegin pName, pVersion, pVersionData, pSourceName, pSourceBase, pCallerTarget, pCompletionMessage
   local tFileURL
   put _resolveFileURL(pVersionData["source"], pSourceBase) into tFileURL

   -- Derive the final filename from the source URL so the plugin stack has its
   -- original name when placed in the plugins folder
   local tFilename
   put _filenameFromURL(pVersionData["source"]) into tFilename

   -- Temp file sits in the plugins folder so the final rename stays on the
   -- same filesystem (avoids cross-device rename errors)
   local tTempPath
   put revEnvironmentUserPluginsPath() & slash & pName & "-" & pVersion & "-" & the seconds & ".download" into tTempPath

   -- Record state so the download callback can resume the install
   put pName              into sPendingInstalls[tFileURL]["name"]
   put pVersion           into sPendingInstalls[tFileURL]["version"]
   put pVersionData       into sPendingInstalls[tFileURL]["versionData"]
   put pSourceName        into sPendingInstalls[tFileURL]["sourceName"]
   put tTempPath          into sPendingInstalls[tFileURL]["tempPath"]
   put "lcp"              into sPendingInstalls[tFileURL]["type"]
   put tFilename          into sPendingInstalls[tFileURL]["filename"]
   put pCallerTarget      into sPendingInstalls[tFileURL]["callerTarget"]
   put pCompletionMessage into sPendingInstalls[tFileURL]["completionMessage"]

   libURLDownloadToFile tFileURL, tTempPath, "_hxtpmLCPDownloadComplete"
end _installLCPBegin

on _hxtpmLCPDownloadComplete pURL, pStatus
   local tState
   put sPendingInstalls[pURL] into tState
   delete variable sPendingInstalls[pURL]

   local tName, tVersion, tVersionData, tSourceName, tTempPath, tFilename, tCallerTarget, tMsg
   put tState["name"]              into tName
   put tState["version"]           into tVersion
   put tState["versionData"]       into tVersionData
   put tState["sourceName"]        into tSourceName
   put tState["tempPath"]          into tTempPath
   put tState["filename"]          into tFilename
   put tState["callerTarget"]      into tCallerTarget
   put tState["completionMessage"] into tMsg

   if pStatus is not "downloaded" then
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, "Download failed for" && tName & ":" && pStatus
      exit _hxtpmLCPDownloadComplete
   end if

   -- Verify checksum
   local tErr
   put _verifyChecksum(tTempPath, tVersionData["checksum"]) into tErr
   if tErr is not empty then
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, tErr
      exit _hxtpmLCPDownloadComplete
   end if

   -- Move the plugin file to its final name in the plugins folder.
   -- The temp file is already in the plugins folder so this is an in-place rename.
   local tDestPath
   put revEnvironmentUserPluginsPath() & slash & tFilename into tDestPath
   rename file tTempPath to tDestPath
   if the result is not empty then
      delete file tTempPath
      dispatch tMsg to tCallerTarget with tName, "Could not save plugin file:" && the result
      exit _hxtpmLCPDownloadComplete
   end if

   -- Create .hxtpm/<name> metadata folder (create parent first if needed)
   local tHxtpmFolder, tMetaFolder
   put revEnvironmentUserPluginsPath() & slash & ".hxtpm" into tHxtpmFolder
   put tHxtpmFolder & slash & tName into tMetaFolder
   if there is not a folder tHxtpmFolder then
      create folder tHxtpmFolder
      if the result is not empty then
         delete file tDestPath
         dispatch tMsg to tCallerTarget with tName, "Could not create plugin metadata folder:" && the result
         exit _hxtpmLCPDownloadComplete
      end if
   end if
   create folder tMetaFolder
   if the result is not empty then
      delete file tDestPath
      dispatch tMsg to tCallerTarget with tName, "Could not create plugin metadata folder:" && the result
      exit _hxtpmLCPDownloadComplete
   end if

   -- Write hxtpm metadata; store filename so uninstall knows which file to remove
   local tExtra
   put tFilename into tExtra["filename"]
   put _writeMeta(tMetaFolder, tName, tVersion, "lcp", tSourceName, tExtra) into tErr
   if tErr is not empty then
      dispatch tMsg to tCallerTarget with tName, tErr
      exit _hxtpmLCPDownloadComplete
   end if

   -- Refresh the IDE plugin registry so the plugin is visible immediately
   revIDEUpdatePlugins

   dispatch tMsg to tCallerTarget with tName, empty
end _hxtpmLCPDownloadComplete

private function _uninstallLCP pName
   -- Read metadata to find which file to remove
   local tMetaPath, tMeta
   put _metaPath(pName, "lcp") into tMetaPath
   if there is a file tMetaPath then
      put JsonImport(URL ("file:" & tMetaPath)) into tMeta
   end if

   -- Remove the plugin stack file from the plugins folder
   if tMeta["filename"] is not empty then
      local tPluginFile
      put revEnvironmentUserPluginsPath() & slash & tMeta["filename"] into tPluginFile
      if there is a file tPluginFile then
         delete file tPluginFile
         if the result is not empty then
            return "Could not remove plugin file:" && the result
         end if
      end if
   end if

   -- Remove the per-package metadata subfolder
   local tMetaFolder
   put revEnvironmentUserPluginsPath() & slash & ".hxtpm" & slash & pName into tMetaFolder
   if there is a folder tMetaFolder then
      local tErr
      put _deleteFolder(tMetaFolder) into tErr
      if tErr is not empty then return tErr
   end if

   -- Refresh the IDE plugin registry
   revIDEUpdatePlugins

   return empty
end _uninstallLCP

private function _uninstallLCS pName
   local tFolder
   put revEnvironmentUserLibraries() & slash & pName into tFolder

   -- Stop using the library before removing files
   repeat for each line tFile in files(tFolder)
      if tFile ends with ".livecodescript" then
         try
            stop using stack (tFolder & slash & tFile)
         catch tError
            -- Not fatal — the file will still be removed
         end try
         exit repeat
      end if
   end repeat

   -- Remove the package folder
   local tErr
   put _deleteFolder(tFolder) into tErr
   if tErr is not empty then
      return "Could not remove package folder:" && tErr
   end if

   return empty
end _uninstallLCS

private function _uninstallLCE pName
   -- Delegate to the IDE extension manager which handles unloading the
   -- compiled module and removing the entire versioned extension folder.
   -- We do not check the result directly: revIDEExtensionUninstall can
   -- return non-fatal internal errors (e.g. 2205 from IDE UI cleanup) even
   -- when the extension was removed successfully. Instead, confirm success
   -- by checking that the versioned extension folder no longer exists.
   local tMetaPath, tExtFolder
   put _findLCEMetaPath(pName) into tMetaPath
   if tMetaPath is not empty then
      set the itemdel to slash
      put the item 1 to -2 of tMetaPath into tExtFolder
   end if

   revIDEExtensionUninstall pName

   -- If we could locate the folder before, confirm it is gone now
   if tExtFolder is not empty and there is a folder tExtFolder then
      return "Extension uninstall failed: folder still present after uninstall"
   end if
   return empty
end _uninstallLCE

-- Locate the hxtpm.json file for an installed LCE package.
-- The IDE extension manager installs to <userExtensions>/<name>.<version>/
-- so we scan for any subfolder of that form that contains hxtpm.json.
private function _findLCEMetaPath pName
   local tExtFolder
   put revEnvironmentUserExtensionsPath() into tExtFolder
   repeat for each line tSub in folders(tExtFolder)
      if tSub begins with pName & "." then
         local tMeta
         put tExtFolder & slash & tSub & slash & "hxtpm.json" into tMeta
         if there is a file tMeta then return tMeta
      end if
   end repeat
   return empty
end _findLCEMetaPath

-- ---------------------------------------------------------------------------
-- Private: metadata
-- ---------------------------------------------------------------------------

private function _metaPath pName, pType
   if pType is "lcs" then
      return revEnvironmentUserLibraries() & slash & pName & slash & "hxtpm.json"
   else if pType is "lcp" then
      -- Plugin metadata lives in a hidden .hxtpm subfolder inside the plugins
      -- folder so the IDE plugin scanner (which uses files()) never sees it.
      return revEnvironmentUserPluginsPath() & slash & ".hxtpm" & slash & pName & slash & "hxtpm.json"
   end if
   return revEnvironmentUserExtensionsPath() & slash & pName & slash & "hxtpm.json"
end _metaPath

private function _readInstalledMeta pName
   local tPath
   put _metaPath(pName, "lcs") into tPath
   if there is not a file tPath then
      put _findLCEMetaPath(pName) into tPath
   end if
   if there is not a file tPath then
      put _metaPath(pName, "lcp") into tPath
   end if
   if there is not a file tPath then return empty
   return JsonImport(URL ("file:" & tPath))
end _readInstalledMeta

-- pExtra is an optional array of additional key-value pairs to include in the
-- written metadata (e.g. ["filename": "MyPlugin.rev"] for lcp packages).
private function _writeMeta pFolder, pName, pVersion, pType, pSourceName, pExtra
   local tMeta
   put pName       into tMeta["name"]
   put pVersion    into tMeta["version"]
   put pType       into tMeta["type"]
   put pSourceName into tMeta["source"]
   put the short date into tMeta["installed"]

   if pExtra is an array then
      repeat for each key tKey in pExtra
         put pExtra[tKey] into tMeta[tKey]
      end repeat
   end if

   put JsonExport(tMeta) into URL ("file:" & pFolder & slash & "hxtpm.json")
   if the result is not empty then
      return "Could not write package metadata:" && the result
   end if
   return empty
end _writeMeta

private command _scanInstalledFolder pFolder, @xResult
   repeat for each line tSubfolder in folders(pFolder)
      if tSubfolder begins with "." then next repeat
      local tMetaPath
      put pFolder & slash & tSubfolder & slash & "hxtpm.json" into tMetaPath
      if there is a file tMetaPath then
         local tMeta
         put JsonImport(URL ("file:" & tMetaPath)) into tMeta
         if tMeta["name"] is not empty then
            put tMeta into xResult[tMeta["name"]]
         end if
      end if
   end repeat
end _scanInstalledFolder

-- ---------------------------------------------------------------------------
-- Private: utilities
-- ---------------------------------------------------------------------------

private function _verifyChecksum pFilePath, pExpected
   if pExpected is empty or pExpected begins with "sha256:placeholder" then
      return empty -- no checksum to verify
   end if

   local tData, tDigest, tHex, tByte
   put URL ("file:" & pFilePath) into tData
   put messageDigest(tData, "SHA-256") into tDigest

   repeat with i = 1 to the length of tDigest
      put baseConvert(charToNum(char i of tDigest), 10, 16) into tByte
      if the length of tByte is 1 then put "0" before tByte
      put tByte after tHex
   end repeat

   -- Normalise to lowercase for comparison — baseConvert returns uppercase,
   -- but stored checksums are conventionally lowercase (e.g. from shasum -a 256).
   put toLower(tHex) into tHex
   if ("sha256:" & tHex) is not toLower(pExpected) then
      return "Checksum mismatch: expected" && pExpected & ", got sha256:" & tHex
   end if
   return empty
end _verifyChecksum

private function _filenameFromURL pURL
   local tSaved, tResult
   put the itemDel into tSaved
   set the itemDel to "/"
   put the last item of pURL into tResult
   set the itemDel to tSaved
   return tResult
end _filenameFromURL

-- Recursively delete a folder and all its contents.
private function _deleteFolder pFolder
   repeat for each line tFile in files(pFolder)
      delete file (pFolder & slash & tFile)
      if the result is not empty then
         return "Could not delete file" && tFile & ":" && the result
      end if
   end repeat

   repeat for each line tSub in folders(pFolder)
      if tSub is "." or tSub is ".." then next repeat
      local tErr
      put _deleteFolder(pFolder & slash & tSub) into tErr
      if tErr is not empty then return tErr
   end repeat

   delete folder pFolder
   if the result is not empty then
      return "Could not delete folder" && pFolder & ":" && the result
   end if
   return empty
end _deleteFolder
