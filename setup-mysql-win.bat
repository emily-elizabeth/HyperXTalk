@echo off
setlocal

:: ============================================================
:: setup-mysql-win.bat
::
:: Copies MySQL 9.6.0 development files (libmysql.lib,
:: libmysql.dll) from a Scoop-installed MySQL into the
:: HyperXTalk prebuilt directories so the engine links
:: against the real 9.6.0 connector instead of the old
:: bundled C-source stub.
::
:: Prerequisites:
::   scoop install mysql
::
:: Run once after installing Scoop MySQL, and again whenever
:: you update MySQL via Scoop (scoop update mysql).
:: ============================================================

cd /d "%~dp0"

:: ----------------------------------------------------------
:: 1.  Locate Scoop MySQL
:: ----------------------------------------------------------
set "SCOOP_DIR=%USERPROFILE%\scoop"
if defined SCOOP set "SCOOP_DIR=%SCOOP%"

set "MYSQL_DIR=%SCOOP_DIR%\apps\mysql\current"

if not exist "%MYSQL_DIR%\" (
    echo ERROR: MySQL not found at %MYSQL_DIR%
    echo.
    echo Please install MySQL via Scoop first:
    echo   scoop install mysql
    exit /b 1
)

echo MySQL root : %MYSQL_DIR%

:: ----------------------------------------------------------
:: 2.  Locate libmysql.lib  (import lib for libmysql.dll)
:: ----------------------------------------------------------
set "MYSQL_LIBDIR=%MYSQL_DIR%\lib"
set "MYSQL_LIB="
if exist "%MYSQL_LIBDIR%\libmysql.lib" set "MYSQL_LIB=%MYSQL_LIBDIR%\libmysql.lib"
if not defined MYSQL_LIB if exist "%MYSQL_LIBDIR%\vs14\libmysql.lib" (
    set "MYSQL_LIBDIR=%MYSQL_LIBDIR%\vs14"
    set "MYSQL_LIB=%MYSQL_LIBDIR%\vs14\libmysql.lib"
)

if not defined MYSQL_LIB (
    echo ERROR: Could not find libmysql.lib under %MYSQL_DIR%\lib\
    exit /b 1
)
echo libmysql.lib: %MYSQL_LIB%

:: ----------------------------------------------------------
:: 3.  Locate libmysql.dll
:: ----------------------------------------------------------
set "MYSQL_DLL="
set "MYSQL_DLLDIR="
if exist "%MYSQL_DIR%\lib\libmysql.dll" (
    set "MYSQL_DLL=%MYSQL_DIR%\lib\libmysql.dll"
    set "MYSQL_DLLDIR=%MYSQL_DIR%\lib"
)
if not defined MYSQL_DLL if exist "%MYSQL_DIR%\bin\libmysql.dll" (
    set "MYSQL_DLL=%MYSQL_DIR%\bin\libmysql.dll"
    set "MYSQL_DLLDIR=%MYSQL_DIR%\bin"
)

if not defined MYSQL_DLL (
    echo WARNING: Could not find libmysql.dll.
    echo Searched: %MYSQL_DIR%\lib\  and  %MYSQL_DIR%\bin\
) else (
    echo libmysql.dll: %MYSQL_DLL%
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
:: 4.  Copy libmysql.lib -> prebuilt unpacked (Debug)
::     Used by dbmysql.vcxproj linker search path.
:: ----------------------------------------------------------
set "PREBUILT_LIB=prebuilt\unpacked\Thirdparty\x86_64-win32-v142_static_Debug\lib"
if not exist "%PREBUILT_LIB%\" (
    echo ERROR: Prebuilt lib directory not found: %PREBUILT_LIB%
    exit /b 1
)
echo Copying libmysql.lib to prebuilt dir...
call :copy_file "%MYSQL_LIBDIR%" "%PREBUILT_LIB%" "libmysql.lib"
if errorlevel 1 exit /b 1

:: ----------------------------------------------------------
:: 5.  Copy libmysql.lib -> Debug\lib\
::     Used by development.vcxproj (searched before prebuilt).
:: ----------------------------------------------------------
set "DEBUG_LIB=build-win-x86_64\hyperxtalk\Debug\lib"
if not exist "%DEBUG_LIB%\" mkdir "%DEBUG_LIB%"
echo Copying libmysql.lib to Debug\lib...
call :copy_file "%MYSQL_LIBDIR%" "%DEBUG_LIB%" "libmysql.lib"
if errorlevel 1 exit /b 1

:: ----------------------------------------------------------
:: 6.  Copy libmysql.dll -> Debug\  (runtime dependency)
::     Uses goto instead of if() block to avoid batch
::     early-expansion swallowing %MYSQL_DLLDIR%.
:: ----------------------------------------------------------
if not defined MYSQL_DLL goto :skip_dll

set "DEBUG_OUT=build-win-x86_64\hyperxtalk\Debug"
if not exist "%DEBUG_OUT%\" mkdir "%DEBUG_OUT%"
echo Copying libmysql.dll to Debug...
call :copy_file "%MYSQL_DLLDIR%" "%DEBUG_OUT%" "libmysql.dll"
if errorlevel 1 exit /b 1

:skip_dll

:: ----------------------------------------------------------
:: 6c. Copy OpenSSL DLLs that libmysql.dll depends on at runtime.
::     libmysql.dll links against libssl-3-x64.dll and
::     libcrypto-3-x64.dll; Windows needs them in the same
::     directory as libmysql.dll (or on PATH) or LoadLibrary fails.
:: ----------------------------------------------------------
set "DEBUG_OUT=build-win-x86_64\hyperxtalk\Debug"
if not exist "%DEBUG_OUT%\" mkdir "%DEBUG_OUT%"

set "OPENSSL_SEARCH=%MYSQL_DIR%\bin"
if not exist "%OPENSSL_SEARCH%\" set "OPENSSL_SEARCH=%MYSQL_DIR%\lib"

if exist "%OPENSSL_SEARCH%\libssl-3-x64.dll" (
    echo Copying libssl-3-x64.dll to Debug...
    call :copy_file "%OPENSSL_SEARCH%" "%DEBUG_OUT%" "libssl-3-x64.dll"
    if errorlevel 1 exit /b 1
) else (
    echo WARNING: libssl-3-x64.dll not found under %MYSQL_DIR% - MySQL TLS may fail at runtime.
)

if exist "%OPENSSL_SEARCH%\libcrypto-3-x64.dll" (
    echo Copying libcrypto-3-x64.dll to Debug...
    call :copy_file "%OPENSSL_SEARCH%" "%DEBUG_OUT%" "libcrypto-3-x64.dll"
    if errorlevel 1 exit /b 1
) else (
    echo WARNING: libcrypto-3-x64.dll not found under %MYSQL_DIR% - MySQL TLS may fail at runtime.
)

:: ----------------------------------------------------------
:: 6b. Delete HyperXTalk.exe so the linker is forced to rerun
::     on the next build even if no source files changed.
::     MSBuild's incremental build skips the link step when
::     only a .lib changed; removing the output breaks that.
:: ----------------------------------------------------------
set "EXE_OUT=build-win-x86_64\hyperxtalk\Debug\HyperXTalk.exe"
if exist "%EXE_OUT%" (
    attrib -R "%EXE_OUT%" 2>nul
    del /F /Q "%EXE_OUT%"
    echo Deleted %EXE_OUT% (forces relink on next build^)
)

:: ----------------------------------------------------------
:: 7.  Sync headers from Scoop into thirdparty\libmysql\include\
::     Only copies if Scoop has an include directory.
:: ----------------------------------------------------------
set "MYSQL_INC=%MYSQL_DIR%\include"
if not exist "%MYSQL_INC%\mysql.h" goto :skip_headers
echo Syncing headers from %MYSQL_INC% ...
xcopy /E /I /Y /Q "%MYSQL_INC%\*" "thirdparty\libmysql\include\"
echo Headers synced.
goto :headers_done
:skip_headers
echo NOTE: No include\mysql.h found in Scoop MySQL; keeping existing headers.
:headers_done

:: ----------------------------------------------------------
:: Done
:: ----------------------------------------------------------
echo.
echo MySQL 9.6.0 setup complete.
echo Run build-engine-x64.bat to rebuild HyperXTalk.
endlocal
