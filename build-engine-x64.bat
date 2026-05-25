@echo off
setlocal

cd /d "%~dp0"
set LOGFILE=%~dp0build-engine-x64.log
set VCXPROJ_ENGINE=build-win-x86_64\livecode\engine\development.vcxproj
set VCXPROJ_BROWSER=build-win-x86_64\livecode\libbrowser\libbrowser.vcxproj
set VCXPROJ_DBMYSQL=build-win-x86_64\livecode\revdb\dbmysql.vcxproj
set VCXPROJ_LCB_MODULES=build-win-x86_64\livecode\engine\engine_lcb_modules.vcxproj
set VCXPROJ_LIBFFI=build-win-x86_64\livecode\thirdparty\libffi\libffi.vcxproj
set VCXPROJ_LIBFOUNDATION=build-win-x86_64\livecode\libfoundation\libFoundation.vcxproj
set VCXPROJ_LIBSCRIPT=build-win-x86_64\livecode\libscript\libScript.vcxproj
set VCXPROJ_STANDALONE=build-win-x86_64\livecode\engine\standalone.vcxproj
set VCXPROJ_KERNEL=build-win-x86_64\livecode\engine\kernel.vcxproj
set VCXPROJ_KERNEL_STANDALONE=build-win-x86_64\livecode\engine\kernel-standalone.vcxproj
set VCXPROJ_KERNEL_DEVELOPMENT=build-win-x86_64\livecode\engine\kernel-development.vcxproj
set VCXPROJ_PERFECT=build-win-x86_64\livecode\util\perfect\perfect.vcxproj
set VCXPROJ_SECURITY_COMMUNITY=build-win-x86_64\livecode\engine\security-community.vcxproj
set VCXPROJ_LIBGIF=build-win-x86_64\livecode\thirdparty\libgif\libgif.vcxproj
set VCXPROJ_LIBJPEG=build-win-x86_64\livecode\thirdparty\libjpeg\libjpeg.vcxproj
set VCXPROJ_LIBPNG=build-win-x86_64\livecode\thirdparty\libpng\libpng.vcxproj
set VCXPROJ_LIBPCRE=build-win-x86_64\livecode\thirdparty\libpcre\libpcre.vcxproj
set VCXPROJ_LIBEXPAT=build-win-x86_64\livecode\thirdparty\libexpat\libexpat.vcxproj
set VCXPROJ_LIBSKIA=build-win-x86_64\livecode\thirdparty\libskia\libskia.vcxproj
set VCXPROJ_LIBSKIA_OPT_NONE=build-win-x86_64\livecode\thirdparty\libskia\libskia_opt_none.vcxproj
set VCXPROJ_LIBSKIA_OPT_SSE2=build-win-x86_64\livecode\thirdparty\libskia\libskia_opt_sse2.vcxproj
set VCXPROJ_LIBSKIA_OPT_SSE3=build-win-x86_64\livecode\thirdparty\libskia\libskia_opt_sse3.vcxproj
set VCXPROJ_LIBSKIA_OPT_SSE41=build-win-x86_64\livecode\thirdparty\libskia\libskia_opt_sse41.vcxproj
set VCXPROJ_LIBSKIA_OPT_SSE42=build-win-x86_64\livecode\thirdparty\libskia\libskia_opt_sse42.vcxproj
set VCXPROJ_LIBSKIA_OPT_AVX=build-win-x86_64\livecode\thirdparty\libskia\libskia_opt_avx.vcxproj
set VCXPROJ_LIBSKIA_OPT_HSW=build-win-x86_64\livecode\thirdparty\libskia\libskia_opt_hsw.vcxproj
set VCXPROJ_LIBSKIA_OPT_ARM=build-win-x86_64\livecode\thirdparty\libskia\libskia_opt_arm.vcxproj
set VCXPROJ_LIBGRAPHICS=build-win-x86_64\livecode\libgraphics\libGraphics.vcxproj
set VCXPROJ_STDSCRIPT=build-win-x86_64\livecode\libscript\stdscript.vcxproj
set VCXPROJ_OPENSSL_SYMLIST=build-win-x86_64\livecode\thirdparty\libopenssl\libopenssl_symbol_list_win.vcxproj
set VCXPROJ_REVSECURITY=build-win-x86_64\livecode\thirdparty\libopenssl\revsecurity.vcxproj
set VCXPROJ_LIBXML=build-win-x86_64\livecode\thirdparty\libxml\libxml.vcxproj
set VCXPROJ_LIBXSLT=build-win-x86_64\livecode\thirdparty\libxslt\libxslt.vcxproj
set VCXPROJ_REVXML=build-win-x86_64\livecode\revxml\external-revxml.vcxproj
set VCXPROJ_REVXML_SERVER=build-win-x86_64\livecode\revxml\external-revxml-server.vcxproj
set VCXPROJ_REVZIP_SERVER=build-win-x86_64\livecode\revzip\external-revzip-server.vcxproj
set VCXPROJ_REVBROWSER=build-win-x86_64\livecode\revbrowser\external-revbrowser.vcxproj

:: ----------------------------------------------------------
:: MySQL 9.6.0 prerequisite check
:: The engine links against libmysql.lib (MySQL 9.6.0).
:: Run setup-mysql-win.bat once after: scoop install mysql
:: ----------------------------------------------------------
set "DEBUG_MYSQL_LIB=build-win-x86_64\livecode\Debug\lib\libmysql.lib"
if not exist "%DEBUG_MYSQL_LIB%" (
    echo.
    echo NOTE: %DEBUG_MYSQL_LIB% not found.
    echo Running setup-mysql-win.bat to copy MySQL 9.6.0 libs from Scoop...
    echo.
    call setup-mysql-win.bat
    if errorlevel 1 (
        echo.
        echo MySQL setup failed. Install MySQL via Scoop first:
        echo   scoop install mysql
        echo Then re-run this script.
        exit /b 1
    )
    echo.
)

:: ----------------------------------------------------------
:: PostgreSQL 18 prerequisite check
:: The engine links against libpq.lib (PostgreSQL 18).
:: Run setup-pgsql-win.bat once after: scoop install postgresql
:: ----------------------------------------------------------
set "DEBUG_PG_LIB=build-win-x86_64\livecode\Debug\lib\libpq.lib"
if not exist "%DEBUG_PG_LIB%" (
    echo.
    echo NOTE: %DEBUG_PG_LIB% not found.
    echo Running setup-pgsql-win.bat to copy PostgreSQL 18 libs from Scoop...
    echo.
    call setup-pgsql-win.bat
    if errorlevel 1 (
        echo.
        echo PostgreSQL setup failed. Install PostgreSQL via Scoop first:
        echo   scoop install postgresql
        echo Then re-run this script.
        exit /b 1
    )
    echo.
)

:: ----------------------------------------------------------
:: Locate MSBuild via a temp PS1 file (avoids cmd mishandling of
:: parentheses inside %ProgramFiles(x86)% in inline commands)
:: ----------------------------------------------------------
set "FIND_PS1=%TEMP%\hxt_find_msbuild.ps1"
echo $pf = [System.Environment]::GetEnvironmentVariable('ProgramFiles(x86)')> "%FIND_PS1%"
echo $vs = "$pf\Microsoft Visual Studio\Installer\vswhere.exe">> "%FIND_PS1%"
echo if (Test-Path $vs) { ^& $vs -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' ^| Select-Object -First 1 }>> "%FIND_PS1%"
for /f "tokens=*" %%i in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%FIND_PS1%"') do set "MSBUILD=%%i"
del "%FIND_PS1%" 2>nul
if not defined MSBUILD (
    echo ERROR: MSBuild.exe not found. Install Visual Studio 2019 Build Tools with C++ workload.
    exit /b 1
)
echo Using MSBuild: %MSBUILD%

echo Build started: %DATE% %TIME%
echo Build started: %DATE% %TIME% > "%LOGFILE%"
echo. >> "%LOGFILE%"

:: ----------------------------------------------------------
:: Build libExternal.lib — required by dbmysql and other DB drivers.
:: On the developer's machine this is a leftover from a previous build;
:: on a clean CI checkout it must be compiled first.
:: ----------------------------------------------------------
echo Building libExternal ...
echo Building libExternal ... >> "%LOGFILE%"
set "VCXPROJ_LIBEXTERNAL=build-win-x86_64\livecode\libexternal\libExternal.vcxproj"
"%MSBUILD%" %VCXPROJ_LIBEXTERNAL%  "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 (
    echo.
    echo LIBEXTERNAL BUILD FAILED. Errors:
    findstr /i " error " "%LOGFILE%"
    echo Full log: %LOGFILE%
    exit /b 1
)
echo libExternal OK.

echo.
:: ----------------------------------------------------------
:: libbrowser — build the real WebView2 backend (Debug).
::
:: libbrowser.vcxproj now compiles libbrowser_webview2.cpp and
:: libbrowser_webview2_win.cpp against the bundled WebView2 SDK
:: headers in packages/Microsoft.Web.WebView2.1.0.3912.50/.
:: WebView2LoaderStatic.lib is linked via a #pragma comment in
:: libbrowser_webview2.cpp; no explicit lib dir is needed here
:: because the lib is bootstrapped into Debug\lib\ below.
:: ----------------------------------------------------------
echo Building libbrowser (WebView2, Debug) ...
echo Building libbrowser (WebView2) ... >> "%LOGFILE%"

:: Bootstrap WebView2LoaderStatic.lib into Debug\lib\ so the linker finds it.
set "WV2_LIB=%~dp0packages\Microsoft.Web.WebView2.1.0.3912.50\build\native\x64\WebView2LoaderStatic.lib"
if not exist "%~dp0build-win-x86_64\livecode\Debug\lib" mkdir "%~dp0build-win-x86_64\livecode\Debug\lib"
if exist "%WV2_LIB%" (
    copy /Y "%WV2_LIB%" "%~dp0build-win-x86_64\livecode\Debug\lib\WebView2LoaderStatic.lib" > nul
)

:: Find CL.exe and lib.exe for the revsecurity step below (still needed).
set "FIND_CLEXE_PS1=%TEMP%\hxt_find_clexe.ps1"
echo $p = '%MSBUILD%'                                                    > "%FIND_CLEXE_PS1%"
echo $p = Split-Path -Parent $p                                         >> "%FIND_CLEXE_PS1%"
echo $p = Split-Path -Parent $p                                         >> "%FIND_CLEXE_PS1%"
echo $p = Split-Path -Parent $p                                         >> "%FIND_CLEXE_PS1%"
echo $p = Split-Path -Parent $p                                         >> "%FIND_CLEXE_PS1%"
echo $vctools = Join-Path $p 'VC\Tools\MSVC'                            >> "%FIND_CLEXE_PS1%"
echo $cl = Get-ChildItem $vctools -Recurse -Filter cl.exe -ErrorAction SilentlyContinue >> "%FIND_CLEXE_PS1%"
echo $cl = $cl ^| Where-Object { $_.DirectoryName -like '*Hostx64\x64' } ^| Select-Object -First 1 >> "%FIND_CLEXE_PS1%"
echo if ($cl) { $cl.FullName }                                          >> "%FIND_CLEXE_PS1%"
for /f "tokens=*" %%i in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%FIND_CLEXE_PS1%"') do set "CL_EXE=%%i"
del "%FIND_CLEXE_PS1%" 2>nul
if not defined CL_EXE (
    echo ERROR: cl.exe not found. Install Visual Studio C++ tools.
    exit /b 1
)
set "LIB_EXE=%CL_EXE:cl.exe=lib.exe%"

:: Set up the VS environment (include/lib paths) by calling vcvarsall.bat x64.
set "FIND_VCVARS_PS1=%TEMP%\hxt_find_vcvars.ps1"
echo $p = '%MSBUILD%'                                    > "%FIND_VCVARS_PS1%"
echo $p = Split-Path -Parent $p                         >> "%FIND_VCVARS_PS1%"
echo $p = Split-Path -Parent $p                         >> "%FIND_VCVARS_PS1%"
echo $p = Split-Path -Parent $p                         >> "%FIND_VCVARS_PS1%"
echo $p = Split-Path -Parent $p                         >> "%FIND_VCVARS_PS1%"
echo Join-Path $p 'VC\Auxiliary\Build\vcvarsall.bat'    >> "%FIND_VCVARS_PS1%"
for /f "tokens=*" %%i in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%FIND_VCVARS_PS1%"') do set "VCVARSALL=%%i"
del "%FIND_VCVARS_PS1%" 2>nul
if exist "%VCVARSALL%" (
    call "%VCVARSALL%" x64 > nul 2>&1
)

set "LIBBROWSER_LOG=%~dp0build-libbrowser.log"
"%MSBUILD%" %VCXPROJ_BROWSER% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo > "%LIBBROWSER_LOG%" 2>&1
set LIBBROWSER_ERR=%ERRORLEVEL%
type "%LIBBROWSER_LOG%"
type "%LIBBROWSER_LOG%" >> "%LOGFILE%"
if %LIBBROWSER_ERR% NEQ 0 (
    echo ERROR: libbrowser WebView2 build failed. See %LIBBROWSER_LOG%
    exit /b 1
)
echo libbrowser OK.

echo.
echo Building libopenssl_stubs (Debug x64) ...
echo Building libopenssl_stubs ... >> "%LOGFILE%"
set "VCXPROJ_OPENSSL_STUBS=build-win-x86_64\livecode\thirdparty\libopenssl\libopenssl_stubs.vcxproj"
"%MSBUILD%" %VCXPROJ_OPENSSL_STUBS% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBOPENSSL_STUBS BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libopenssl_stubs OK.

echo.
echo Building libCore (Debug x64) ...
echo Building libCore ... >> "%LOGFILE%"
set "VCXPROJ_LIBCORE=build-win-x86_64\livecode\libcore\libCore.vcxproj"
"%MSBUILD%" %VCXPROJ_LIBCORE% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBCORE BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libCore OK.

echo.
echo Building dbmysql (MySQL 9.6.0 database driver) ...
echo Building dbmysql ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_DBMYSQL% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 (
    echo.
    echo DBMYSQL BUILD FAILED. Errors:
    findstr /i " error " "%LOGFILE%"
    echo Full log: %LOGFILE%
    exit /b 1
)
echo dbmysql OK.

echo.
:: ----------------------------------------------------------
:: Build libffi (x64 must use include_win64 so FFI_DEFAULT_ABI = FFI_WIN64 = 1,
:: matching what libffi.lib expects; using include_win32 gave FFI_MS_CDECL = 5
:: which caused ffi_prep_cif to return FFI_BAD_ABI and throw "unexpected libffi failure").
:: ----------------------------------------------------------
echo Building libffi ...
echo Building libffi ... >> "%LOGFILE%"
set "LIBFFI_LOG=%~dp0build-libffi.log"
"%MSBUILD%" %VCXPROJ_LIBFFI% /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\\livecode\\" /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo > "%LIBFFI_LOG%" 2>&1
set LIBFFI_ERR=%ERRORLEVEL%
type "%LIBFFI_LOG%"
type "%LIBFFI_LOG%" >> "%LOGFILE%"
if %LIBFFI_ERR% NEQ 0 (
    echo.
    echo LIBFFI BUILD FAILED. See %LIBFFI_LOG% for details.
    exit /b 1
)
echo libffi OK.
:: Copy freshly-built libffi.lib into the Thirdparty prebuilt dirs so that
:: toolchain projects using BuildProjectReferences=false can find it via
:: their AdditionalLibraryDirectories (they point to the prebuilt dir, not
:: the build output dir).
copy /y "build-win-x86_64\livecode\Debug\lib\libffi.lib" "prebuilt\unpacked\Thirdparty\x86_64-win32-v145_static_Debug\lib\libffi.lib"
copy /y "build-win-x86_64\livecode\Debug\lib\libffi.lib" "prebuilt\unpacked\Thirdparty\x86_64-win32-v143_static_Debug\lib\libffi.lib"
copy /y "build-win-x86_64\livecode\Debug\lib\libffi.lib" "prebuilt\unpacked\Thirdparty\x86_64-win32-v142_static_Debug\lib\libffi.lib"
echo libffi.lib copied to Thirdparty prebuilt dirs.


echo.
echo Building libz ...
echo Building libz ... >> "%LOGFILE%"
set "VCXPROJ_LIBZ=build-win-x86_64\livecode\thirdparty\libz\libz.vcxproj"
set "LIBZ_LOG=%~dp0build-libz.log"
"%MSBUILD%" "%VCXPROJ_LIBZ%" /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo > "%LIBZ_LOG%" 2>&1
set LIBZ_ERR=%ERRORLEVEL%
type "%LIBZ_LOG%"
type "%LIBZ_LOG%" >> "%LOGFILE%"
if %LIBZ_ERR% NEQ 0 (
    echo.
    echo LIBZ BUILD FAILED. See %LIBZ_LOG% for details.
    exit /b 1
)
echo libz OK.

echo.
:: /t:Rebuild is required here — MSBuild does not detect include-path changes
:: in incremental builds, so foundation-handler.obj and foundation-typeinfo.obj
:: would keep the stale FFI_DEFAULT_ABI=5 value (from include_win32) baked in.
:: Rebuild guarantees they are compiled with include_win64 → FFI_DEFAULT_ABI=1.
:: ----------------------------------------------------------
:: Generate icudata-minimal.cpp (shared_intermediate/src/icudata-minimal.cpp).
::
:: dependency chain:
::   minimal_icu_data.vcxproj
::     → runs icupkg (from prebuilt ICU) on icudt58l.dat to produce
::       shared_intermediate/data/icudata-minimal.dat
::   encode_minimal_icu_data.vcxproj
::     → runs util/encode_data.py on the .dat file to produce
::       shared_intermediate/src/icudata-minimal.cpp
::
:: libFoundation.vcxproj compiles icudata-minimal.cpp, so this must run
:: before the libFoundation build.
:: ----------------------------------------------------------
:: Step 0: pre-generate icudata-minimal.dat and icudata-minimal.cpp using a
:: Python fallback.  The MSBuild steps (minimal_icu_data / encode_minimal_icu_data)
:: run icupkg.exe from the prebuilt ICU bin dir, but that binary is not always
:: present (it is not included in the vcpkg ICU packages).  The Python script
:: copies icudt58l.dat as the minimal dat and encodes it as a C++ array in the
:: same format as encode_data.pl.
::
:: The output directory is %~d0\shared_intermediate — where %~d0 is the drive
:: letter of this script (the same drive as the repo checkout).  This matches
:: where MSBuild resolves $(obj)\..\shared_intermediate when $(obj) is the
:: empty string in standalone vcxproj builds: $(obj)\..\shared_intermediate
:: = \..\shared_intermediate = \shared_intermediate = <repo-drive>:\shared_intermediate.
set "SHARED_INT=%~d0\shared_intermediate"
echo Pre-generating icudata-minimal.cpp (Python fallback for icupkg) ...
echo Pre-generating icudata-minimal.cpp ... >> "%LOGFILE%"
set "ICU_DAT=%~dp0prebuilt\unpacked\icu\x86_64-win32-v145_static_Debug\share\icudt58l.dat"
python "%~dp0util\generate_icudata_cpp.py" "%ICU_DAT%" "%SHARED_INT%" >> "%LOGFILE%" 2>&1
if errorlevel 1 (
    echo WARNING: Python fallback failed; will try MSBuild icupkg path.
) else (
    echo icudata-minimal.cpp pre-generated OK.
)

:: Step 1: minimal_icu_data — runs icupkg on the bundled ICU dat file to
:: produce icudata-minimal.dat.  Built with BuildProjectReferences=false to
:: avoid chaining into fetch.vcxproj → fetch-win.vcxproj, which would abort
:: because prebuilt/lib/win32/icudt.lib is not in the repo.  The ICU
:: binaries (icupkg.exe) are already present in prebuilt/unpacked/icu/.
echo Generating icudata-minimal.dat (minimal_icu_data) ...
echo Generating icudata-minimal.dat ... >> "%LOGFILE%"
set "VCXPROJ_MINICU=build-win-x86_64\livecode\prebuilt\minimal_icu_data.vcxproj"
set "MINICU_LOG=%~dp0build-minimal-icu.log"
"%MSBUILD%" %VCXPROJ_MINICU%  "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%MINICU_LOG%" 2>&1
set MINICU_ERR=%ERRORLEVEL%
type "%MINICU_LOG%"
type "%MINICU_LOG%" >> "%LOGFILE%"
if %MINICU_ERR% NEQ 0 (
    echo.
    echo MINIMAL_ICU_DATA FAILED — icupkg not found in prebuilt ICU bin dir.
    echo Trying Python-only fallback - icudata-minimal.cpp already pre-generated.
    echo If %SHARED_INT%\src\icudata-minimal.cpp exists, continuing...
    if not exist "%SHARED_INT%\src\icudata-minimal.cpp" (
        echo ERROR: icudata-minimal.cpp not found. See %MINICU_LOG%
        exit /b 1
    )
    echo Fallback OK: using pre-generated icudata-minimal.cpp.
)
echo minimal_icu_data OK.

echo.
:: Step 2: encode_minimal_icu_data — runs util/encode_data.py on the .dat
:: file to produce icudata-minimal.cpp.
echo Generating icudata-minimal.cpp (encode_minimal_icu_data) ...
echo Generating icudata-minimal.cpp ... >> "%LOGFILE%"
set "VCXPROJ_ICU=build-win-x86_64\livecode\prebuilt\encode_minimal_icu_data.vcxproj"
set "ICU_LOG=%~dp0build-encode-icu.log"
"%MSBUILD%" %VCXPROJ_ICU%  "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%ICU_LOG%" 2>&1
set ICU_ERR=%ERRORLEVEL%
type "%ICU_LOG%"
type "%ICU_LOG%" >> "%LOGFILE%"
if %ICU_ERR% NEQ 0 (
    echo.
    echo encode_minimal_icu_data failed - checking for pre-generated fallback ...
    if exist "%SHARED_INT%\src\icudata-minimal.cpp" (
        echo Fallback OK: using pre-generated icudata-minimal.cpp from Python script.
    ) else (
        echo ERROR: icudata-minimal.cpp not found. See %ICU_LOG% for details.
        exit /b 1
    )
)
echo encode_minimal_icu_data OK.

echo.
echo Building libFoundation (FFI closure fix: x64 now uses include_win64 headers) ...
echo Building libFoundation ... >> "%LOGFILE%"
set "FOUND_LOG=%~dp0build-libfoundation.log"
"%MSBUILD%" %VCXPROJ_LIBFOUNDATION% /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\\livecode\\" /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo > "%FOUND_LOG%" 2>&1
set FOUND_ERR=%ERRORLEVEL%
type "%FOUND_LOG%"
type "%FOUND_LOG%" >> "%LOGFILE%"
if %FOUND_ERR% NEQ 0 (
    echo.
    echo LIBFOUNDATION BUILD FAILED. See %FOUND_LOG% for details.
    exit /b 1
)
echo libFoundation OK.

echo.
echo Building libScript ...
echo Building libScript ... >> "%LOGFILE%"
set "SCRIPT_LOG=%~dp0build-libscript.log"
"%MSBUILD%" %VCXPROJ_LIBSCRIPT% /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\\livecode\\" /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo > "%SCRIPT_LOG%" 2>&1
set SCRIPT_ERR=%ERRORLEVEL%
type "%SCRIPT_LOG%"
type "%SCRIPT_LOG%" >> "%LOGFILE%"
if %SCRIPT_ERR% NEQ 0 (
    echo.
    echo LIBSCRIPT BUILD FAILED. See %SCRIPT_LOG% for details.
    exit /b 1
)
echo libScript OK.

echo.
:: ----------------------------------------------------------
:: Build lc-compile toolchain (needed by engine_lcb_modules).
:: Build order (with BuildProjectReferences=false to avoid fetch):
::   grts -> libcompile -> lc-compile-lib -> reflex -> gentle ->
::   lc-bootstrap-compile -> lc-compile-stage2 -> lc-compile-stage3 ->
::   lc-compile  (final wrapper, produces Debug\lc-compile.exe)
:: ----------------------------------------------------------
set SOLDIR=%~dp0build-win-x86_64\livecode\

echo Building grts ...
set VCXPROJ_GRTS=build-win-x86_64\livecode\toolchain\gentle\gentle\grts.vcxproj
"%MSBUILD%" %VCXPROJ_GRTS% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%SOLDIR% /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo
if errorlevel 1 ( echo grts FAILED & exit /b 1 )

echo Building libcompile ...
set VCXPROJ_LIBCOMPILE=build-win-x86_64\livecode\toolchain\libcompile\libcompile.vcxproj
"%MSBUILD%" %VCXPROJ_LIBCOMPILE% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%SOLDIR% /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo
if errorlevel 1 ( echo libcompile FAILED & exit /b 1 )

echo Building lc-compile-lib ...
set VCXPROJ_LCCOMPILELIB=build-win-x86_64\livecode\toolchain\lc-compile\src\lc-compile-lib.vcxproj
"%MSBUILD%" %VCXPROJ_LCCOMPILELIB% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%SOLDIR% /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo
if errorlevel 1 ( echo lc-compile-lib FAILED & exit /b 1 )

echo Building reflex ...
set VCXPROJ_REFLEX=build-win-x86_64\livecode\toolchain\gentle\reflex\reflex.vcxproj
"%MSBUILD%" %VCXPROJ_REFLEX% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%SOLDIR% /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo
if errorlevel 1 ( echo reflex FAILED & exit /b 1 )

echo Building gentle ...
set VCXPROJ_GENTLE=build-win-x86_64\livecode\toolchain\gentle\gentle\gentle.vcxproj
"%MSBUILD%" %VCXPROJ_GENTLE% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%SOLDIR% /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo
if errorlevel 1 ( echo gentle FAILED & exit /b 1 )

echo Building lc-bootstrap-compile ...
set VCXPROJ_LCBOOTSTRAP=build-win-x86_64\livecode\toolchain\lc-compile\src\lc-bootstrap-compile.vcxproj
"%MSBUILD%" %VCXPROJ_LCBOOTSTRAP% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%SOLDIR% /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo
if errorlevel 1 ( echo lc-bootstrap-compile FAILED & exit /b 1 )

:: Copy ICU DLLs to Debug so toolchain executables can load them at runtime.
:: sicudt.lib is an import lib for icudt58.dll, so lc-bootstrap-compile.exe
:: requires icudt58.dll to be present in the same directory when it runs.
:: On a fresh CI checkout there is no Release\ yet, so we source the DLLs
:: from ide\Runtime\Windows\x86-64\ (always present) and fall back to
:: Release\ for developer machines that already have a Release build.
echo Copying ICU DLLs to Debug output dir ...
if not exist "%~dp0build-win-x86_64\livecode\Debug" mkdir "%~dp0build-win-x86_64\livecode\Debug"
set "ICU_RT_DIR=%~dp0ide\Runtime\Windows\x86-64"
set "ICU_REL_DIR=%~dp0build-win-x86_64\livecode\Release"
for %%F in (icudt58.dll icuin58.dll icutu58.dll icuuc58.dll) do (
    if exist "%ICU_REL_DIR%\%%F" (
        copy /Y "%ICU_REL_DIR%\%%F" "%~dp0build-win-x86_64\livecode\Debug\" >nul
    ) else if exist "%ICU_RT_DIR%\%%F" (
        copy /Y "%ICU_RT_DIR%\%%F" "%~dp0build-win-x86_64\livecode\Debug\" >nul
    ) else (
        echo WARNING: %%F not found in Release or ide\Runtime\Windows\x86-64
    )
)
echo ICU DLLs copied.

echo Building lc-compile-stage2 ...
set VCXPROJ_LCSTAGE2=build-win-x86_64\livecode\toolchain\lc-compile\src\lc-compile-stage2.vcxproj
"%MSBUILD%" %VCXPROJ_LCSTAGE2% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%SOLDIR% /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo
if errorlevel 1 ( echo lc-compile-stage2 FAILED & exit /b 1 )

echo Building lc-compile-stage3 ...
set VCXPROJ_LCSTAGE3=build-win-x86_64\livecode\toolchain\lc-compile\src\lc-compile-stage3.vcxproj
"%MSBUILD%" %VCXPROJ_LCSTAGE3% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%SOLDIR% /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo
if errorlevel 1 ( echo lc-compile-stage3 FAILED & exit /b 1 )

echo Building lc-compile ...
set VCXPROJ_LCCOMPILE=build-win-x86_64\livecode\toolchain\lc-compile\lc-compile.vcxproj
"%MSBUILD%" %VCXPROJ_LCCOMPILE% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%SOLDIR% /p:RuntimeLibrary=MultiThreadedDebug /v:minimal /nologo
if errorlevel 1 ( echo lc-compile FAILED & exit /b 1 )
echo lc-compile.exe OK.

echo.
:: ----------------------------------------------------------
:: Build LCB engine modules (compiles engine/src/browser.lcb et al.
:: via lc-compile; produces engine_lcb_modules.cpp and .lci files).
:: Must run before the engine so any .lcb changes are picked up.
:: ----------------------------------------------------------
echo Building LCB engine modules ...
echo Building LCB engine modules ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LCB_MODULES% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 (
    echo.
    echo LCB MODULES BUILD FAILED. See %LOGFILE% for details.
    exit /b 1
)
echo LCB modules OK.

echo.
:: ----------------------------------------------------------
:: Generate startupstack.cpp (shared_intermediate/src/startupstack.cpp).
::
:: dependency chain:
::   descriptify_environment_stack.vcxproj
::     → runs server-community.exe (committed bootstrap binary) to produce
::       shared_intermediate/src/environment_descriptified.livecode
::   encode_environment_stack.vcxproj
::     → runs util/compress_data.py on the .livecode file to produce
::       shared_intermediate/src/startupstack.cpp
::
:: development.vcxproj compiles startupstack.cpp, so this must run before
:: the engine build.  On the developer's machine the file is a leftover from
:: a previous build; on a clean CI checkout it does not exist.
::
:: We build encode_environment_stack with BuildProjectReferences=true so
:: MSBuild automatically chains descriptify_environment_stack first.
:: ----------------------------------------------------------
:: Step 1: descriptify_environment_stack — runs server-community.exe to produce
:: environment_descriptified.livecode, then encode_environment_stack encodes
:: it into startupstack.cpp.
::
:: server-community.exe is not committed to the repo.  On developer machines it
:: is present in Release\ from a prior build.  On a clean CI checkout it does
:: not exist, so we fall back to engine\src\bootstrap-startupstack.cpp — a
:: pre-generated version committed to the repo.  The bootstrap file is
:: regenerated whenever the environment stack changes by running:
::   make compile-mac  (macOS)
:: and committing the updated engine\src\bootstrap-startupstack.cpp.
:: ----------------------------------------------------------
set "BOOTSTRAP_STARTUP=%~dp0engine\src\bootstrap-startupstack.cpp"
set "STARTUP_CPP=%SHARED_INT%\src\startupstack.cpp"

:: Prefer the live descriptify path if server-community.exe is available.
:: (Variables inside parenthesised blocks expand at parse time in cmd.exe,
::  so all error checks must be at the top level — use goto for branching.)
set "SERVER_EXE=%~dp0build-win-x86_64\livecode\Release\server-community.exe"
if not exist "%~dp0build-win-x86_64\livecode\Debug" mkdir "%~dp0build-win-x86_64\livecode\Debug"
if exist "%SERVER_EXE%" copy /Y "%SERVER_EXE%" "%~dp0build-win-x86_64\livecode\Debug\" >nul
if not exist "%~dp0build-win-x86_64\livecode\Debug\server-community.exe" goto use_bootstrap_startup

echo Generating environment_descriptified.livecode (descriptify_environment_stack) ...
echo Generating environment_descriptified.livecode ... >> "%LOGFILE%"
set "VCXPROJ_DESCRIPTIFY=build-win-x86_64\livecode\engine\descriptify_environment_stack.vcxproj"
set "DESCRIPTIFY_LOG=%~dp0build-descriptify-stack.log"
"%MSBUILD%" %VCXPROJ_DESCRIPTIFY% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo > "%DESCRIPTIFY_LOG%" 2>&1
set DESCRIPTIFY_ERR=%ERRORLEVEL%
type "%DESCRIPTIFY_LOG%"
type "%DESCRIPTIFY_LOG%" >> "%LOGFILE%"
if %DESCRIPTIFY_ERR% NEQ 0 goto use_bootstrap_startup

echo Generating startupstack.cpp (encode_environment_stack) ...
echo Generating startupstack.cpp ... >> "%LOGFILE%"
set "VCXPROJ_ENCODE=build-win-x86_64\livecode\engine\encode_environment_stack.vcxproj"
set "ENCODE_LOG=%~dp0build-encode-stack.log"
"%MSBUILD%" %VCXPROJ_ENCODE%  "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%ENCODE_LOG%" 2>&1
set ENCODE_ERR=%ERRORLEVEL%
type "%ENCODE_LOG%"
type "%ENCODE_LOG%" >> "%LOGFILE%"
if %ENCODE_ERR% NEQ 0 goto use_bootstrap_startup
echo startupstack.cpp OK.
goto startup_done

:use_bootstrap_startup
echo server-community.exe not available or descriptify failed -- using bootstrap-startupstack.cpp
if not exist "%BOOTSTRAP_STARTUP%" (
    echo ERROR: engine\src\bootstrap-startupstack.cpp not found.
    exit /b 1
)
if not exist "%SHARED_INT%\src" mkdir "%SHARED_INT%\src"
copy /Y "%BOOTSTRAP_STARTUP%" "%STARTUP_CPP%" >nul
if not exist "%STARTUP_CPP%" (
    echo ERROR: Failed to copy bootstrap-startupstack.cpp to %STARTUP_CPP%
    exit /b 1
)
echo bootstrap-startupstack.cpp copied OK.
:: Also mirror to the project-local path that build-release-x64.bat expects.
:: When development.vcxproj is built standalone (not from the solution),
:: $(obj) resolves to empty so shared_intermediate lands at D:\shared_intermediate.
:: build-release-x64.bat looks in the project-relative location:
::   build-win-x86_64\livecode\engine\Debug\x64\obj\shared_intermediate\src\
:: Copy there so both find it.
set "LOCAL_DBG_SHARED=%~dp0build-win-x86_64\livecode\engine\Debug\x64\obj\shared_intermediate\src"
if not exist "%LOCAL_DBG_SHARED%" mkdir "%LOCAL_DBG_SHARED%"
copy /Y "%STARTUP_CPP%" "%LOCAL_DBG_SHARED%\startupstack.cpp" >nul
echo startupstack.cpp mirrored to project-local Debug shared_intermediate.
:startup_done

echo.
:: ----------------------------------------------------------
:: Build security-community.lib (stacksecurity.cpp — sets
:: license_class and deploy_targets for community builds).
:: Must be explicit because development.vcxproj uses
:: BuildProjectReferences=false.
:: ----------------------------------------------------------
:: ----------------------------------------------------------
:: Generate revbuild.h — contains build version defines required by the
:: precompiled header (w32prefix.h).  Without it every translation unit
:: fails with "Cannot open include file: revbuild.h" and cascades into
:: hundreds of undefined-type errors.
:: encode_version.vcxproj runs util/encode_version.pl (Perl) to produce
:: shared_intermediate/include/revbuild.h from engine/include/revbuild.h.in.
:: ----------------------------------------------------------
echo Generating revbuild.h (encode_version) ...
echo Generating revbuild.h ... >> "%LOGFILE%"
set "VCXPROJ_ENCODE_VER=build-win-x86_64\livecode\engine\encode_version.vcxproj"
set "ENCVER_LOG=%~dp0build-encode-version.log"
"%MSBUILD%" %VCXPROJ_ENCODE_VER% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo > "%ENCVER_LOG%" 2>&1
set ENCVER_ERR=%ERRORLEVEL%
type "%ENCVER_LOG%"
type "%ENCVER_LOG%" >> "%LOGFILE%"
if %ENCVER_ERR% NEQ 0 (
    echo ERROR: encode_version failed. See %ENCVER_LOG%
    exit /b 1
)
echo revbuild.h OK.

echo.
echo Building security-community ...
echo Building security-community ... >> "%LOGFILE%"
set "SECCOM_LOG=%~dp0build-security-community.log"
"%MSBUILD%" %VCXPROJ_SECURITY_COMMUNITY% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo > "%SECCOM_LOG%" 2>&1
set SECCOM_ERR=%ERRORLEVEL%
type "%SECCOM_LOG%"
type "%SECCOM_LOG%" >> "%LOGFILE%"
if %SECCOM_ERR% NEQ 0 (
    echo.
    echo SECURITY-COMMUNITY BUILD FAILED. See %SECCOM_LOG% for details.
    exit /b 1
)
echo security-community OK.

echo.
:: ----------------------------------------------------------
:: Build kernel.lib — the main engine object library (~300 source files).
::
:: Root cause of previous winnt.h(24176) C2059 failures (now fixed):
::   sysdefs.h defined #define None 0 which collided with the
::   IMAGE_POLICY_ENTRY.None union member added in Windows SDK 26100.
::   Fixed in engine\src\w32prefix.h: #undef None before <windows.h>,
::   #define None 0 restored after.  All engine files can now compile.
::
:: /p:OutDir="..\Debug\" overrides the standalone output directory so
:: kernel.lib lands in build-win-x86_64\livecode\Debug\lib\kernel.lib —
:: the same path that development.vcxproj links against.  Without this,
:: a standalone kernel.vcxproj build puts the lib under engine\Debug\lib\
:: which development.vcxproj never finds.
:: ----------------------------------------------------------
echo Building kernel ...
echo Building kernel ... >> "%LOGFILE%"
set "KERNEL_LOG=%~dp0build-kernel.log"
:: OutDir quoting note:
::   The C runtime's CommandLineToArgvW treats \" as an escaped quote, so
::   "/p:OutDir=..\Debug\" would consume the rest of the command line.
::   Fix: use an absolute path and end with \\ inside the quotes — the C
::   runtime converts \\" to one literal \ followed by the closing quote.
set "KERNEL_OUTDIR=%~dp0build-win-x86_64\livecode\Debug"
"%MSBUILD%" %VCXPROJ_KERNEL% /t:Rebuild /p:Configuration=Debug /p:Platform=x64 "/p:OutDir=%KERNEL_OUTDIR%\\" /p:BuildProjectReferences=false /v:minimal /nologo > "%KERNEL_LOG%" 2>&1
set KERNEL_ERR=%ERRORLEVEL%
type "%KERNEL_LOG%"
type "%KERNEL_LOG%" >> "%LOGFILE%"
if %KERNEL_ERR% NEQ 0 (
    echo.
    echo KERNEL BUILD FAILED. See %KERNEL_LOG% for details.
    exit /b 1
)
echo kernel OK.

echo.
:: ----------------------------------------------------------
:: Patch kernel.lib with printer base class (MCPrinter) and Windows
:: subclass (MCWindowsPrinter).
::
:: kernel.vcxproj compiles printer.cpp, w32printer.cpp, and
:: customprinter.cpp, but the winnt.h SDK conflict historically prevented
:: w32printer.cpp from compiling correctly inside vcxproj.  compile-printer.bat
:: compiles all three with the correct flags and patches the resulting objects
:: into kernel.lib so the linker can always resolve all MCPrinter:: symbols.
::
:: This must run AFTER the kernel build (so kernel.lib exists) and BEFORE
:: the engine link step (so the symbols are present when needed).
:: ----------------------------------------------------------
echo Patching kernel.lib with printer objects ...
echo Patching kernel.lib with printer objects ... >> "%LOGFILE%"
call "%~dp0compile-printer.bat" Debug
if errorlevel 1 (
    echo.
    echo PRINTER PATCH FAILED.
    exit /b 1
)
echo Printer patch OK.

echo.
:: ----------------------------------------------------------
:: Build kernel-development.lib — contains the bulk of engine
:: source files (deploy.cpp, license.cpp, etc.).  Must be built
:: explicitly because development.vcxproj uses
:: BuildProjectReferences=false and will fail with LNK1181 if
:: this lib is missing or stale.
:: ----------------------------------------------------------
:: Build perfect-target.exe — hash-string generator used by kernel-development's
:: custom build step (hash_strings.pl).  Must exist in Debug\ before kdev builds.
echo Building perfect-target ...
echo Building perfect-target ... >> "%LOGFILE%"
set "PERFECT_LOG=%~dp0build-perfect.log"
"%MSBUILD%" %VCXPROJ_PERFECT% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo > "%PERFECT_LOG%" 2>&1
set PERFECT_ERR=%ERRORLEVEL%
type "%PERFECT_LOG%"
type "%PERFECT_LOG%" >> "%LOGFILE%"
if %PERFECT_ERR% NEQ 0 (
    echo.
    echo PERFECT BUILD FAILED. See %PERFECT_LOG% for details.
    exit /b 1
)
echo perfect-target OK.

echo.
echo Building kernel-development ...
echo Building kernel-development ... >> "%LOGFILE%"
set "KDEV_LOG=%~dp0build-kernel-development.log"
"%MSBUILD%" %VCXPROJ_KERNEL_DEVELOPMENT% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo > "%KDEV_LOG%" 2>&1
set KDEV_ERR=%ERRORLEVEL%
type "%KDEV_LOG%"
type "%KDEV_LOG%" >> "%LOGFILE%"
if %KDEV_ERR% NEQ 0 (
    echo.
    echo KERNEL-DEVELOPMENT BUILD FAILED. See %KDEV_LOG% for details.
    exit /b 1
)
echo kernel-development OK.

echo.
:: ----------------------------------------------------------
:: Build graphics/rendering dependency libs (leaf → trunk order):
::   libgif, libjpeg, libpng, libpcre, libexpat → libskia (+ opts)
::   → libGraphics → stdscript
:: These are ProjectReference inputs for development.vcxproj and must
:: exist in Debug\lib\ before the engine link step.
:: ----------------------------------------------------------
echo Building libgif ...
echo Building libgif ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LIBGIF% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libgif failed. & exit /b 1 )

echo Building libjpeg ...
echo Building libjpeg ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LIBJPEG% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libjpeg failed. & exit /b 1 )

echo Building libpng ...
echo Building libpng ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LIBPNG% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libpng failed. & exit /b 1 )

echo Building libpcre ...
echo Building libpcre ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LIBPCRE% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libpcre failed. & exit /b 1 )

echo Building libexpat ...
echo Building libexpat ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LIBEXPAT% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libexpat failed. & exit /b 1 )

echo Building libskia (this may take a while) ...
echo Building libskia ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LIBSKIA% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libskia failed. & exit /b 1 )
"%MSBUILD%" %VCXPROJ_LIBSKIA_OPT_NONE% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libskia_opt_none failed. & exit /b 1 )
"%MSBUILD%" %VCXPROJ_LIBSKIA_OPT_SSE2% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libskia_opt_sse2 failed. & exit /b 1 )
"%MSBUILD%" %VCXPROJ_LIBSKIA_OPT_SSE3% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libskia_opt_sse3 failed. & exit /b 1 )
"%MSBUILD%" %VCXPROJ_LIBSKIA_OPT_SSE41% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libskia_opt_sse41 failed. & exit /b 1 )
"%MSBUILD%" %VCXPROJ_LIBSKIA_OPT_SSE42% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libskia_opt_sse42 failed. & exit /b 1 )
"%MSBUILD%" %VCXPROJ_LIBSKIA_OPT_AVX% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libskia_opt_avx failed. & exit /b 1 )
"%MSBUILD%" %VCXPROJ_LIBSKIA_OPT_HSW% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libskia_opt_hsw failed. & exit /b 1 )
"%MSBUILD%" %VCXPROJ_LIBSKIA_OPT_ARM% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libskia_opt_arm failed. & exit /b 1 )
echo libskia OK.

echo Building libGraphics ...
echo Building libGraphics ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LIBGRAPHICS% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: libGraphics failed. & exit /b 1 )
echo libGraphics OK.

echo Building stdscript ...
echo Building stdscript ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_STDSCRIPT% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo ERROR: stdscript failed. & exit /b 1 )
echo stdscript OK.

echo.
:: ----------------------------------------------------------
:: Generate revsecurity.lib import library.
:: revsecurity.vcxproj is a DLL that wraps OpenSSL; the OpenSSL
:: prebuilt static libs are not in the repo so we cannot build the
:: DLL itself.  The engine only needs the import library at link
:: time (the actual revsecurity.dll is already present in Debug\).
:: Steps:
::   1. Run libopenssl_symbol_list_win.vcxproj to generate
::      revsecurity.def from ssl.stubs via list_stub_symbols.pl.
::   2. Use lib.exe /def: to produce the import library.
:: ----------------------------------------------------------
echo Generating revsecurity.def ...
echo Generating revsecurity.def ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_OPENSSL_SYMLIST% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: libopenssl_symbol_list_win failed.
    exit /b 1
)
set "REVSEC_DEF=%~dp0build-win-x86_64\livecode\Debug\obj\global_intermediate\src\revsecurity.def"
if not exist "%REVSEC_DEF%" (
    echo ERROR: revsecurity.def was not generated at %REVSEC_DEF%
    exit /b 1
)

:: Find lib.exe alongside the cl.exe that MSBuild uses
set "FIND_LIB_PS1=%TEMP%\hxt_find_lib.ps1"
echo $pf = [System.Environment]::GetEnvironmentVariable('ProgramFiles(x86)')> "%FIND_LIB_PS1%"
echo $vs = "$pf\Microsoft Visual Studio\Installer\vswhere.exe">> "%FIND_LIB_PS1%"
echo if (Test-Path $vs) { ^& $vs -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'VC\Tools\MSVC\**\bin\Hostx64\x64\lib.exe' ^| Select-Object -First 1 }>> "%FIND_LIB_PS1%"
for /f "tokens=*" %%i in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%FIND_LIB_PS1%"') do set "LIB_EXE=%%i"
del "%FIND_LIB_PS1%" 2>nul
if not defined LIB_EXE (
    echo ERROR: lib.exe not found via vswhere.
    exit /b 1
)

echo Generating revsecurity.lib import library using lib.exe ...
echo Generating revsecurity.lib ... >> "%LOGFILE%"
if not exist "%~dp0build-win-x86_64\livecode\Debug" mkdir "%~dp0build-win-x86_64\livecode\Debug"
"%LIB_EXE%" /def:"%REVSEC_DEF%" /out:"%~dp0build-win-x86_64\livecode\Debug\revsecurity.lib" /machine:x64 >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: lib.exe failed to create revsecurity.lib
    exit /b 1
)
echo revsecurity.lib OK.

echo.
echo Building revsecurity.dll (Debug) ...
echo Building revsecurity.dll ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_REVSECURITY% /p:Configuration=Debug /p:Platform=x64 "/p:OutDir=%~dp0build-win-x86_64\livecode\Debug\\" /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: revsecurity.dll Debug build failed -- Release fallback will not be available.
) else (
    echo revsecurity.dll OK.
)

echo.
:: ----------------------------------------------------------
:: Build libxml2 and libxslt BEFORE the engine — development.vcxproj
:: links libxml.lib and libxslt.lib directly, so they must exist on
:: disk before the link step runs.  revxml (a DLL loaded at runtime)
:: is built after standalone-community.exe.
:: ----------------------------------------------------------
echo Building libxml2 (2.15.3) ...
echo Building libxml2 ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LIBXML% /t:Rebuild  "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: libxml2 build failed.
    exit /b 1
)
echo libxml2 OK.

echo Building libxslt ...
echo Building libxslt ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_LIBXSLT% /t:Rebuild  "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: libxslt build failed.
    exit /b 1
)
echo libxslt OK.

echo.
:: ----------------------------------------------------------
:: Create stub libvlc.lib (import library for delay-loaded libvlc.dll)
::
:: VLC is not installed on CI runners.  The engine delay-loads libvlc.dll
:: (DelayLoadDLLs in kernel.gyp) so no VLC installation is required at
:: runtime — EnsureVLCInstance() returns false gracefully when the DLL is
:: absent.  However the MSVC linker still requires an import library at
:: link time in order to resolve the symbol references.
::
:: We generate a minimal stub import library from a .def file that lists
:: every libvlc symbol imported by the engine (vlc-player.cpp).
:: lib.exe /DEF creates the import library without needing VLC installed.
:: ----------------------------------------------------------
echo Creating stub libvlc.lib for linker ...
set "VLC_DEF=%TEMP%\hxt_libvlc.def"
set "VLC_LIB_DIR=%~dp0build-win-x86_64\livecode\Debug\lib"
set "VLC_LIB=%VLC_LIB_DIR%\libvlc.lib"

(
    echo LIBRARY libvlc
    echo EXPORTS
    echo     libvlc_new
    echo     libvlc_release
    echo     libvlc_get_version
    echo     libvlc_errmsg
    echo     libvlc_log_set
    echo     libvlc_media_new_location
    echo     libvlc_media_new_path
    echo     libvlc_media_parse
    echo     libvlc_media_parse_with_options
    echo     libvlc_media_release
    echo     libvlc_media_get_duration
    echo     libvlc_media_tracks_get
    echo     libvlc_media_tracks_release
    echo     libvlc_media_player_new
    echo     libvlc_media_player_release
    echo     libvlc_media_player_set_media
    echo     libvlc_media_player_play
    echo     libvlc_media_player_pause
    echo     libvlc_media_player_stop
    echo     libvlc_media_player_set_rate
    echo     libvlc_media_player_get_state
    echo     libvlc_media_player_get_time
    echo     libvlc_media_player_set_time
    echo     libvlc_media_player_get_length
    echo     libvlc_media_player_set_hwnd
    echo     libvlc_media_player_set_nsobject
    echo     libvlc_media_player_set_xwindow
    echo     libvlc_media_player_event_manager
    echo     libvlc_event_attach
    echo     libvlc_audio_set_volume
    echo     libvlc_video_get_size
    echo     libvlc_video_set_callbacks
    echo     libvlc_video_set_format_callbacks
) > "%VLC_DEF%"

if not exist "%VLC_LIB_DIR%" mkdir "%VLC_LIB_DIR%"
"%LIB_EXE%" /NOLOGO /DEF:"%VLC_DEF%" /MACHINE:X64 /OUT:"%VLC_LIB%" > nul 2>&1
del "%VLC_DEF%" 2>nul
if not exist "%VLC_LIB%" (
    echo ERROR: Failed to create stub libvlc.lib
    exit /b 1
)
echo Stub libvlc.lib created: %VLC_LIB%
:: The directory is added to the linker's AdditionalLibraryDirectories via the
:: atl-include.props ForceImportBeforeCppTargets file (see CI workflow step
:: "Inject ATL include path via MSBuild props").  No LIB env var change needed.

echo.
echo Building engine ...

set "EXE=build-win-x86_64\livecode\Debug\HyperXTalk.exe"
set "ENGINE_LOG=%~dp0build-engine-step.log"
set "LINK_TLOG=build-win-x86_64\livecode\engine\Debug\x64\obj\development\development.tlog\link.write.1.tlog"

:: If the exe is missing, delete the linker tlog so MSBuild is forced
:: to rerun the link step even when no source files changed.
if not exist "%EXE%" (
    if exist "%LINK_TLOG%" (
        del /F /Q "%LINK_TLOG%"
        echo Cleared linker tlog to force relink.
    )
)

"%MSBUILD%" %VCXPROJ_ENGINE% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo > "%ENGINE_LOG%" 2>&1
set BUILD_ERR=%ERRORLEVEL%

:: Show the engine build output (errors and warnings) on the console.
type "%ENGINE_LOG%"
type "%ENGINE_LOG%" >> "%LOGFILE%"

if %BUILD_ERR% NEQ 0 (
    echo.
    echo ENGINE BUILD FAILED.
    exit /b 1
)

if not exist "%EXE%" (
    echo.
    echo ENGINE BUILD FAILED - HyperXTalk.exe was not produced.
    exit /b 1
)

echo Engine built: %EXE%

echo.
:: ----------------------------------------------------------
:: Build kernel-standalone.lib for x64.
:: The pre-built lib shipped as x86; we must compile it for x64 so that
:: standalone-community.exe can link against it.  Only the two mode source
:: files (mode_standalone.cpp, lextable.cpp) are compiled here — the
:: winnt.h SDK conflict does NOT affect these files.
:: BuildProjectReferences=false: encode_version and perfect are already
:: present from the engine build; no need to rebuild them.
:: ----------------------------------------------------------
echo Building kernel-standalone (x64 mode library) ...
echo Building kernel-standalone ... >> "%LOGFILE%"
set "KSTD_LOG=%~dp0build-kernel-standalone.log"
"%MSBUILD%" %VCXPROJ_KERNEL_STANDALONE% /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo > "%KSTD_LOG%" 2>&1
set KSTD_ERR=%ERRORLEVEL%
type "%KSTD_LOG%"
type "%KSTD_LOG%" >> "%LOGFILE%"
if %KSTD_ERR% NEQ 0 (
    echo.
    echo KERNEL-STANDALONE BUILD FAILED. See %KSTD_LOG% for details.
    exit /b 1
)
echo kernel-standalone OK.

echo.
:: ----------------------------------------------------------
:: Build standalone-community.exe (required by the standalone builder).
:: The IDE's revSBEnginePath resolves to standalone-community.exe when
:: editionType is "community" (or falls back to it for community builds).
:: ----------------------------------------------------------
echo Building standalone-community.exe ...
echo Building standalone-community.exe ... >> "%LOGFILE%"
set "STANDALONE_LOG=%~dp0build-standalone.log"
set "STANDALONE_EXE=build-win-x86_64\livecode\Debug\standalone-community.exe"
:: BuildProjectReferences=false: kernel-standalone.lib, security-community.lib and
:: kernel.lib are already pre-built; we just link against them without rebuilding
:: (rebuilding them triggers winnt.h SDK conflicts in those older vcxprojs).
"%MSBUILD%" %VCXPROJ_STANDALONE% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo > "%STANDALONE_LOG%" 2>&1
set STANDALONE_ERR=%ERRORLEVEL%
type "%STANDALONE_LOG%" >> "%LOGFILE%"
if %STANDALONE_ERR% NEQ 0 goto standalone_failed
echo standalone-community.exe OK.
goto standalone_done
:standalone_failed
echo.
:: Write filtered errors to a small file for easy inspection.
set "STANDALONE_ERRORS=%~dp0build-standalone-errors.log"
findstr /v /r "LNK4099\|LNK4075" "%STANDALONE_LOG%" > "%STANDALONE_ERRORS%"
echo STANDALONE BUILD FAILED. Filtered errors (excluding LNK4099/LNK4075 noise):
type "%STANDALONE_ERRORS%"
if not exist "%STANDALONE_EXE%" (
    echo ERROR: standalone-community.exe is missing and could not be built.
    exit /b 1
)
echo WARNING: Using existing standalone-community.exe from a previous build.
:standalone_done

echo.
:: ----------------------------------------------------------
:: Build revxml (IDE XML external).
:: libxml.lib / libxslt.lib were already built before the engine above.
:: revxml.dll is loaded at runtime by the IDE — it does not need to
:: exist before the engine links.
:: ----------------------------------------------------------
echo Building revxml (IDE XML external) ...
echo Building revxml ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_REVXML% /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: revxml build failed.
    exit /b 1
)
echo revxml OK.

echo.
:: ----------------------------------------------------------
:: Build server-revzip.dll (Debug) — needed by the LCB
:: extensions step below (extension-utils loads revzip via
:: __EnsureExternal "revzip" to package .lcm files into zips).
:: libz and libzip must exist in Debug\lib\ first.
:: ----------------------------------------------------------
echo Building server-revxml.dll (Debug — for extension packaging) ...
echo Building server-revxml.dll ... >> "%LOGFILE%"
set "VCXPROJ_LIBZIP=build-win-x86_64\livecode\thirdparty\libzip\libzip.vcxproj"
"%MSBUILD%" %VCXPROJ_LIBZIP% /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo WARNING: libzip Debug build failed -- server-revzip may not link. )
echo Building server-revzip.dll (Debug) ...
echo Building server-revzip.dll ... >> "%LOGFILE%"
"%MSBUILD%" %VCXPROJ_REVZIP_SERVER% /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: server-revzip.dll Debug build failed -- extension packaging may fail.
) else (
    echo server-revzip.dll OK.
)

echo.
echo Building revbrowser (Debug — used as fallback by build-release-x64.bat) ...
echo Building revbrowser (Debug) ... >> "%LOGFILE%"
set "REVBROWSER_LOG=%~dp0build-revbrowser.log"
"%MSBUILD%" %VCXPROJ_REVBROWSER%  "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%REVBROWSER_LOG%" 2>&1
set REVBROWSER_ERR=%ERRORLEVEL%
type "%REVBROWSER_LOG%"
type "%REVBROWSER_LOG%" >> "%LOGFILE%"
if %REVBROWSER_ERR% NEQ 0 (
    echo WARNING: revbrowser Debug build failed -- Release step will also fail.
) else (
    echo revbrowser Debug OK.
)

echo.
:: ----------------------------------------------------------
:: Build kernel-server + server-community.exe (Debug).
::
:: server-community.exe is used below to compile LCB extensions.
:: kernel-server is the LiveCode server mode kernel; it is built
:: separately from kernel (IDE mode) because it defines MODE_SERVER.
:: ----------------------------------------------------------
echo Building kernel-server ...
echo Building kernel-server ... >> "%LOGFILE%"
"%MSBUILD%" build-win-x86_64\livecode\engine\kernel-server.vcxproj /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: kernel-server build failed -- server-community.exe will not be available.
    goto skip_server_build
)
echo kernel-server OK.

echo.
echo Building server-community.exe (Debug) ...
echo Building server-community.exe ... >> "%LOGFILE%"
"%MSBUILD%" build-win-x86_64\livecode\engine\server.vcxproj /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\livecode\\" /v:minimal /nologo
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: server-community.exe build failed -- LCB extension build will be skipped.
) else (
    echo server-community.exe OK.
    :: Copy to Debug root so the LCB extension step can find it
    if exist "build-win-x86_64\livecode\Debug\server-community.exe" (
        echo server-community.exe already in Debug root.
    )
)
:skip_server_build

echo.
:: ----------------------------------------------------------
:: Build LCB extensions (module libraries + widgets) for Debug.
::
:: Uses server-community.exe with extension-utils.lc in
:: buildlcbextensions mode.  The script handles dependency ordering,
:: generates manifest.xml in each source directory, compiles
:: .lcb → .lcm, packages and extracts to
:: Debug\packaged_extensions\<module-id>\.
::
:: server-revzip.dll is bootstrapped from Release\ because the
:: Debug bat does not build server-revzip (it's Release-only).
:: ----------------------------------------------------------
echo.
echo Building LCB extensions (libraries and widgets, Debug) ...
echo Building LCB extensions ... >> "%LOGFILE%"
set "LC_COMPILE=build-win-x86_64\livecode\Debug\lc-compile.exe"
set "DBG_OUT=%~dp0build-win-x86_64\livecode\Debug"
set "LCI_DIR=%DBG_OUT%\modules\lci"
set "EXT_LOG_DBG=%~dp0build-extensions-debug.log"
set "EXT_UTILS=%~dp0extensions\script-libraries\extension-utils\resources\extension-utils.lc"
set "PACKAGED_EXT_DBG=%DBG_OUT%\packaged_extensions"

if not exist "%LC_COMPILE%" (
    echo ERROR: lc-compile.exe not found. Build the engine first.
    exit /b 1
)

:: Bootstrap server-revzip.dll from Release so extension-utils can
:: load it via __EnsureExternal "revzip".
if not exist "%DBG_OUT%\server-revzip.dll" (
    if exist "%~dp0build-win-x86_64\livecode\Release\server-revzip.dll" (
        copy /Y "%~dp0build-win-x86_64\livecode\Release\server-revzip.dll" "%DBG_OUT%\server-revzip.dll" > nul
        echo Bootstrapped server-revzip.dll from Release.
    )
)
:: Likewise for server-revxml.dll (revxml Debug is built above,
:: but use Release as a belt-and-suspenders fallback).
if not exist "%DBG_OUT%\server-revxml.dll" (
    if exist "%~dp0build-win-x86_64\livecode\Release\server-revxml.dll" (
        copy /Y "%~dp0build-win-x86_64\livecode\Release\server-revxml.dll" "%DBG_OUT%\server-revxml.dll" > nul
        echo Bootstrapped server-revxml.dll from Release.
    )
)

if not exist "%PACKAGED_EXT_DBG%" mkdir "%PACKAGED_EXT_DBG%"

"%DBG_OUT%\server-community.exe" "%EXT_UTILS%" buildlcbextensions "%~dp0ide-support\revdocsparser.livecodescript" "%PACKAGED_EXT_DBG%" false "%LC_COMPILE%" "%LCI_DIR%" "" "%~dp0extensions\modules\widget-utils\widget-utils.lcb" "%~dp0extensions\modules\scriptitems\scriptitems.lcb" "%~dp0extensions\libraries\canvas\canvas.lcb" "%~dp0extensions\libraries\iconsvg\iconsvg.lcb" "%~dp0extensions\libraries\json\json.lcb" "%~dp0extensions\libraries\objectrepository\objectrepository.lcb" "%~dp0extensions\libraries\ini\ini.lcb" "%~dp0extensions\libraries\timezone\timezone.lcb" "%~dp0extensions\widgets\svgpath\svgpath.lcb" "%~dp0extensions\widgets\clock\clock.lcb" "%~dp0extensions\widgets\graph\graph.lcb" "%~dp0extensions\widgets\header\header.lcb" "%~dp0extensions\widgets\iconpicker\iconpicker.lcb" "%~dp0extensions\widgets\navbar\navbar.lcb" "%~dp0extensions\widgets\paletteactions\paletteactions.lcb" "%~dp0extensions\widgets\segmented\segmented.lcb" "%~dp0extensions\widgets\switchbutton\switchbutton.lcb" "%~dp0extensions\widgets\treeview\treeview.lcb" "%~dp0extensions\widgets\colorswatch\colorswatch.lcb" "%~dp0extensions\widgets\gradientrampeditor\gradientrampeditor.lcb" "%~dp0extensions\widgets\tile\tile.lcb" "%~dp0extensions\widgets\spinner\spinner.lcb" > "%EXT_LOG_DBG%" 2>&1
set EXT_DBG_ERR=%ERRORLEVEL%
type "%EXT_LOG_DBG%"
type "%EXT_LOG_DBG%" >> "%LOGFILE%"
if %EXT_DBG_ERR% NEQ 0 (
    echo WARNING: LCB extension build had errors. IDE palette may be incomplete. See %EXT_LOG_DBG%
) else (
    echo LCB extensions OK.
)

echo.
:: ----------------------------------------------------------
:: Compile browser widget directly with lc-compile (no -Werror,
:: writes module.lcm straight into packaged_extensions).
:: ----------------------------------------------------------
echo Compiling browser widget (Debug) ...
echo Compiling browser widget ... >> "%LOGFILE%"
set "BROWSER_PKG_DBG=%PACKAGED_EXT_DBG%\com.livecode.widget.browser"
set "BROWSER_LCB=%~dp0extensions\widgets\browser\browser.lcb"
if not exist "%BROWSER_PKG_DBG%" mkdir "%BROWSER_PKG_DBG%"
if not exist "%BROWSER_PKG_DBG%\manifest.xml" (
    copy /Y "%~dp0extensions\widgets\browser\manifest.xml" "%BROWSER_PKG_DBG%\manifest.xml" > nul
)
if exist "%BROWSER_LCB%" (
    "%LC_COMPILE%" --modulepath "%LCI_DIR%" --manifest "%BROWSER_PKG_DBG%\manifest.xml" --output "%BROWSER_PKG_DBG%\module.lcm" "%BROWSER_LCB%" > nul 2>&1
    if exist "%BROWSER_PKG_DBG%\module.lcm" (
        echo Browser widget compiled OK.
    ) else (
        echo WARNING: browser widget compilation failed -- skipping.
    )
) else (
    echo WARNING: browser.lcb not found -- skipping browser widget compilation.
)

echo.
echo Build completed: %DATE% %TIME%
echo Build completed: %DATE% %TIME% >> "%LOGFILE%"
echo Full log: %LOGFILE%
exit /b 0