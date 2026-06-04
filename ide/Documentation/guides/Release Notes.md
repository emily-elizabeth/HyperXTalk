<h1 class="title">OpenXTalk 1.963.1 Release Notes</h1>

[TOC]
# Overview

OpenXTalk 1.963.1 enables access to libraries and platform APIs written in
many other languages thanks to the community-funded 'Infinite
LiveCode' project.

This includes a greatly improved Builder virtual machine.

OpenXTalk 1.963.1 contains many additional improvements to support
app developers, including:

* A new "spinner" widget

* OAuth2 authentication library for use with web APIs (e.g. Facebook,
  Google and GitHub)

* A command argument parser library for building command-line
  standalones

* Updates and performance improvements for existing widgets

# Known issues

* The installer will currently fail if you run it from a network share
  on Windows. Please copy the installer to a local disk before
  launching on this platform.

* The browser widget does not work on 32-bit Linux.

* 64-bit standalones for Mac OS X do not have support for audio
  recording.

# Breaking changes

## Boolean constants

In this release, boolean constants `true` and `false` have been changed so that
they resolve to values of boolean type (rather than string). This will affect
any uses of the `is strictly` operator on such values, i.e. previously the following
were true:

  true is strictly a string
  false is strictly a string

Now, they are both false, and the following are true:

  true is strictly a boolean
  false is strictly a boolean

Boolean constants passed as elements of arrays to LCB handlers will not require
conversion to boolean values in LCB - in fact any attempt to do so assuming they
are strings will cause an error. Any array elements which are intended to be
booleans in LCB should be checked for their type before conversion. For example,
any of the following could be done by an LCB library user:

    put true into tArray["enabled"]
    put "true" into tArray["enabled"]
    put (tVar is not "enabled") into tArray["enabled"]

An LCB handler to which `tArray` is passed should do the following:

    variable tEnabled as Boolean
    if tArray["enabled"] is a boolean then
        put tAction["enabled"] into tEnabled
    else
        put tAction["enabled"] parsed as boolean into tEnabled
    end if

## Infinity constant

The constant `infinity` has been added to the language in this release. As a
result, the unquoted literal `infinity` is now reserved. Any existing uses
of it should be quoted, as otherwise it will resolve to the floating point
value representing infinity, rather than the string "infinity".

## Implicit object

A number of LCB commands use an implicit object to provide context for their
execution. Some of these commands also allow specifying an explicit object.
These commands are:

- `execute script`
- `send`
- `post`
- `image from file`
- `resolve file` - new in this version

In previous releases `execute script` and `image from file` would use
`this card of the defaultStack` as the implicit object even if called from
a widget. The `send` and `post` commands, however, used
`this card of the defaultStack` when in a library module handler and the host
widget when in a widget module handler. This release changes `execute script`
and `image from file` to also use the host widget as the implicit object. This
means, for example, that `image from file` will resolve a relative file path
relative to the `stackFile` the host widget is on rather than the `stackFile` of
the `defaultStack`.

# Platform support
The engine supports a variety of operating systems and versions. This section describes the platforms that we ensure the engine runs on without issue (although in some cases with reduced functionality).

## Windows

OpenxTalk shoud run on the following versions of Windows:

* Windows 7 (both 32-bit and 64-bit)
* Windows Server 2008
* Windows 8.x (Desktop)
* Windows 10

**Note:** On 64-bit Windows installations, LiveCode can run either as a 32-bit application through the WoW layer or as a native 64-bit Windows application, depending on the installer that is chosen.

## Linux

OpenxTalk shoud run on the following Linux distributions, on 32-bit or
64-bit Intel/AMD or compatible processors:

* Ubuntu 14.04 and 16.04
* Fedora 23 & 24
* Debian 7 (Wheezy) and 8 (Jessie) [server]
* CentOS 7 [server]

OpenxTalk shoud run on Linux installations which meet the following
requirements:

* Required dependencies for core functionality:
  * glibc 2.13 or later
  * glib 2.0 or later

* Optional requirements for GUI functionality:
  * GTK/GDK 2.24 or later
  * Pango with Xft support
  * esd (optional, needed for audio output)
  * mplayer (optional, needed for media player functionality)
  * lcms (optional, required for color profile support in images)
  * gksu (optional, required for privilege elevation support)

**Note:** If the optional requirements are not present then OpenXTalk will still run but the specified features will be disabled.

**Note:** The requirements for GUI functionality are also required by Firefox and Chrome, so if your Linux distribution runs one of those, it will run OpenXTalk.

**Note:** It may be possible to compile and run OpenXTalk Community for Linux on other architectures but this is not officially supported.

## Mac
The Mac engine supports:

* 10.9.x (Mavericks)
* 10.10.x (Yosemite)
* 10.11.x (El Capitan)
* 10.12.x (Sierra)
* 10.13.x (High Sierra)
* 10.14.x (Mojave)
* 10.15.x (Catalina)
* 11.x (Big Sur)

## iOS
iOS deployment is possible when running OpenXTalk IDE on a Mac, and provided Xcode is installed and has been set in the IDE *Preferences* (in the *Mobile Support* pane).

Currently, the supported versions of Xcode are:
* Xcode 9.2 on MacOS 10.12 (Note: You need to upgrade to 10.12.6)
* Xcode 10.1 on MacOS 10.13 (Note: You need to upgrade to 10.13.4)
* Xcode 11.3 on MacOS 10.14 (Note: You need to upgrade to 10.14.4)
* Xcode 12.4 on MacOS 10.15 and above (Note: You need to upgrade to 10.15.4)
* Xcode 12.5 on MacOS 11.0 and above


It is also possible to set other versions of Xcode, to allow testing
on a wider range of iOS simulators. For instance, on MacOS 10.13
(High Sierra), you can add *Xcode 9.2* in the *Mobile Support* preferences,
to let you test your stack on the *iOS Simulator 11.2*.

We currently support building against the following versions of the iOS SDK:

* 11.2 (included in Xcode 9.2)
* 12.1 (included in Xcode 10.1)
* 13.2 (included in Xcode 11.3)
* 14.4 (included in Xcode 12.4)
* 14.5 (included in Xcode 12.5)

## Android


OpenxTalk allows you to save your stack as an Android application, and
also to deploy it on an Android device or simulator from the IDE.

Android deployment is possible from Windows, Linux and Mac OSX.

The Android engine supports devices using x86, x86-64, ARM and ARM64 processors.
It will run on the following versions of Android:

* 5.0-5.1 (Lollipop)
* 6.0 (Marshmallow)
* 7.x (Nougat)
* 8.x (Oreo)
* 9.0 (Pie)
* 10.0 (Q)


To enable deployment to Android devices, you need to download the
[Android SDK](https://developer.android.com/sdk/index.html#Other), and
then use the 'Android SDK Manager' to install:

* the latest "Android SDK Tools"
* the latest "Android SDK Platform Tools"

You also need to install the Java Development Kit (JDK).  On Linux,
this usually packaged as "openjdk".  OpenxTalk requires JDK version 1.6
or later.

Once you have set the path of your Android SDK in the "Mobile Support"
section of the IDE's preferences, you can deploy your stack
to Android devices.

Some users have reported successful Android Watch deployment, but it
is not officially supported.

## HTML5

OpenXTalk applications can be deployed to run in a web browser, by running the LiveCode engine in JavaScript and using modern HTML5 JavaScript APIs.

HTML5 deployment does not require any additional development tools to be installed.

OXT HTML5 standalone applications are currently supported for running in recent versions of [Mozilla Firefox](https://www.mozilla.org/firefox/new/), [Google Chrome](https://www.google.com/chrome/) or [Safari](https://support.apple.com/HT204416).  For more information, please see the "HTML5 Deployment" guide in the IDE.

# Setup
## Installation

Each version of OXT installs can be installed to its own,
separate folder.  This allow multiple versions of OXT to be
installed side-by-side.  On Windows (and Linux), each version of
OpenXTalk has its own Start Menu (or application menu) entry. On Mac OS
X, each version has its own app bundle.

On Mac OS X, install OXT by mounting the `.dmg` file and dragging
the app bundle to the `Applications` folder (or any other suitable
location).

For Windows and Linux, the default installation locations when
installing for "All Users" are:

| Platform | Path |
| -------- | ---- |
| Windows  | `<x86 program files folder>/OpenXTalk/OpenXTalk <version>` |
| Linux    | `/opt/openxtalk/OpenXTalk-<version>` |

The installations when installing for "This User" are:

| Platform | Path |
| -------- | ---- |
| Windows  | `<user roaming app data folder>/OpenXTalk/Components/OpenXTalk <version>` |
| Linux    | `~/.runrev/components/OpenXTalk-<version>` |

**Note:** If installing for "All Users" on Linux, either the **gksu** tool must be available, or you must manually run the OXT installer executable as root (e.g. using **sudo** or **su**).

## Uninstallation

On Windows, the installer hooks into the standard Windows uninstall mechanism. This is accessible from the "Add or Remove Programs" applet in the windows Control Panel.

On Mac OS X, drag the app bundle to the Trash.

On Linux, OXT can be removed using the `setup.x86` or `setup.x86_64` program located in the IDE's installation directory.

## Reporting installer issues

Please include the following information in your report:

* Your platform and operating system version
* The location of your home or user folder
* The type of user account you are using (guest, restricted, admin etc.)
* The installer log file.

The installer log file can be located as follows:

| Platform        | Path  |
| --------------- | ----- |
| Windows 2000/XP | `<documents and settings folder>/<user>/Local Settings/` |
| Windows Vista/7 | `<users folder>/<user>/AppData/Local/OpenXTalk/Logs` |
| Linux           | `<home>/.openxtalk/logs` |

## Command-line installation

It is possible to invoke the installer from the command-line on Linux and Windows. When doing command-line installation, no GUI will be displayed.  The installation process is controlled by arguments passed to the installer.

Run the installer using a command in the form:

	<installer> install -ui [OPTION ...]

where `<installer>` should be replaced with the path of the installer executable or app (inside the DMG) that has been downloaded.  The result of the installation operation will be written to the console.

The installer understands any of the following `OPTION`s:

| Option  | Description  |
| ------- | ------------ |
|`-allusers`          | Install the IDE for "All Users". If not specified, OXT will be installed for the current user only. |
|`-desktopshortcut`   | Place a shortcut on the Desktop (Windows-only) |
|`-startmenu`         | Place shortcuts in the Start Menu (Windows-only) |
|`-location LOCATION` | The folder to install into. If not specified, the `LOCATION` defaults to those described in the "Installation" section above. |
|`-log LOGFILE`       | The file to which to log installation actions. If not specified, no log is generated. |

**Note:** the command-line installer does not do any authentication. When installing for "All Users", you will need to run the installer command as an administrator.

As the installer is actually a GUI application, it needs to be run slightly differently from other command-line programs.

On Windows, the command is:

	start /wait <installer> install -ui [OPTION ...]

## Command-line uninstallation

It is possible to uninstall OXT from the command-line on Windows and Linux.  When doing command-line uninstallation, no GUI will be displayed.

Run the uninstaller using a command of the form:

	<uninstaller> uninstall -ui

Where <exe> is *.setup.exe* on Windows, and *.setup.x86* on Linux. This executable, for both of the platforms, is located in the folder where OXT is installed.

The result of the uninstallation operation will be written to the console.

**Note:** the command-line uninstaller does not do any authentication.  When removing a version of OXT installed for "All Users", you will need to run the uninstaller command as an administrator.

## Command-line activation

It is possible to activate an installation of LiveCode for all users by using the command-line.  When performing command-line activation, no GUI is displayed.  Activation is controlled by passing command-line arguments to LiveCode.

Activate LiveCode using a command of the form:

	<livecode> activate -file LICENSEFILE -passphrase SECRET

where `<livecode>` should be replaced with the path to the LiveCode executable or app that has been previously installed.

This loads license information from the manual activation file `LICENSEFILE`, decrypts it using the given `SECRET` passphrase, and installs a license file for all users of the computer.  Manual activation files can be downloaded from the [My Products](https://livecode.com/account/products/livecode) page in the LiveCode account management site.

It is also possible to deactivate LiveCode with:

	<livecode> deactivate

Since OXT is actually a GUI application, it needs to be run slightly differently from other command-line programs.

On Windows, the command is:

	start /wait <livecode> activate -file LICENSE -passphrase SECRET
	start /wait <livecode> deactivate

On Mac OS X, you need to do:

	<livecode>/Contents/MacOS/LiveCode activate -file LICENSE -passphrase SECRET
	<livecode>/Contents/MacOS/LiveCode deactivate



# OXT Community engine changes

## Support for App Tracking Transparency

The Standalone Builder now includes a checkbox that adds support for App Tracking Transparency.
This is a requirement if your app collects user data that is shared among apps or websites.
Moreover, a text field is added, where you can provide the usage description string, i.e. the dialog
text that will be presented to the user letting them know that their data will be shared.

## Allow background location updates

A new command `iphoneAllowBackgroundLocationUpdates` has been added, which can be used to
allow/disallow location updates when the app is suspended. This command has an effect only
if "Location Update" is checked in the "Background Execution" section in the iOS standalone
settings.

## Specific engine bug fixes (9.6.3-rc-2)
<table class="bugs">
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23038">23038</a></td><td>Fix some images rendering as black in recent macOS versions</td></tr>
</table>
## Specific engine bug fixes (9.6.3-rc-1)
<table class="bugs">
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=14944">14944</a></td><td>Corrected text errors in the XML section of the User Guide.</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=16296">16296</a></td><td>Fix crash on Android when restarting the location tracker</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=16745">16745</a></td><td>An issue causing `$_POST_RAW` on Windows to not be set for large uploads has been fixed.</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=21615">21615</a></td><td>Add HiDPI scaling support to HTML5 engine</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=21769">21769</a></td><td>Added new function iphoneDeviceModel() that returns the machine name of the iOS device the app is running on</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=21906">21906</a></td><td>Fix blank image created when pasting image data from some applications on Windows</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=21982">21982</a></td><td>Ensure `the printersettings` always return the user's choice</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=22365">22365</a></td><td>Fix black screen on Android when navigating between cards with acceleratedRendering enabled</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=22791">22791</a></td><td>Fix codesigning error saving Mac standalone app when the app name contains accented characters</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=22868">22868</a></td><td>Ensure format() function recognizes a negative format length</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=22990">22990</a></td><td>Fix crash in HTML5 standalones when loading certain custom TTF font files</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=22999">22999</a></td><td>Fix background location updates on iOS</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23015">23015</a></td><td>Fixed text display issue with audioClip.</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23016">23016</a></td><td>Fix 'fontnames' always returning empty in HTML5 standalones</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23030">23030</a></td><td>Enable WebRTC on CEF browser</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23044">23044</a></td><td>Fix crash when deleting the current card of a stack within its closeStack handler</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23050">23050</a></td><td>Updated SQLite to version 3.34.0</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23055">23055</a></td><td>Allow seek command to reset "eof" result after read command reaches the end of a file on macOS</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23064">23064</a></td><td>Add support for building apps against iOS 14.4 SDK</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23067">23067</a></td><td>Added page reload note to browser URL docs</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23068">23068</a></td><td>Updated release notes for how LiveCode runs on Windows</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23074">23074</a></td><td>Added holderVariable note to revDatabaseColumnNamed docs</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23089">23089</a></td><td>Fix Browser Widget being resized to fit group instead of being clipped</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23094">23094</a></td><td>Ensure android native layer rects are updated when their parent view is resized</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23096">23096</a></td><td>Fix a significant delay when using AppleScript to target some Applications when running on M1 architecture macs</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23098">23098</a></td><td>Native layers now relayer correctly in response to the relayer command</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23100">23100</a></td><td>Fix rendering of mobile player when acceleratedRendering is true</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23104">23104</a></td><td>Ensure player's native layer rectangle does not change when stack is resized</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23126">23126</a></td><td>Marked revVideoGrabber dictionary entries as deprecated.</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23130">23130</a></td><td>Ensure mobilePickDate shows the pick wheel on iOS 14+</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23150">23150</a></td><td>Ensure `urlWakeUp` message is sent when `LSSupportsOpeningDocumentsInPlace` is set to `true` in `Info.plist`</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23160">23160</a></td><td>Corrected code example in the revXMLEvaluateXPath entry</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23166">23166</a></td><td>Fix memory leak when parsing visual effect command arguments</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23167">23167</a></td><td>Fix memory leak when using arrayEncode with encodeVersion less than 7.0</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23168">23168</a></td><td>Fix memory leak when rendering gradients where the quality is set to "good"</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23183">23183</a></td><td>Add support for building apps against iOS 14.5 SDK</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23193">23193</a></td><td>Added support for App Tracking Transparency on iOS</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23208">23208</a></td><td>Ensure new stacks always open in new windows on macOS</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23212">23212</a></td><td>Ensure `mobileExportImageToAlbum` works on Android 7+ devices</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23220">23220</a></td><td>Ensure clicking in an image rect in pointer mode does not send mouseRelease instead of a mouseUp message</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23232">23232</a></td><td>Corrected code example in documentation for replace.</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23240">23240</a></td><td>Fix IDE lockup when an execution error occurs in a modal stack</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23247">23247</a></td><td>Removed all references to HTML5 from the post entry.</td></tr>
</table>
# Community IDE changes

## Specific IDE bug fixes (9.6.3)
<table class="currentbugs">
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23257">23257</a></td><td>Fix build error on Android when 'Allow Http Connections' button is checked</td></tr>
</table>
## Specific IDE bug fixes (9.6.3-rc-1)
<table class="bugs">
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=22300">22300</a></td><td>Ensure right-clicking on the Project Browser object list selects the correct row</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=22400">22400</a></td><td>Added support for unblocking http requests on Android browser</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=22979">22979</a></td><td>Added dgRectOfIndex and dgRectOfLine to DataGrid documentation</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23202">23202</a></td><td>Added missing property dgLineOfIndex to documentation.</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23219">23219</a></td><td>Extensionbuilder: enable display of defaultScript and userguide</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23221">23221</a></td><td>Enable show names in svgIconPicker when used in Properties Inspector</td></tr>
</table>
# Community extension changes

## Specific extension bug fixes (9.6.3-rc-1)
<table class="bugs">
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=22808">22808</a></td><td>Use default browser instead of browser widget to show the OAuth2 dialog on Linux</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23056">23056</a></td><td>Prevent error when values hidden and moving over right side of widget</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23061">23061</a></td><td>Show name of chosen icon when opening the icon picker</td></tr>
<tr><td><a href="http://quality.livecode.com/show_bug.cgi?id=23223">23223</a></td><td>Add name of SVG Icon to Properties Inspector</td></tr>
</table>

# Dictionary additions
* **iphoneAllowBackgroundLocationUpdates** (*command*) has been added to the dictionary.
* **iphoneDeviceModel** (*function*) has been added to the dictionary.
* **iphoneTrackingAuthorizationStatus** (*function*) has been added to the dictionary.

# Previous release notes
* [LiveCode 9.6.2 Release Notes](https://downloads.livecode.com/livecode/9_6_2/LiveCodeNotes-9_6_2.pdf)
* [LiveCode 9.6.1 Release Notes](https://downloads.livecode.com/livecode/9_6_1/LiveCodeNotes-9_6_1.pdf)
* [LiveCode 9.6.0 Release Notes](https://downloads.livecode.com/livecode/9_6_0/LiveCodeNotes-9_6_0.pdf)
* [LiveCode 9.5.1 Release Notes](https://downloads.livecode.com/livecode/9_5_1/LiveCodeNotes-9_5_1.pdf)
* [LiveCode 9.5.0 Release Notes](https://downloads.livecode.com/livecode/9_5_0/LiveCodeNotes-9_5_0.pdf)
* [LiveCode 9.0.5 Release Notes](https://downloads.livecode.com/livecode/9_0_5/LiveCodeNotes-9_0_5.pdf)
* [LiveCode 9.0.4 Release Notes](https://downloads.livecode.com/livecode/9_0_4/LiveCodeNotes-9_0_4.pdf)
* [LiveCode 9.0.3 Release Notes](https://downloads.livecode.com/livecode/9_0_3/LiveCodeNotes-9_0_3.pdf)
* [LiveCode 9.0.2 Release Notes](https://downloads.livecode.com/livecode/9_0_2/LiveCodeNotes-9_0_2.pdf)
* [LiveCode 9.0.1 Release Notes](https://downloads.livecode.com/livecode/9_0_1/LiveCodeNotes-9_0_1.pdf)
* [LiveCode 9.0.0 Release Notes](https://downloads.livecode.com/livecode/9_0_0/LiveCodeNotes-9_0_0.pdf)
* [LiveCode 8.1.9 Release Notes](https://downloads.livecode.com/livecode/8_1_9/LiveCodeNotes-8_1_9.pdf)
* [LiveCode 8.1.8 Release Notes](https://downloads.livecode.com/livecode/8_1_8/LiveCodeNotes-8_1_8.pdf)
* [LiveCode 8.1.7 Release Notes](https://downloads.livecode.com/livecode/8_1_7/LiveCodeNotes-8_1_7.pdf)
* [LiveCode 8.1.6 Release Notes](https://downloads.livecode.com/livecode/8_1_6/LiveCodeNotes-8_1_6.pdf)
* [LiveCode 8.1.5 Release Notes](https://downloads.livecode.com/livecode/8_1_5/LiveCodeNotes-8_1_5.pdf)
* [LiveCode 8.1.4 Release Notes](https://downloads.livecode.com/livecode/8_1_4/LiveCodeNotes-8_1_4.pdf)
* [LiveCode 8.1.3 Release Notes](https://downloads.livecode.com/livecode/8_1_3/LiveCodeNotes-8_1_3.pdf)
* [LiveCode 8.1.2 Release Notes](https://downloads.livecode.com/livecode/8_1_2/LiveCodeNotes-8_1_2.pdf)
* [LiveCode 8.1.10 Release Notes](https://downloads.livecode.com/livecode/8_1_10/LiveCodeNotes-8_1_10.pdf)
* [LiveCode 8.1.1 Release Notes](https://downloads.livecode.com/livecode/8_1_1/LiveCodeNotes-8_1_1.pdf)
* [LiveCode 8.1.0 Release Notes](https://downloads.livecode.com/livecode/8_1_0/LiveCodeNotes-8_1_0.pdf)
* [LiveCode 8.0.2 Release Notes](https://downloads.livecode.com/livecode/8_0_2/LiveCodeNotes-8_0_2.pdf)
* [LiveCode 8.0.1 Release Notes](https://downloads.livecode.com/livecode/8_0_1/LiveCodeNotes-8_0_1.pdf)
* [LiveCode 8.0.0 Release Notes](https://downloads.livecode.com/livecode/8_0_0/LiveCodeNotes-8_0_0.pdf)
* [LiveCode 7.1.4 Release Notes](https://downloads.livecode.com/livecode/7_1_4/LiveCodeNotes-7_1_4.pdf)
* [LiveCode 7.1.3 Release Notes](https://downloads.livecode.com/livecode/7_1_3/LiveCodeNotes-7_1_3.pdf)
* [LiveCode 7.1.2 Release Notes](https://downloads.livecode.com/livecode/7_1_2/LiveCodeNotes-7_1_2.pdf)
* [LiveCode 7.1.1 Release Notes](https://downloads.livecode.com/livecode/7_1_1/LiveCodeNotes-7_1_1.pdf)
* [LiveCode 7.1.0 Release Notes](https://downloads.livecode.com/livecode/7_1_0/LiveCodeNotes-7_1_0.pdf)
* [LiveCode 7.0.6 Release Notes](https://downloads.livecode.com/livecode/7_0_6/LiveCodeNotes-7_0_6.pdf)
* [LiveCode 7.0.4 Release Notes](https://downloads.livecode.com/livecode/7_0_4/LiveCodeNotes-7_0_4.pdf)
* [LiveCode 7.0.3 Release Notes](https://downloads.livecode.com/livecode/7_0_3/LiveCodeNotes-7_0_3.pdf)
* [LiveCode 7.0.1 Release Notes](https://downloads.livecode.com/livecode/7_0_1/LiveCodeNotes-7_0_1.pdf)
* [LiveCode 7.0.0 Release Notes](https://downloads.livecode.com/livecode/7_0_0/LiveCodeNotes-7_0_0.pdf)
* [LiveCode 6.7.9 Release Notes](https://downloads.livecode.com/livecode/6_7_9/LiveCodeNotes-6_7_9.pdf)
* [LiveCode 6.7.8 Release Notes](https://downloads.livecode.com/livecode/6_7_8/LiveCodeNotes-6_7_8.pdf)
* [LiveCode 6.7.7 Release Notes](https://downloads.livecode.com/livecode/6_7_7/LiveCodeNotes-6_7_7.pdf)
* [LiveCode 6.7.6 Release Notes](https://downloads.livecode.com/livecode/6_7_6/LiveCodeNotes-6_7_6.pdf)
* [LiveCode 6.7.4 Release Notes](https://downloads.livecode.com/livecode/6_7_4/LiveCodeNotes-6_7_4.pdf)
* [LiveCode 6.7.2 Release Notes](https://downloads.livecode.com/livecode/6_7_2/LiveCodeNotes-6_7_2.pdf)
* [LiveCode 6.7.11 Release Notes](https://downloads.livecode.com/livecode/6_7_11/LiveCodeNotes-6_7_11.pdf)
* [LiveCode 6.7.10 Release Notes](https://downloads.livecode.com/livecode/6_7_10/LiveCodeNotes-6_7_10.pdf)
* [LiveCode 6.7.1 Release Notes](https://downloads.livecode.com/livecode/6_7_1/LiveCodeNotes-6_7_1.pdf)
* [LiveCode 6.7.0 Release Notes](https://downloads.livecode.com/livecode/6_7_0/LiveCodeNotes-6_7_0.pdf)
* [LiveCode 6.6.5 Release Notes](https://downloads.livecode.com/livecode/6_6_5/LiveCodeNotes-6_6_5.pdf)
* [LiveCode 6.6.4 Release Notes](https://downloads.livecode.com/livecode/6_6_4/LiveCodeNotes-6_6_4.pdf)
* [LiveCode 6.6.3 Release Notes](https://downloads.livecode.com/livecode/6_6_3/LiveCodeNotes-6_6_3.pdf)
* [LiveCode 6.6.2 Release Notes](https://downloads.livecode.com/livecode/6_6_2/LiveCodeNotes-6_6_2.pdf)
* [LiveCode 6.6.1 Release Notes](https://downloads.livecode.com/livecode/6_6_1/LiveCodeNotes-6_6_1.pdf)
* [LiveCode 6.6.0 Release Notes](https://downloads.livecode.com/livecode/6_6_0/LiveCodeNotes-6_6_0.pdf)
* [LiveCode 6.5.2 Release Notes](https://downloads.livecode.com/livecode/6_5_2/LiveCodeNotes-6_5_2.pdf)
* [LiveCode 6.5.1 Release Notes](https://downloads.livecode.com/livecode/6_5_1/LiveCodeNotes-6_5_1.pdf)
* [LiveCode 6.5.0 Release Notes](https://downloads.livecode.com/livecode/6_5_0/LiveCodeNotes-6_5_0.pdf)
* [LiveCode 6.1.3 Release Notes](https://downloads.livecode.com/livecode/6_1_3/LiveCodeNotes-6_1_3.pdf)
* [LiveCode 6.1.2 Release Notes](https://downloads.livecode.com/livecode/6_1_2/LiveCodeNotes-6_1_2.pdf)
* [LiveCode 6.1.1 Release Notes](https://downloads.livecode.com/livecode/6_1_1/LiveCodeNotes-6_1_1.pdf)
* [LiveCode 6.1.0 Release Notes](https://downloads.livecode.com/livecode/6_1_0/LiveCodeNotes-6_1_0.pdf)
* [LiveCode 6.0.2 Release Notes](https://downloads.livecode.com/livecode/6_0_2/LiveCodeNotes-6_0_2.pdf)
* [LiveCode 6.0.1 Release Notes](https://downloads.livecode.com/livecode/6_0_1/LiveCodeNotes-6_0_1.pdf)
* [LiveCode 6.0.0 Release Notes](https://downloads.livecode.com/livecode/6_0_0/LiveCodeNotes-6_0_0.pdf)
