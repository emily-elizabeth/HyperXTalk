; ============================================================
; HyperXTalk Windows Installer
; Inno Setup 6.x  (https://jrsoftware.org/isinfo.php)
;
; Build steps:
;   1. Run build-release-x64.bat to produce the Release binaries.
;   2. Open this file in the Inno Setup Compiler (or run:
;        iscc.exe installer\HyperXTalk-setup.iss)
;   3. The installer is written to installer\output\.
; ============================================================

#define MyAppName      "HyperXTalk"
#define MyAppVersion   "0.9.15"
#define MyAppPublisher "HyperXTalk.com"
#define MyAppURL       "https://HyperXTalk.com"
#define MyAppExeName   "HyperXTalk.exe"
#define MyAppID        "7F3A2C1D-B4E5-4F89-A012-C3D456789ABC"

; Source tree — relative to this .iss file.
; The Release build script places all outputs here.
#define SourceDir "..\build-win-x86_64\hyperxtalk\Release"

; ============================================================
[Setup]
; -------- Identity --------
AppId={{{#MyAppID}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; -------- Install location --------
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes

; -------- Output --------
OutputDir=output
OutputBaseFilename=HyperXTalk-{#MyAppVersion}-win64-setup
SetupIconFile=..\engine\rsrc\installer.ico

; -------- Compression --------
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes

; -------- Platform --------
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; -------- UI --------
WizardStyle=modern
WizardSmallImageFile=application.png

; -------- Uninstall --------
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}

; -------- Misc --------
MinVersion=10.0
; Require Windows 10 or later (Per-Monitor DPI awareness requires Win10)

; ============================================================
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; ============================================================
[Tasks]
Name: "desktopicon"; \
    Description: "{cm:CreateDesktopIcon}"; \
    GroupDescription: "{cm:AdditionalIcons}"

Name: "fileassoc"; \
    Description: "Associate .hyperxtalk and .hyperxtalkscript files with {#MyAppName}"; \
    GroupDescription: "File associations"

; ============================================================
[Files]
; ---- Main executables ----
Source: "{#SourceDir}\HyperXTalk.exe";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\standalone-community.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\lc-compile.exe";           DestDir: "{app}"; Flags: ignoreversion

; ---- Standalone engine (runtime) ----
; revEngineCheck() in installed mode looks for Runtime\Windows\x86-64\Standalone
; (no .exe extension — LiveCode reads it as a raw PE image when building standalones).
; Without this file the Windows platform checkboxes in Standalone Application Settings
; are permanently disabled because revEngineCheck() returns false for all Windows targets.
Source: "{#SourceDir}\standalone-community.exe"; \
    DestDir: "{app}\Runtime\Windows\x86-64"; \
    DestName: "Standalone"; \
    Flags: ignoreversion

; ---- Runtime support DLLs for standalone builder ----
; The standalone builder (revSaveAsWindowsStandalone / revStandalonePlatformDetails) in
; installed mode resolves all support paths relative to Runtime\Windows\x86-64\.
; Files must be present there in addition to {app} root (which the IDE uses at run-time).

; ICU text-processing libraries — needed by standalone exe at runtime
Source: "{#SourceDir}\icudt58.dll";  DestDir: "{app}";                      Flags: ignoreversion
Source: "{#SourceDir}\icudt58.dll";  DestDir: "{app}\Runtime\Windows\x86-64"; Flags: ignoreversion
Source: "{#SourceDir}\icuin58.dll";  DestDir: "{app}";                      Flags: ignoreversion
Source: "{#SourceDir}\icuin58.dll";  DestDir: "{app}\Runtime\Windows\x86-64"; Flags: ignoreversion
Source: "{#SourceDir}\icutu58.dll";  DestDir: "{app}";                      Flags: ignoreversion
Source: "{#SourceDir}\icutu58.dll";  DestDir: "{app}\Runtime\Windows\x86-64"; Flags: ignoreversion
Source: "{#SourceDir}\icuuc58.dll";  DestDir: "{app}";                      Flags: ignoreversion
Source: "{#SourceDir}\icuuc58.dll";  DestDir: "{app}\Runtime\Windows\x86-64"; Flags: ignoreversion

; OpenSSL (IDE use only — not copied into standalones)
Source: "{#SourceDir}\libcrypto-3-x64.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\libssl-3-x64.dll";    DestDir: "{app}"; Flags: ignoreversion

; WebView2 — no WebView2Loader.dll needed; the static loader (WebView2LoaderStatic.lib)
; is compiled into HyperXTalk.exe and standalone-community.exe, so the DLL is not
; required at runtime.

; Support DLLs — revpdfprinter and revsecurity sit in {app} root (not Externals).
; The standalone builder (revCopyExternals) copies them from the Support subfolder of
; the runtime engine directory into the standalone output folder when the user opts in
; to PDF printing or SSL & Encryption.
Source: "{#SourceDir}\revpdfprinter.dll"; DestDir: "{app}";                                  Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\revpdfprinter.dll"; DestDir: "{app}\Runtime\Windows\x86-64\Support";   Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\revsecurity.dll";   DestDir: "{app}";                                  Flags: ignoreversion
Source: "{#SourceDir}\revsecurity.dll";   DestDir: "{app}\Runtime\Windows\x86-64\Support";   Flags: ignoreversion

; Database drivers — installed in two locations each:
;   {app}\Externals\Database Drivers\
;       revGetDatabaseDriverPath() (native, from revdb.dll) reads Database Drivers.txt
;       here to list available drivers at IDE runtime.
;   {app}\Runtime\Windows\x86-64\Externals\Database Drivers\
;       revDBDriverPath() in installed mode reads Database Drivers.txt here and
;       expects the DLL files to sit alongside it for the standalone builder.
Source: "{#SourceDir}\dbmysql.dll"; \
    DestDir: "{app}\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\dbmysql.dll"; \
    DestDir: "{app}\Runtime\Windows\x86-64\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
; libmysql.dll — runtime dependency of dbmysql.dll (libmysql.lib is an import lib,
; not a static lib, so the DLL must be present at load time).  Must be co-located
; with dbmysql.dll so Windows finds it when dbmysql.dll is loaded via LoadLibrary.
; libssl-3-x64.dll / libcrypto-3-x64.dll — libmysql.dll's OpenSSL dependencies.
;   * In the installed IDE, the versions in {app}\ (installed below) are found when
;     libmysql.dll searches the process EXE directory.
;   * In built standalones, Database Drivers.txt lists these DLLs so the standalone
;     builder copies them into Externals/database_drivers/ alongside libmysql.dll.
;     The Runtime\ copies below make them available to that copy step.
Source: "{#SourceDir}\libmysql.dll"; \
    DestDir: "{app}\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\libmysql.dll"; \
    DestDir: "{app}\Runtime\Windows\x86-64\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\libssl-3-x64.dll"; \
    DestDir: "{app}\Runtime\Windows\x86-64\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\libcrypto-3-x64.dll"; \
    DestDir: "{app}\Runtime\Windows\x86-64\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\dbodbc.dll"; \
    DestDir: "{app}\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\dbodbc.dll"; \
    DestDir: "{app}\Runtime\Windows\x86-64\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\dbpostgresql.dll"; \
    DestDir: "{app}\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\dbpostgresql.dll"; \
    DestDir: "{app}\Runtime\Windows\x86-64\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\dbsqlite.dll"; \
    DestDir: "{app}\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\dbsqlite.dll"; \
    DestDir: "{app}\Runtime\Windows\x86-64\Externals\Database Drivers"; \
    Flags: ignoreversion skipifsourcedoesntexist

; Database Drivers.txt index files — one for each install location above.
; The Runtime copy is picked up automatically by the ide\Runtime\Windows\* recursive rule below.
Source: "..\ide\Externals\Database Drivers\Database Drivers.txt"; \
    DestDir: "{app}\Externals\Database Drivers"; \
    Flags: ignoreversion

; LiveCode externals — two locations each:
;   {app}\Externals
;       revInternal__SetupExternals scans here at IDE startup to register LCB modules
;       (e.g. com.livecode.extensions.libbrowser).  revExternalsList() also reads
;       {app}\Externals\Externals.txt from here to list available externals.
;   {app}\Runtime\Windows\x86-64\Externals
;       revExternalPath() reads Externals.txt here to resolve DLL paths when the
;       standalone builder copies externals into a Windows standalone.
Source: "{#SourceDir}\revbrowser.dll"; DestDir: "{app}\Externals";                         Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\revbrowser.dll"; DestDir: "{app}\Runtime\Windows\x86-64\Externals";  Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\revdb.dll";      DestDir: "{app}\Externals";                         Flags: ignoreversion
Source: "{#SourceDir}\revdb.dll";      DestDir: "{app}\Runtime\Windows\x86-64\Externals";  Flags: ignoreversion
Source: "{#SourceDir}\revxml.dll";     DestDir: "{app}\Externals";                         Flags: ignoreversion
Source: "{#SourceDir}\revxml.dll";     DestDir: "{app}\Runtime\Windows\x86-64\Externals";  Flags: ignoreversion
Source: "{#SourceDir}\revspeech.dll";  DestDir: "{app}\Externals";                         Flags: ignoreversion
Source: "{#SourceDir}\revspeech.dll";  DestDir: "{app}\Runtime\Windows\x86-64\Externals";  Flags: ignoreversion
Source: "{#SourceDir}\revzip.dll";     DestDir: "{app}\Externals";                         Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\revzip.dll";     DestDir: "{app}\Runtime\Windows\x86-64\Externals";  Flags: ignoreversion skipifsourcedoesntexist

; Externals index files — read by revExternalsList() and revExternalPath() in installed mode.
; {app}\Externals\Externals.txt     → revExternalsList() (what externals are available)
; Runtime\…\Externals\Externals.txt → revExternalPath()  (DLL filename for each external)
Source: "..\ide\Externals\Externals.txt"; \
    DestDir: "{app}\Externals"; \
    Flags: ignoreversion
; Note: ide\Runtime\Windows\x86-64\Externals\Externals.txt and
;       ide\Runtime\Windows\x86-64\Externals\Database Drivers\Database Drivers.txt
; are picked up automatically by the ide\Runtime\Windows\* recursive rule below.

; Server externals (needed by the IDE's server-side script testing)
Source: "{#SourceDir}\server-revdb.dll";  DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\server-revxml.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\server-revzip.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Utility DLLs
Source: "{#SourceDir}\tz.dll";   DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\inih.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; ---- VLC runtime (optional — video playback) ----
; libvlc.dll / libvlccore.dll are copied by build-release-x64.bat from the
; VLC installation on the build machine.  vlc-plugins\ contains the codec
; and demux plugins that VLC needs at runtime.
;
; All three entries use skipifsourcedoesntexist so the installer still builds
; on machines where VLC is not installed — video playback will simply not work
; on those installs.
;
; EnsureVLCInstance() in vlc-player.cpp probes for plugins in this order:
;   1. <exedir>\vlc-plugins\plugins\   ← what we install below
;   2. <exedir>\vlc-plugins\
;   3. C:\Program Files\VideoLAN\VLC\plugins  (system VLC fallback)
Source: "{#SourceDir}\libvlc.dll"; \
    DestDir: "{app}"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\libvlccore.dll"; \
    DestDir: "{app}"; \
    Flags: ignoreversion skipifsourcedoesntexist
; MinGW runtime DLLs — VLC 3.x is GCC-compiled; these are NOT Windows
; system DLLs and must ship alongside libvlccore.dll.
Source: "{#SourceDir}\libgcc_s_seh-1.dll"; \
    DestDir: "{app}"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\libstdc++-6.dll"; \
    DestDir: "{app}"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\libwinpthread-1.dll"; \
    DestDir: "{app}"; \
    Flags: ignoreversion skipifsourcedoesntexist
Source: "{#SourceDir}\vlc-plugins\*"; \
    DestDir: "{app}\vlc-plugins"; \
    Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; ---- LCB modules (for extension compilation) ----
Source: "{#SourceDir}\modules\lci\*"; \
    DestDir: "{app}\modules\lci"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

; ---- Packaged extensions ----
; macOS-only, iOS-only and Emscripten-only extensions are excluded — they
; contain no Windows-compatible binaries and their api.lcdoc files would
; appear as orphaned entries in the Windows documentation browser.
Source: "{#SourceDir}\packaged_extensions\*"; \
    DestDir: "{app}\packaged_extensions"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; \
    Excludes: "com.hyperxtalk.macos*,com.livecode.library.native.mac.*,com.livecode.widget.native.mac.*,com.livecode.widget.native.ios.*,com.livecode.widget.native.emscripten.*"

; ---- IDE Toolset (home stack, libraries, palettes, resources) ----
; This is the entire development environment UI layer. Without it
; the engine's startUp handler cannot initialize the IDE.
Source: "..\ide\Toolset\*"; \
    DestDir: "{app}\Toolset"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

; ---- IDE Resources (examples, sample projects, Start Center) ----
Source: "..\ide\Resources\*"; \
    DestDir: "{app}\Resources"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

; ---- IDE Plugins (built-in palette plugins) ----
Source: "..\ide\Plugins\*"; \
    DestDir: "{app}\Plugins"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

; ---- IDE Documentation (dictionary, guides, HTML viewer) ----
Source: "..\ide\Documentation\*"; \
    DestDir: "{app}\Documentation"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

; ---- IDE support libraries (deploy, revliburl, etc.) ----
Source: "..\ide-support\*"; \
    DestDir: "{app}\ide-support"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

; In installed mode, home.livecodescript's revInternal__StackFiles only scans
; Toolset/palettes/** and Toolset/libraries/ — it never scans ide-support/.
; Without this, revsblibrary (and other ide-support stacks) are never registered
; with the engine, so revSBLibrary fails to load and the Standalone Application
; Settings window cannot open.  Copying the .livecodescript files into
; Toolset\libraries\ puts them on the path that revInternal__StackFiles does scan.
Source: "..\ide-support\*.livecodescript"; \
    DestDir: "{app}\Toolset\libraries"; \
    Flags: ignoreversion

; ---- Windows standalone runtime templates ----
; These are the manifest XML files used by the standalone builder
; to embed DPI awareness and UAC settings into compiled standalones.
Source: "..\ide\Runtime\Windows\*"; \
    DestDir: "{app}\Runtime\Windows"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

; ============================================================
[Dirs]
; Create writable user-data directories outside Program Files.
; HyperXTalk stores user stacks and preferences here.
Name: "{userdocs}\{#MyAppName}"
Name: "{userdocs}\{#MyAppName}\Stacks"

; ============================================================
[Icons]
; AppUserModelID must match SetCurrentProcessExplicitAppUserModelID in dskw32main.cpp
; so that Windows can deliver toast notifications and list the app in notification settings.
Name: "{group}\{#MyAppName}"; \
    Filename: "{app}\{#MyAppExeName}"; \
    AppUserModelID: "HyperXTalk.Engine"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; \
    Filename: "{app}\{#MyAppExeName}"; \
    AppUserModelID: "HyperXTalk.Engine"; \
    Tasks: desktopicon

; ============================================================
[Registry]
; ---- File association: .hyperxtalk ----
Root: HKA; Subkey: "Software\Classes\.hyperxtalk"; \
    ValueType: string; ValueName: ""; ValueData: "HyperXTalk.Stack"; \
    Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\HyperXTalk.Stack"; \
    ValueType: string; ValueName: ""; ValueData: "HyperXTalk Stack"; \
    Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\HyperXTalk.Stack\DefaultIcon"; \
    ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; \
    Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\HyperXTalk.Stack\shell\open\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; \
    Tasks: fileassoc

; ---- File association: .hyperxtalkscript ----
Root: HKA; Subkey: "Software\Classes\.hyperxtalkscript"; \
    ValueType: string; ValueName: ""; ValueData: "HyperXTalk.Script"; \
    Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\HyperXTalk.Script"; \
    ValueType: string; ValueName: ""; ValueData: "HyperXTalk Script"; \
    Flags: uninsdeletekey; Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\HyperXTalk.Script\DefaultIcon"; \
    ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; \
    Tasks: fileassoc
Root: HKA; Subkey: "Software\Classes\HyperXTalk.Script\shell\open\command"; \
    ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; \
    Tasks: fileassoc

; ============================================================
[Run]
Filename: "{app}\{#MyAppExeName}"; \
    Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; \
    Flags: nowait postinstall skipifsilent
