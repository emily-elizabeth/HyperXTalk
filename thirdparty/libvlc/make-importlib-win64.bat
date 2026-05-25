@echo off
:: make-importlib-win64.bat
:: Generates thirdparty\libvlc\lib\win64\libvlc.lib from the installed VLC DLL.
::
:: Usage:
::   make-importlib-win64.bat [VLCInstallDir]
::
:: If VLCInstallDir is not supplied it defaults to C:\Program Files\VideoLAN\VLC.
::
:: Must be run from a Visual Studio 2019 x64 Native Tools Command Prompt, OR
:: called from build-release-x64.bat (which sets up VCTOOLS automatically).

setlocal EnableDelayedExpansion

:: ---- 1. Resolve VLC install location ----
if "%~1" NEQ "" (
    set "VLC_DIR=%~1"
) else if "%VLCInstallDir%" NEQ "" (
    set "VLC_DIR=%VLCInstallDir%"
) else (
    set "VLC_DIR=C:\Program Files\VideoLAN\VLC"
)

if not exist "%VLC_DIR%\libvlc.dll" (
    echo ERROR: libvlc.dll not found at "%VLC_DIR%"
    echo        Install VLC 3.x for Windows ^(64-bit^) first.
    exit /b 1
)

:: ---- 2. Locate lib.exe from VS2019 ----
set "LIB_EXE="

:: Try VCINSTALLDIR (set when running inside a VS Developer Command Prompt)
if defined VCINSTALLDIR (
    set "LIB_EXE=!VCINSTALLDIR!bin\HostX64\x64\lib.exe"
    if not exist "!LIB_EXE!" set "LIB_EXE=!VCINSTALLDIR!Tools\MSVC\*\bin\Hostx64\x64\lib.exe"
)

:: Fall back: search common VS2019 install paths
if not defined LIB_EXE (
    for /f "delims=" %%P in ('dir /b /s "C:\Program Files (x86)\Microsoft Visual Studio\2019\*\VC\Tools\MSVC\*\bin\Hostx64\x64\lib.exe" 2^>nul') do (
        set "LIB_EXE=%%P"
        goto found_lib
    )
    for /f "delims=" %%P in ('dir /b /s "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\lib.exe" 2^>nul') do (
        set "LIB_EXE=%%P"
        goto found_lib
    )
    echo ERROR: lib.exe not found. Run from a VS 2019/2022 x64 Native Tools prompt.
    exit /b 1
)
:found_lib
echo Using lib.exe: %LIB_EXE%

:: ---- 3. Create output directory ----
set "OUT_DIR=%~dp0lib\win64"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

:: ---- 4. Generate libvlc.lib from the .def ----
set "DEF_FILE=%~dp0libvlc.def"
set "OUT_LIB=%OUT_DIR%\libvlc.lib"

echo Generating %OUT_LIB% ...
"%LIB_EXE%" /def:"%DEF_FILE%" /out:"%OUT_LIB%" /machine:x64 /nologo
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: lib.exe failed ^(exit code %ERRORLEVEL%^).
    exit /b 1
)
echo Done: %OUT_LIB%
endlocal
