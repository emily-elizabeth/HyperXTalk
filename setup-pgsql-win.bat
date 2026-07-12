@echo off
setlocal

:: ============================================================
:: setup-pgsql-win.bat
::
:: Copies PostgreSQL 18 development files (libpq.lib, libpq.dll)
:: from a Scoop-installed PostgreSQL into the HyperXTalk build
:: directories so the engine links against the prebuilt libpq
:: instead of the old bundled C-source stub.
::
:: Prerequisites:
::   scoop install postgresql
::
:: Run once after installing Scoop PostgreSQL, and again whenever
:: you update PostgreSQL via Scoop (scoop update postgresql).
:: ============================================================

cd /d "%~dp0"

:: ----------------------------------------------------------
:: 1.  Locate Scoop PostgreSQL
:: ----------------------------------------------------------
set "SCOOP_DIR=%USERPROFILE%\scoop"
if defined SCOOP set "SCOOP_DIR=%SCOOP%"

set "PG_DIR=%SCOOP_DIR%\apps\postgresql\current"

if not exist "%PG_DIR%\" (
    echo ERROR: PostgreSQL not found at %PG_DIR%
    echo.
    echo Please install PostgreSQL via Scoop first:
    echo   scoop install postgresql
    exit /b 1
)

echo PostgreSQL root : %PG_DIR%

:: ----------------------------------------------------------
:: 2.  Locate libpq.lib  (MSVC import library)
::     EDB-packaged PostgreSQL puts it in lib\
:: ----------------------------------------------------------
set "PG_LIBDIR=%PG_DIR%\lib"
set "PG_LIB="
if exist "%PG_LIBDIR%\libpq.lib" set "PG_LIB=%PG_LIBDIR%\libpq.lib"

if not defined PG_LIB (
    echo ERROR: Could not find libpq.lib under %PG_DIR%\lib\
    echo.
    echo Make sure you installed the official EDB PostgreSQL package via Scoop.
    exit /b 1
)
echo libpq.lib: %PG_LIB%

:: ----------------------------------------------------------
:: 3.  Locate libpq.dll  (runtime dependency)
:: ----------------------------------------------------------
set "PG_DLL="
set "PG_DLLDIR="
if exist "%PG_DIR%\bin\libpq.dll" (
    set "PG_DLL=%PG_DIR%\bin\libpq.dll"
    set "PG_DLLDIR=%PG_DIR%\bin"
)
if not defined PG_DLL if exist "%PG_DIR%\lib\libpq.dll" (
    set "PG_DLL=%PG_DIR%\lib\libpq.dll"
    set "PG_DLLDIR=%PG_DIR%\lib"
)

if not defined PG_DLL (
    echo WARNING: Could not find libpq.dll.
    echo Searched: %PG_DIR%\bin\  and  %PG_DIR%\lib\
) else (
    echo libpq.dll: %PG_DLL%
)

:: ----------------------------------------------------------
:: Helper: clear read-only, then robocopy a single file
::   %1 = full source directory
::   %2 = destination directory
::   %3 = filename
:: ----------------------------------------------------------
goto :copy_begin
:copy_file
    attrib -R "%~2\%~3" 2>nul
    robocopy "%~1" "%~2" "%~3" /IS /IT /NJH /NJS /NFL /NDL
    if errorlevel 8 (
        echo   FAILED: could not copy %~3 to %~2
        exit /b 1
    )
    echo   OK: %~2\%~3
    exit /b 0
:copy_begin

:: ----------------------------------------------------------
:: 4.  Copy libpq.lib -> Debug\lib\
::     This is where development.vcxproj searches for it.
:: ----------------------------------------------------------
set "DEBUG_LIB=build-win-x86_64\hyperxtalk\Debug\lib"
if not exist "%DEBUG_LIB%\" mkdir "%DEBUG_LIB%"
echo Copying libpq.lib to Debug\lib...
call :copy_file "%PG_LIBDIR%" "%DEBUG_LIB%" "libpq.lib"
if errorlevel 1 exit /b 1

:: ----------------------------------------------------------
:: 5.  Copy libpq.dll -> Debug\  (runtime dependency)
:: ----------------------------------------------------------
if not defined PG_DLL goto :skip_dll

set "DEBUG_OUT=build-win-x86_64\hyperxtalk\Debug"
if not exist "%DEBUG_OUT%\" mkdir "%DEBUG_OUT%"
echo Copying libpq.dll to Debug...
call :copy_file "%PG_DLLDIR%" "%DEBUG_OUT%" "libpq.dll"
if errorlevel 1 exit /b 1

:skip_dll

:: ----------------------------------------------------------
:: 6.  Delete HyperXTalk.exe so the linker is forced to rerun
::     on the next build even if no source files changed.
:: ----------------------------------------------------------
set "EXE_OUT=build-win-x86_64\hyperxtalk\Debug\HyperXTalk.exe"
if exist "%EXE_OUT%" (
    attrib -R "%EXE_OUT%" 2>nul
    del /F /Q "%EXE_OUT%"
    echo Deleted %EXE_OUT% (forces relink on next build^)
)

:: ----------------------------------------------------------
:: 7.  Sync headers from Scoop into thirdparty\libpq\include\
::     Replaces the old 9.4.5 headers with 18.x headers.
:: ----------------------------------------------------------
set "PG_INC=%PG_DIR%\include"
if not exist "%PG_INC%\libpq-fe.h" goto :skip_headers
echo Syncing headers from %PG_INC% ...
xcopy /E /I /Y /Q "%PG_INC%\*" "thirdparty\libpq\include\"
echo Headers synced.
goto :headers_done
:skip_headers
echo NOTE: No include\libpq-fe.h found in Scoop PostgreSQL; keeping existing headers.
:headers_done

:: ----------------------------------------------------------
:: 8.  Report the installed version
:: ----------------------------------------------------------
set "PG_VER_H=%PG_DIR%\include\pg_config.h"
if exist "%PG_VER_H%" (
    for /f "tokens=3" %%v in ('findstr /C:"#define PG_VERSION " "%PG_VER_H%"') do (
        echo PostgreSQL version: %%~v
    )
)

echo.
echo PostgreSQL setup complete.
echo   libpq.lib : %DEBUG_LIB%\libpq.lib
if defined PG_DLL echo   libpq.dll : %DEBUG_OUT%\libpq.dll
echo.
echo You can now run build-engine-x64.bat
