@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0"

:: ============================================================
:: compile-printer.bat  [Debug|Release]
::
:: Compiles three printer source files and replaces their members in
:: kernel.lib for the given configuration (default: Debug).
::
::   engine\src\printer.cpp       — MCPrinter base class (printer_base.obj)
::   engine\src\w32printer.cpp    — Windows MCWindowsPrinter subclass (printer.obj)
::   engine\src\customprinter.cpp — MCCustomPrinter implementation (customprinter.obj)
::
:: Why three files?
::   The linker needs both the base MCPrinter class (printer.cpp) AND the
::   Windows subclass (w32printer.cpp).  Previously only the subclass was
::   compiled here, leaving 44 MCPrinter:: unresolved externals at link time.
::   printer.cpp is compiled to printer_base.obj to avoid a name collision
::   with the existing printer.obj slot used for the Windows subclass.
::
::   customprinter.obj is also included because a previous run accidentally
::   removed it when findstr matched "customprinter.obj" for "printer.obj".
::
:: Root cause of the original winnt.h C2059 errors (now fixed):
::   sysdefs.h(811): #define None 0  collided with _IMAGE_POLICY_ENTRY.None
::   in winnt.h(24176) of SDK 10.0.26100.0.  Fixed in w32prefix.h by
::   undefining None before #include <windows.h> and restoring it after.
:: ============================================================

:: Accept optional configuration parameter — default to Debug.
set "CONFIG=%~1"
if /i "!CONFIG!" == "Release" (
    set "CONFIG=Release"
) else (
    set "CONFIG=Debug"
)

set "SRC_BASE=%~dp0engine\src\printer.cpp"
set "SRC_PRINTER=%~dp0engine\src\w32printer.cpp"
set "SRC_CUSTOM=%~dp0engine\src\customprinter.cpp"
set "KERNEL_LIB=%~dp0build-win-x86_64\livecode\!CONFIG!\lib\kernel.lib"
set "IBASE=%~dp0build-win-x86_64\livecode\engine\!CONFIG!"
set "OBJDIR=%IBASE%\x64\obj\kernel"
set "OBJ_BASE=%OBJDIR%\printer_base.obj"
set "OBJ_PRINTER=%OBJDIR%\printer.obj"
set "OBJ_CUSTOM=%OBJDIR%\customprinter.obj"
set "LOGFILE=%~dp0compile-printer-!CONFIG!.log"

echo compile-printer.bat [%CONFIG%] started: %DATE% %TIME% > "%LOGFILE%"
echo [Config: %CONFIG%]
echo.

:: Ensure the OBJ output directory exists (MSBuild creates it during a
:: normal kernel build, but compile-printer.bat may run on a clean tree
:: where kernel.vcxproj has not yet written any intermediate files).
if not exist "%OBJDIR%" (
    mkdir "%OBJDIR%"
    if errorlevel 1 ( echo ERROR: Cannot create OBJ dir: %OBJDIR% & exit /b 1 )
    echo Created OBJ dir: %OBJDIR%
)

:: ============================================================
:: Validate inputs
:: ============================================================
if not exist "%SRC_BASE%"    ( echo ERROR: Not found: %SRC_BASE%    & exit /b 1 )
if not exist "%SRC_PRINTER%" ( echo ERROR: Not found: %SRC_PRINTER% & exit /b 1 )
if not exist "%SRC_CUSTOM%"  ( echo ERROR: Not found: %SRC_CUSTOM%  & exit /b 1 )
if not exist "%KERNEL_LIB%"  ( echo ERROR: Not found: %KERNEL_LIB%  & exit /b 1 )

:: ============================================================
:: Step 1: Locate vcvars64.bat via vswhere
:: ============================================================
echo Step 1: Locating VS build tools ...

set "FIND_PS1=%TEMP%\hxt_find_vcvars.ps1"
echo $pf = [System.Environment]::GetEnvironmentVariable('ProgramFiles(x86)')> "%FIND_PS1%"
echo $vs = "$pf\Microsoft Visual Studio\Installer\vswhere.exe">> "%FIND_PS1%"
echo if (Test-Path $vs) { (^& $vs -latest -products * -requires Microsoft.VisualCpp.Tools.HostX64.TargetX64 -property installationPath) }>> "%FIND_PS1%"
for /f "tokens=*" %%i in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%FIND_PS1%"') do set "VS_INSTALL=%%i"
del "%FIND_PS1%" 2>nul

if defined VS_INSTALL (
    set "VCVARS=!VS_INSTALL!\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS goto :try_fallback
if not exist "%VCVARS%" goto :try_fallback
goto :found_vcvars

:try_fallback
for %%E in (BuildTools Community Professional Enterprise) do (
    for %%Y in (2022 2019) do (
        set "_try=C:\Program Files (x86)\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!_try!" ( set "VCVARS=!_try!" & goto :found_vcvars )
        set "_try=C:\Program Files\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
        if exist "!_try!" ( set "VCVARS=!_try!" & goto :found_vcvars )
    )
)
echo ERROR: Could not find vcvars64.bat.
exit /b 1

:found_vcvars
echo Using: %VCVARS%
echo Using VCVARS: %VCVARS% >> "%LOGFILE%"

:: ============================================================
:: Step 2: Set up x64 build environment
:: ============================================================
echo Step 2: Setting up x64 build environment ...
call "%VCVARS%" >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo ERROR: vcvars64.bat failed. & exit /b 1 )

:: ============================================================
:: Detect versioned prebuilt directories
::
:: ICU: the repo ships v145 and v142; the CI workflow aliases v145 → v143
:: so MSBuild (v143 toolset) can resolve them.  Search v143 first, then
:: v145, then v142 as a last resort.
::
:: OpenSSL: only v142 is shipped; use it directly.
:: ============================================================
set "ICU_INCLUDE="
for %%T in (v143 v145 v142) do (
    if not defined ICU_INCLUDE (
        if exist "%~dp0prebuilt\unpacked\icu\x86_64-win32-%%T_static_!CONFIG!\include" (
            set "ICU_INCLUDE=%~dp0prebuilt\unpacked\icu\x86_64-win32-%%T_static_!CONFIG!\include"
        )
    )
)
if not defined ICU_INCLUDE (
    echo WARNING: ICU include directory not found - compilation may fail
    set "ICU_INCLUDE=%~dp0prebuilt\unpacked\icu\x86_64-win32-v142_static_!CONFIG!\include"
)
echo ICU include: !ICU_INCLUDE!

set "OPENSSL_INCLUDE=%~dp0prebuilt\unpacked\openssl3\x86_64-win32-v142_static_!CONFIG!\include"
echo OpenSSL include: !OPENSSL_INCLUDE!

:: ============================================================
:: Step 3: Write cl.exe response file (shared flags for both files)
:: ============================================================
echo Step 3: Preparing compiler options ...

set "RSP=%TEMP%\hxt_printer_cl.rsp"

:: Flags matching kernel.vcxproj ClCompile for the chosen configuration.
:: No /std:c++ flag — matches kernel.vcxproj default (C++14).
:: The winnt.h C2059 'constant' issue is fixed in w32prefix.h, not here.
if /i "!CONFIG!" == "Release" (
    echo /c /nologo /MT /O2 /Oi /Gy /GS- /EHs-c- /wd4577 /wd4800 /wd4244 > "%RSP%"
) else (
    echo /c /nologo /MTd /ZI /RTC1 /GS- /EHs-c- /wd4577 /wd4800 /wd4244 > "%RSP%"
)

:: Preprocessor definitions
if /i "!CONFIG!" == "Release" (
    echo /D_RELEASE >> "%RSP%"
    echo /DNDEBUG >> "%RSP%"
) else (
    echo /D_DEBUG >> "%RSP%"
)
echo /D_CRT_NONSTDC_NO_DEPRECATE >> "%RSP%"
echo /D_CRT_SECURE_NO_DEPRECATE >> "%RSP%"
echo /D_CRT_DISABLE_PERFCRIT_LOCKS >> "%RSP%"
echo /D__LITTLE_ENDIAN__ >> "%RSP%"
echo /DWINVER=0x0601 >> "%RSP%"
echo /D_WIN32_WINNT=0x0601 >> "%RSP%"
echo /DU_STATIC_IMPLEMENTATION=1 >> "%RSP%"
echo /DOPENSSL_API_COMPAT=0x10100000L >> "%RSP%"
echo /DPCRE_STATIC=1 >> "%RSP%"
echo /DCROSS_COMPILE_TARGET >> "%RSP%"
echo /D_WINDOWS >> "%RSP%"
echo /DWIN32 >> "%RSP%"
echo /DTARGET_PLATFORM_WINDOWS >> "%RSP%"

:: Include paths
echo /I"%IBASE%\x64\obj\kernel\src" >> "%RSP%"
echo /I"%IBASE%\x64\obj\kernel\include" >> "%RSP%"
echo /I"%IBASE%\x64\obj\global_intermediate\src" >> "%RSP%"
echo /I"%IBASE%\x64\obj\global_intermediate\include" >> "%RSP%"
echo /I"%~dp0engine\include" >> "%RSP%"
echo /I"%~dp0engine\src" >> "%RSP%"
echo /I"!ICU_INCLUDE!" >> "%RSP%"
echo /I"%~dp0prebuilt\include" >> "%RSP%"
echo /I"%~dp0libfoundation\include" >> "%RSP%"
echo /I"%~dp0libgraphics\include" >> "%RSP%"
echo /I"%~dp0libscript\include" >> "%RSP%"
echo /I"%~dp0libbrowser\include" >> "%RSP%"
echo /I"!OPENSSL_INCLUDE!" >> "%RSP%"
echo /I"%~dp0prebuilt\include\openssl" >> "%RSP%"
echo /I"%~dp0thirdparty\libpcre\include" >> "%RSP%"
echo /I"%~dp0thirdparty\libjpeg\include" >> "%RSP%"
echo /I"%~dp0thirdparty\libgif\include" >> "%RSP%"
echo /I"%~dp0thirdparty\libpng\include" >> "%RSP%"
echo /I"%~dp0thirdparty\libz\include" >> "%RSP%"
echo /I"%IBASE%\x64\obj\shared_intermediate\include" >> "%RSP%"

:: ============================================================
:: Step 4: Compile printer.cpp (MCPrinter base class)
:: ============================================================
echo Step 4: Compiling printer.cpp (MCPrinter base class) ...
set "CL_LOG=%TEMP%\hxt_cl_out.txt"
cl.exe @"%RSP%" /Fo"%OBJ_BASE%" "%SRC_BASE%" > "%CL_LOG%" 2>&1
set CL_ERR=%ERRORLEVEL%
type "%CL_LOG%"
type "%CL_LOG%" >> "%LOGFILE%"
del "%CL_LOG%" 2>nul
if %CL_ERR% NEQ 0 (
    echo.
    echo *** printer.cpp COMPILE FAILED *** See %LOGFILE%
    del "%RSP%" 2>nul
    exit /b 1
)
if not exist "%OBJ_BASE%" ( echo ERROR: Output not found: %OBJ_BASE% & del "%RSP%" 2>nul & exit /b 1 )
echo Compiled OK: %OBJ_BASE%
echo.

:: ============================================================
:: Step 5: Compile w32printer.cpp (MCWindowsPrinter subclass)
:: ============================================================
echo Step 5: Compiling w32printer.cpp (MCWindowsPrinter subclass) ...
cl.exe @"%RSP%" /Fo"%OBJ_PRINTER%" "%SRC_PRINTER%" > "%CL_LOG%" 2>&1
set CL_ERR=%ERRORLEVEL%
type "%CL_LOG%"
type "%CL_LOG%" >> "%LOGFILE%"
del "%CL_LOG%" 2>nul
if %CL_ERR% NEQ 0 (
    echo.
    echo *** w32printer.cpp COMPILE FAILED *** See %LOGFILE%
    del "%RSP%" 2>nul
    exit /b 1
)
if not exist "%OBJ_PRINTER%" ( echo ERROR: Output not found: %OBJ_PRINTER% & del "%RSP%" 2>nul & exit /b 1 )
echo Compiled OK: %OBJ_PRINTER%
echo.

:: ============================================================
:: Step 6: Compile customprinter.cpp
:: ============================================================
echo Step 6: Compiling customprinter.cpp ...
set "CL_LOG=%TEMP%\hxt_cl_out.txt"
cl.exe @"%RSP%" /Fo"%OBJ_CUSTOM%" "%SRC_CUSTOM%" > "%CL_LOG%" 2>&1
set CL_ERR=%ERRORLEVEL%
type "%CL_LOG%"
type "%CL_LOG%" >> "%LOGFILE%"
del "%CL_LOG%" 2>nul
del "%RSP%" 2>nul
if %CL_ERR% NEQ 0 (
    echo.
    echo *** customprinter.cpp COMPILE FAILED *** See %LOGFILE%
    exit /b 1
)
if not exist "%OBJ_CUSTOM%" ( echo ERROR: Output not found: %OBJ_CUSTOM% & exit /b 1 )
echo Compiled OK: %OBJ_CUSTOM%
echo.

:: ============================================================
:: Step 7: Find member names in kernel.lib
::
:: We look for three members:
::   printer_base.obj  — MCPrinter base class (may not exist yet)
::   printer.obj       — w32printer Windows subclass (exclude customprinter)
::   customprinter.obj — MCCustomPrinter
::
:: IMPORTANT: search for printer.obj but EXCLUDE customprinter.obj and
:: printer_base.obj — findstr substring match would otherwise confuse them.
:: ============================================================
echo Step 7: Locating members in kernel.lib ...

lib.exe /NOLOGO /LIST "%KERNEL_LIB%" > "%TEMP%\hxt_lib_list.txt" 2>> "%LOGFILE%"

:: printer_base.obj member
findstr /i "printer_base\.obj" "%TEMP%\hxt_lib_list.txt" > "%TEMP%\hxt_base_member.txt"

:: printer.obj member: must contain "printer.obj" but NOT "customprinter" or "printer_base"
findstr /i "printer\.obj"       "%TEMP%\hxt_lib_list.txt" > "%TEMP%\hxt_all_printer.txt"
findstr /iv "customprinter"     "%TEMP%\hxt_all_printer.txt" > "%TEMP%\hxt_all_printer2.txt"
findstr /iv "printer_base"      "%TEMP%\hxt_all_printer2.txt" > "%TEMP%\hxt_printer_member.txt"
del "%TEMP%\hxt_all_printer.txt"  2>nul
del "%TEMP%\hxt_all_printer2.txt" 2>nul

:: customprinter.obj member
findstr /i "customprinter\.obj" "%TEMP%\hxt_lib_list.txt" > "%TEMP%\hxt_custom_member.txt"

del "%TEMP%\hxt_lib_list.txt" 2>nul

set "MEMBER_BASE="
for /f "tokens=*" %%L in (%TEMP%\hxt_base_member.txt) do (
    if not defined MEMBER_BASE set "MEMBER_BASE=%%L"
)
set "MEMBER_PRINTER="
for /f "tokens=*" %%L in (%TEMP%\hxt_printer_member.txt) do (
    if not defined MEMBER_PRINTER set "MEMBER_PRINTER=%%L"
)
set "MEMBER_CUSTOM="
for /f "tokens=*" %%L in (%TEMP%\hxt_custom_member.txt) do (
    if not defined MEMBER_CUSTOM set "MEMBER_CUSTOM=%%L"
)
del "%TEMP%\hxt_base_member.txt"    2>nul
del "%TEMP%\hxt_printer_member.txt" 2>nul
del "%TEMP%\hxt_custom_member.txt"  2>nul

if defined MEMBER_BASE (
    echo   printer_base member:  [!MEMBER_BASE!]
    echo   printer_base member:  [!MEMBER_BASE!] >> "%LOGFILE%"
) else (
    echo   printer_base.obj not found in lib - will add as new member
)
if defined MEMBER_PRINTER (
    echo   printer member:       [!MEMBER_PRINTER!]
    echo   printer member:       [!MEMBER_PRINTER!] >> "%LOGFILE%"
) else (
    echo   printer.obj not found in lib - will add as new member
)
if defined MEMBER_CUSTOM (
    echo   customprinter member: [!MEMBER_CUSTOM!]
    echo   customprinter member: [!MEMBER_CUSTOM!] >> "%LOGFILE%"
) else (
    echo   customprinter.obj not found in lib - will add as new member
)
echo.

:: ============================================================
:: Step 8: Build REMOVE arguments dynamically
:: ============================================================
set "REMOVE_ARGS="
if defined MEMBER_BASE     set "REMOVE_ARGS=!REMOVE_ARGS! /REMOVE:"!MEMBER_BASE!""
if defined MEMBER_PRINTER  set "REMOVE_ARGS=!REMOVE_ARGS! /REMOVE:"!MEMBER_PRINTER!""
if defined MEMBER_CUSTOM   set "REMOVE_ARGS=!REMOVE_ARGS! /REMOVE:"!MEMBER_CUSTOM!""

:: ============================================================
:: Step 9: Patch kernel.lib
::   Remove old members (if present), then add freshly compiled ones.
:: ============================================================
echo Step 9: Patching kernel.lib ...

set "LIB_TEMP=%KERNEL_LIB%.patch_tmp"

if defined REMOVE_ARGS (
    :: Remove old members → temp lib
    lib.exe /NOLOGO /OUT:"%LIB_TEMP%" "%KERNEL_LIB%" !REMOVE_ARGS! >> "%LOGFILE%" 2>&1
    if errorlevel 1 (
        echo ERROR: lib.exe /REMOVE failed. See %LOGFILE%
        if exist "%LIB_TEMP%" del "%LIB_TEMP%" 2>nul
        exit /b 1
    )
) else (
    :: Nothing to remove — start from existing lib
    copy /Y "%KERNEL_LIB%" "%LIB_TEMP%" > nul
)

:: Add new objs → final kernel.lib
lib.exe /NOLOGO /OUT:"%KERNEL_LIB%" "%LIB_TEMP%" "%OBJ_BASE%" "%OBJ_PRINTER%" "%OBJ_CUSTOM%" >> "%LOGFILE%" 2>&1
if errorlevel 1 (
    echo ERROR: lib.exe /ADD failed. See %LOGFILE%
    del "%LIB_TEMP%" 2>nul
    exit /b 1
)
del "%LIB_TEMP%" 2>nul

:: ============================================================
:: Done
:: ============================================================
echo.
echo ============================================================
echo  SUCCESS: kernel.lib updated
echo    - printer_base.obj: MCPrinter base class (fixes 44 unresolved externals)
echo    - printer.obj:      w32printer Windows subclass (lazy-init fix)
echo    - customprinter.obj: freshly compiled
echo ============================================================
echo.
if /i "!CONFIG!" == "Release" (
    echo Now run build-release-x64.bat to link HyperXTalk.exe.
) else (
    echo Now run build-engine-x64.bat to link HyperXTalk.exe.
)
echo.

echo SUCCESS >> "%LOGFILE%"
exit /b 0
