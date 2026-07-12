@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

REM ============================================================
REM  Rebuild libcurl 7.21.0 for x64 / VS 2019 (v142) static CRT
REM
REM  Run this from a "VS 2019 x64 Native Tools Command Prompt".
REM  (Start menu -> "x64 Native Tools Command Prompt for VS 2019")
REM
REM  Expected directory layout:
REM    <repo>\prebuilt\scripts\rebuild-curl-x64.bat   <- this file
REM    <repo>\prebuilt\unpacked\curl\...              <- destination
REM
REM  The curl 7.21.0 source tree must be accessible.  Edit
REM  CURL_SRC below if you placed it elsewhere.
REM ============================================================

REM -- Locate this script's directory (prebuilt\scripts) ----------
SET SCRIPTS_DIR=%~dp0
REM Strip trailing backslash
IF "%SCRIPTS_DIR:~-1%"=="\" SET SCRIPTS_DIR=%SCRIPTS_DIR:~0,-1%

REM Prebuilt root is one level up from scripts\
SET PREBUILT_DIR=%SCRIPTS_DIR%\..
FOR %%I IN ("%PREBUILT_DIR%") DO SET PREBUILT_DIR=%%~fI

REM -- Location of the curl source --------------------------------
REM Pass the path to curl-7.21.0 as the first argument, OR
REM edit the fallback path below.
IF "%~1"=="" (
    REM Default: look for curl-7.21.0 next to this script
    SET CURL_SRC=%SCRIPTS_DIR%\curl-7.21.0
) ELSE (
    SET CURL_SRC=%~1
)

REM -- Destination directories ------------------------------------
SET DEST_DEBUG=%PREBUILT_DIR%\unpacked\curl\x86_64-win32-v142_static_Debug
SET DEST_RELEASE=%PREBUILT_DIR%\unpacked\curl\x86_64-win32-v142_static_Release

REM ============================================================
ECHO.
ECHO == Rebuilding libcurl 7.21.0 for x64 (VS 2019 / v142 / static CRT)
ECHO    Source : %CURL_SRC%
ECHO    Debug  -> %DEST_DEBUG%\lib\libcurl_a.lib
ECHO    Release-> %DEST_RELEASE%\lib\libcurl_a.lib
ECHO.

IF NOT EXIST "%CURL_SRC%\lib\Makefile.vc9" (
    ECHO ERROR: Could not find %CURL_SRC%\lib\Makefile.vc9
    ECHO        Edit the CURL_SRC variable in this script to point at
    ECHO        the curl-7.21.0 source directory.
    EXIT /B 1
)

CD /D "%CURL_SRC%\lib"

REM ============================================================
ECHO == Building Debug (static lib, static CRT debug, x64, Windows SSPI)
ECHO.

nmake /F Makefile.vc9 CFG=debug MACHINE=X64 WINDOWS_SSPI=1 RTLIBCFG=static

IF %ERRORLEVEL% NEQ 0 (
    ECHO.
    ECHO ERROR: Debug build failed.
    EXIT /B %ERRORLEVEL%
)

IF NOT EXIST "%DEST_DEBUG%\lib" MKDIR "%DEST_DEBUG%\lib"
COPY /Y "debug\libcurld.lib" "%DEST_DEBUG%\lib\libcurl_a.lib"
IF %ERRORLEVEL% NEQ 0 (
    ECHO ERROR: Failed to copy debug lib to destination.
    EXIT /B %ERRORLEVEL%
)
ECHO    -> Copied debug\libcurld.lib to %DEST_DEBUG%\lib\libcurl_a.lib

REM ============================================================
ECHO.
ECHO == Building Release (static lib, static CRT, x64, Windows SSPI)
ECHO.

nmake /F Makefile.vc9 CFG=release MACHINE=X64 WINDOWS_SSPI=1 RTLIBCFG=static

IF %ERRORLEVEL% NEQ 0 (
    ECHO.
    ECHO ERROR: Release build failed.
    EXIT /B %ERRORLEVEL%
)

IF NOT EXIST "%DEST_RELEASE%\lib" MKDIR "%DEST_RELEASE%\lib"
COPY /Y "release\libcurl.lib" "%DEST_RELEASE%\lib\libcurl_a.lib"
IF %ERRORLEVEL% NEQ 0 (
    ECHO ERROR: Failed to copy release lib to destination.
    EXIT /B %ERRORLEVEL%
)
ECHO    -> Copied release\libcurl.lib to %DEST_RELEASE%\lib\libcurl_a.lib

REM ============================================================
ECHO.
ECHO == Done.  Re-run build-engine-x64.bat to build server-community.exe
ECHO.
