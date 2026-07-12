@echo off
setlocal

cd /d "%~dp0"
set LOGFILE=%~dp0build-release-x64.log
set VCXPROJ_ENGINE=build-win-x86_64\hyperxtalk\engine\development.vcxproj
set VCXPROJ_BROWSER=build-win-x86_64\hyperxtalk\libbrowser\libbrowser.vcxproj
set VCXPROJ_DBMYSQL=build-win-x86_64\hyperxtalk\revdb\dbmysql.vcxproj
set VCXPROJ_DBODBC=build-win-x86_64\hyperxtalk\revdb\dbodbc.vcxproj
set VCXPROJ_DBPOSTGRESQL=build-win-x86_64\hyperxtalk\revdb\dbpostgresql.vcxproj
set VCXPROJ_DBSQLITE=build-win-x86_64\hyperxtalk\revdb\dbsqlite.vcxproj
set VCXPROJ_OPENSSL_STUBS=build-win-x86_64\hyperxtalk\thirdparty\libopenssl\libopenssl_stubs.vcxproj
set VCXPROJ_LIBSQLITE=build-win-x86_64\hyperxtalk\thirdparty\libsqlite\libsqlite.vcxproj
set VCXPROJ_LCB_MODULES=build-win-x86_64\hyperxtalk\engine\engine_lcb_modules.vcxproj
set VCXPROJ_LIBFFI=build-win-x86_64\hyperxtalk\thirdparty\libffi\libffi.vcxproj
set VCXPROJ_LIBFOUNDATION=build-win-x86_64\hyperxtalk\libfoundation\libFoundation.vcxproj
set VCXPROJ_LIBSCRIPT=build-win-x86_64\hyperxtalk\libscript\libScript.vcxproj
set VCXPROJ_STANDALONE=build-win-x86_64\hyperxtalk\engine\standalone.vcxproj
set VCXPROJ_KERNEL=build-win-x86_64\hyperxtalk\engine\kernel.vcxproj
set VCXPROJ_KERNEL_STANDALONE=build-win-x86_64\hyperxtalk\engine\kernel-standalone.vcxproj
set VCXPROJ_PERFECT=build-win-x86_64\hyperxtalk\util\perfect\perfect.vcxproj
set VCXPROJ_KERNEL_DEVELOPMENT=build-win-x86_64\hyperxtalk\engine\kernel-development.vcxproj
set VCXPROJ_SECURITY_COMMUNITY=build-win-x86_64\hyperxtalk\engine\security-community.vcxproj
set VCXPROJ_LIBXML=build-win-x86_64\hyperxtalk\thirdparty\libxml\libxml.vcxproj
set VCXPROJ_LIBXSLT=build-win-x86_64\hyperxtalk\thirdparty\libxslt\libxslt.vcxproj
set VCXPROJ_REVXML=build-win-x86_64\hyperxtalk\revxml\external-revxml.vcxproj
set VCXPROJ_REVXML_SERVER=build-win-x86_64\hyperxtalk\revxml\external-revxml-server.vcxproj
set VCXPROJ_REVZIP_SERVER=build-win-x86_64\hyperxtalk\revzip\external-revzip-server.vcxproj
set VCXPROJ_REVBROWSER=build-win-x86_64\hyperxtalk\revbrowser\external-revbrowser.vcxproj
set VCXPROJ_REVDB=build-win-x86_64\hyperxtalk\revdb\external-revdb.vcxproj
set VCXPROJ_OPENSSL_SYMLIST=build-win-x86_64\hyperxtalk\thirdparty\libopenssl\libopenssl_symbol_list_win.vcxproj
set VCXPROJ_REVSECURITY=build-win-x86_64\hyperxtalk\thirdparty\libopenssl\revsecurity.vcxproj
set VCXPROJ_REVZIP=build-win-x86_64\hyperxtalk\revzip\external-revzip.vcxproj
set VCXPROJ_REVSPEECH=build-win-x86_64\hyperxtalk\revspeech\external-revspeech.vcxproj
set VCXPROJ_REVPDFPRINTER=build-win-x86_64\hyperxtalk\revpdfprinter\external-revpdfprinter.vcxproj
set VCXPROJ_LIBEXTERNAL=build-win-x86_64\hyperxtalk\libexternal\libExternal.vcxproj
set VCXPROJ_LIBZ=build-win-x86_64\hyperxtalk\thirdparty\libz\libz.vcxproj
set VCXPROJ_LIBGIF=build-win-x86_64\hyperxtalk\thirdparty\libgif\libgif.vcxproj
set VCXPROJ_LIBPNG=build-win-x86_64\hyperxtalk\thirdparty\libpng\libpng.vcxproj
set VCXPROJ_LIBJPEG=build-win-x86_64\hyperxtalk\thirdparty\libjpeg\libjpeg.vcxproj
set VCXPROJ_LIBPCRE=build-win-x86_64\hyperxtalk\thirdparty\libpcre\libpcre.vcxproj
set VCXPROJ_LIBCAIRO=build-win-x86_64\hyperxtalk\thirdparty\libcairo\libcairo.vcxproj
set VCXPROJ_LIBZIP=build-win-x86_64\hyperxtalk\thirdparty\libzip\libzip.vcxproj
set VCXPROJ_LIBSKIA=build-win-x86_64\hyperxtalk\thirdparty\libskia\libskia.vcxproj
set VCXPROJ_LIBSKIA_NONE=build-win-x86_64\hyperxtalk\thirdparty\libskia\libskia_opt_none.vcxproj
set VCXPROJ_LIBSKIA_ARM=build-win-x86_64\hyperxtalk\thirdparty\libskia\libskia_opt_arm.vcxproj
set VCXPROJ_LIBSKIA_SSE2=build-win-x86_64\hyperxtalk\thirdparty\libskia\libskia_opt_sse2.vcxproj
set VCXPROJ_LIBSKIA_SSE3=build-win-x86_64\hyperxtalk\thirdparty\libskia\libskia_opt_sse3.vcxproj
set VCXPROJ_LIBSKIA_SSE41=build-win-x86_64\hyperxtalk\thirdparty\libskia\libskia_opt_sse41.vcxproj
set VCXPROJ_LIBSKIA_SSE42=build-win-x86_64\hyperxtalk\thirdparty\libskia\libskia_opt_sse42.vcxproj
set VCXPROJ_LIBSKIA_AVX=build-win-x86_64\hyperxtalk\thirdparty\libskia\libskia_opt_avx.vcxproj
set VCXPROJ_LIBSKIA_HSW=build-win-x86_64\hyperxtalk\thirdparty\libskia\libskia_opt_hsw.vcxproj
set VCXPROJ_LIBGRAPHICS=build-win-x86_64\hyperxtalk\libgraphics\libGraphics.vcxproj
set VCXPROJ_STDSCRIPT=build-win-x86_64\hyperxtalk\libscript\stdscript.vcxproj
set VCXPROJ_LIBEXTERNAL_EXPORTS=build-win-x86_64\hyperxtalk\libexternal\libExternal-symbol-exports.vcxproj
set VCXPROJ_LIBCORE=build-win-x86_64\hyperxtalk\libcore\libCore.vcxproj
set VCXPROJ_LCS_EXTENSIONS=build-win-x86_64\hyperxtalk\extensions\lcs-extensions.vcxproj

:: ----------------------------------------------------------
:: Release output directory.
:: All build artifacts land here; the installer picks them up
:: from this location.
:: ----------------------------------------------------------
set "OUTDIR=%~dp0build-win-x86_64\hyperxtalk\Release"

:: ----------------------------------------------------------
:: Mirror Debug import libs into the Release equivalents.
::
:: Two consumers, two locations:
::
::   dbmysql.vcxproj  AdditionalLibraryDirectories uses $(Configuration),
::   so Release links against:
::     prebuilt\unpacked\Thirdparty\x86_64-win32-v142_static_Release\lib
::
::   development.vcxproj links against:
::     build-win-x86_64\hyperxtalk\Release\lib
::
:: setup-mysql-win.bat only populates the Debug variants of both.
:: We copy from Debug → Release here so no extra Scoop installs are needed.
:: ----------------------------------------------------------
set "PREBUILT_DBG=%~dp0prebuilt\unpacked\Thirdparty\x86_64-win32-v142_static_Debug\lib"
set "PREBUILT_REL=%~dp0prebuilt\unpacked\Thirdparty\x86_64-win32-v142_static_Release\lib"
set "DEBUG_LIB_DIR=%~dp0build-win-x86_64\hyperxtalk\Debug\lib"
set "RELEASE_LIB_DIR=%OUTDIR%\lib"

if not exist "%RELEASE_LIB_DIR%" mkdir "%RELEASE_LIB_DIR%"
if not exist "%PREBUILT_REL%"    mkdir "%PREBUILT_REL%"

:: ---- Ensure Debug prebuilt libs exist (run setup scripts if needed) ----
set "DBG_MYSQL_PREBUILT=%PREBUILT_DBG%\libmysql.lib"
if exist "%DBG_MYSQL_PREBUILT%" goto mysql_src_ok
echo.
echo NOTE: libmysql.lib not in prebuilt Debug dir. Running setup-mysql-win.bat ...
echo.
call setup-mysql-win.bat
if errorlevel 1 ( echo MySQL setup failed. Install MySQL via Scoop: scoop install mysql & exit /b 1 )
:mysql_src_ok

set "DBG_PG_LIB=%DEBUG_LIB_DIR%\libpq.lib"
if exist "%DBG_PG_LIB%" goto pg_src_ok
echo.
echo NOTE: libpq.lib not found. Running setup-pgsql-win.bat ...
echo.
call setup-pgsql-win.bat
if errorlevel 1 ( echo PostgreSQL setup failed. Install PostgreSQL via Scoop: scoop install postgresql & exit /b 1 )
:pg_src_ok

:: ---- Mirror the entire Debug prebuilt lib directory to Release ----
:: The Release prebuilt dir ships with 9-byte stub files; we overwrite
:: them unconditionally so the Release linker gets the real libraries.
echo Mirroring prebuilt libs Debug -> Release ...
for %%F in ("%PREBUILT_DBG%\*.lib") do (
    copy /Y "%%F" "%PREBUILT_REL%\%%~nxF" > nul
)

:: ---- Also mirror to Release\lib for development.vcxproj ----
copy /Y "%DEBUG_LIB_DIR%\libmysql.lib" "%RELEASE_LIB_DIR%\libmysql.lib" > nul 2>nul
copy /Y "%DBG_PG_LIB%"                 "%RELEASE_LIB_DIR%\libpq.lib"    > nul 2>nul

:: ----------------------------------------------------------
:: Locate MSBuild
:: ----------------------------------------------------------
set "FIND_PS1=%TEMP%\hxt_find_msbuild.ps1"
echo $pf = [System.Environment]::GetEnvironmentVariable('ProgramFiles(x86)')> "%FIND_PS1%"
echo $vs = "$pf\Microsoft Visual Studio\Installer\vswhere.exe">> "%FIND_PS1%"
echo if (Test-Path $vs) { ^& $vs -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' ^| Select-Object -First 1 }>> "%FIND_PS1%"
for /f "tokens=*" %%i in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%FIND_PS1%"') do set "MSBUILD=%%i"
del "%FIND_PS1%" 2>nul
if not defined MSBUILD (
    echo ERROR: MSBuild.exe not found. Install Visual Studio 2019+ with C++ workload.
    exit /b 1
)
echo Using MSBuild: %MSBUILD%

:: ----------------------------------------------------------
:: Platform toolset override.
:: v142 = VS 2019, v143 = VS 2022, v144 = VS 2025.
:: Adjust if the build fails with MSB8020 (toolset not found).
:: ----------------------------------------------------------
set "TOOLSET=/p:PlatformToolset=v142"

echo Build started: %DATE% %TIME%
echo Build started: %DATE% %TIME% > "%LOGFILE%"
echo. >> "%LOGFILE%"

:: ----------------------------------------------------------
:: Regenerate revbuild.h from the version file.
::
:: All revbuild.h copies in the build tree are regenerated here
:: before any engine compilation so the version string embedded
:: in the binary always matches the top-level 'version' file.
:: Without this step, MSBuild's tlog may keep a stale revbuild.h
:: (e.g. the original 9.7.0-dp-1 values) if the version file was
:: updated without a full rebuild.
:: ----------------------------------------------------------
echo Regenerating revbuild.h from version file ...
set "REVBUILD_SHARED_DBG=%~dp0build-win-x86_64\hyperxtalk\engine\Debug\x64\obj\shared_intermediate"
set "REVBUILD_SHARED_REL=%~dp0build-win-x86_64\hyperxtalk\engine\Release\x64\obj\shared_intermediate"
set "REVBUILD_GLOBAL_DBG=%~dp0build-win-x86_64\hyperxtalk\Debug\obj\global_intermediate"
set "REVBUILD_GLOBAL_REL=%~dp0build-win-x86_64\hyperxtalk\Release\obj\global_intermediate"
set "REVBUILD_SRC=%~dp0engine\obj\global_intermediate"
if not exist "%REVBUILD_SHARED_DBG%\include" mkdir "%REVBUILD_SHARED_DBG%\include"
if not exist "%REVBUILD_SHARED_REL%\include" mkdir "%REVBUILD_SHARED_REL%\include"
if not exist "%REVBUILD_GLOBAL_DBG%\include"  mkdir "%REVBUILD_GLOBAL_DBG%\include"
if not exist "%REVBUILD_GLOBAL_REL%\include"  mkdir "%REVBUILD_GLOBAL_REL%\include"
if not exist "%REVBUILD_SRC%\include"          mkdir "%REVBUILD_SRC%\include"
python "%~dp0util\encode_version.py" "-66adce28fb01673e536b0a6a970155bec7ff00ad" "%~dp0engine" "%REVBUILD_SHARED_DBG%"
if errorlevel 1 ( echo ERROR: encode_version.py failed & exit /b 1 )
copy /Y "%REVBUILD_SHARED_DBG%\include\revbuild.h" "%REVBUILD_SHARED_REL%\include\revbuild.h" > nul
copy /Y "%REVBUILD_SHARED_DBG%\include\revbuild.h" "%REVBUILD_GLOBAL_DBG%\include\revbuild.h"  > nul
copy /Y "%REVBUILD_SHARED_DBG%\include\revbuild.h" "%REVBUILD_GLOBAL_REL%\include\revbuild.h"  > nul
copy /Y "%REVBUILD_SHARED_DBG%\include\revbuild.h" "%REVBUILD_SRC%\include\revbuild.h"          > nul
echo revbuild.h OK.

echo Building libCore (Release) ...
echo Building libCore ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBCORE%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBCORE BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libCore OK.

echo.
echo Building libExternal (Release) ...
echo Building libExternal ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBEXTERNAL%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBEXTERNAL BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libExternal OK.

echo.
echo Building libExternal-symbol-exports (Release) ...
echo Building libExternal-symbol-exports ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBEXTERNAL_EXPORTS%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBEXTERNAL-SYMBOL-EXPORTS BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libExternal-symbol-exports OK.

echo.
:: ----------------------------------------------------------
:: libbrowser — build the real WebView2 backend (Release).
::
:: libbrowser.vcxproj compiles libbrowser_webview2.cpp and
:: libbrowser_webview2_win.cpp against the bundled WebView2 SDK.
:: WebView2LoaderStatic.lib is linked via a #pragma comment in
:: libbrowser_webview2.cpp; we bootstrap it into Release\lib\
:: here (the WebView2 copy step above also covers it).
:: ----------------------------------------------------------
echo Building libbrowser (WebView2, Release) ...
echo Building libbrowser (WebView2) ... >> "%LOGFILE%"
if not exist "%RELEASE_LIB_DIR%" mkdir "%RELEASE_LIB_DIR%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_BROWSER% /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBBROWSER BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libbrowser OK.

echo.
:: ----------------------------------------------------------
:: Build libopenssl_stubs.lib for x64 Release.
::
:: dbmysql, dbpostgresql and others link against libopenssl_stubs
:: to weakly reference OpenSSL symbols — must come BEFORE those.
:: ----------------------------------------------------------
echo Building libopenssl_stubs (Release x64) ...
echo Building libopenssl_stubs ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_OPENSSL_STUBS% /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo
if errorlevel 1 ( echo LIBOPENSSL_STUBS BUILD FAILED. & exit /b 1 )
if not exist "%RELEASE_LIB_DIR%\libopenssl_stubs.lib" ( echo ERROR: libopenssl_stubs.lib not found in %RELEASE_LIB_DIR% & exit /b 1 )
echo libopenssl_stubs OK.

echo.
echo Building dbmysql (Release) ...
echo Building dbmysql ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_DBMYSQL% /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo
if errorlevel 1 ( echo DBMYSQL BUILD FAILED. & exit /b 1 )
echo dbmysql OK.

echo.
echo Building dbodbc (Release) ...
echo Building dbodbc ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_DBODBC%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo DBODBC BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo dbodbc OK.

echo.
echo Building dbpostgresql (Release) ...
echo Building dbpostgresql ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_DBPOSTGRESQL% /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\ /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo DBPOSTGRESQL BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo dbpostgresql OK.

echo.
:: ----------------------------------------------------------
:: Build libsqlite.lib for x64 Release.
::
:: The bootstrapped Debug copy in Release\lib was compiled with
:: /MTd and _ITERATOR_DEBUG_LEVEL=2 (debug STL), which causes
:: LNK2038 ABI-mismatch errors and an unresolved _CrtDbgReport
:: when linking the Release dbsqlite.dll.  Building the Release
:: version here overwrites that copy before dbsqlite links.
:: ----------------------------------------------------------
echo Building libsqlite (Release x64) ...
echo Building libsqlite ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSQLITE% /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSQLITE BUILD FAILED. See %LOGFILE% & exit /b 1 )
if not exist "%RELEASE_LIB_DIR%\libsqlite.lib" ( echo ERROR: libsqlite.lib not found in %RELEASE_LIB_DIR% & exit /b 1 )
echo libsqlite OK.

echo.
echo Building dbsqlite (Release) ...
echo Building dbsqlite ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_DBSQLITE% /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\ /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo DBSQLITE BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo dbsqlite OK.

echo.
echo Building libffi (Release) ...
echo Building libffi ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBFFI% /t:Rebuild "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBFFI BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libffi OK.

echo.
echo Building libFoundation (Release) ...
echo Building libFoundation ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBFOUNDATION% /t:Rebuild  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBFOUNDATION BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libFoundation OK.

echo.
echo Building libScript (Release) ...
echo Building libScript ... >> "%LOGFILE%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSCRIPT% /t:Rebuild  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSCRIPT BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libScript OK.

echo.
:: ----------------------------------------------------------
:: Build thirdparty libs in Release.
::
:: These must be Release-compiled to avoid pulling in debug-CRT
:: symbols (_CrtDbgReport from Skia, _chvalidator from libpcre)
:: and HyperXTalk debug macros (__MCAssert etc. from libGraphics)
:: when linking the Release engine.
::
:: Build order:
::   libz first (libpng, libcairo, libzip depend on it)
::   image libs (libgif, libpng, libjpeg) before libcairo/libGraphics
::   libskia variants before libGraphics
::   libGraphics last (depends on all of the above)
:: ----------------------------------------------------------
echo Building libz (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBZ%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBZ BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libz OK.

echo Building libgif (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBGIF%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBGIF BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libgif OK.

echo Building libpng (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBPNG%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBPNG BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libpng OK.

echo Building libjpeg (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBJPEG%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBJPEG BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libjpeg OK.

echo Building libpcre (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBPCRE%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBPCRE BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libpcre OK.

echo Building libcairo (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBCAIRO%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBCAIRO BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libcairo OK.

:: libzip: w32support.cpp was removed from the source tree and is excluded
:: in libzip.vcxproj.  Pass SolutionDir explicitly so the lib output lands
:: in Release\lib\libzip.lib (matching where external-revzip.vcxproj looks).
echo Building libzip (Release) ...
echo Building libzip ... >> "%LOGFILE%"
:: /t:Rebuild ensures obj files are recompiled from scratch so that changes to
:: vendored headers like thirdparty/libzip/src/config.h are always picked up.
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBZIP% /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBZIP BUILD FAILED. See %LOGFILE% & exit /b 1 )
if not exist "%RELEASE_LIB_DIR%\libzip.lib" ( echo ERROR: libzip.lib not found in %RELEASE_LIB_DIR% & exit /b 1 )
echo libzip OK.

echo Building libskia (Release) -- 530 source files, this will take a few minutes ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSKIA%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSKIA BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libskia OK.

echo Building libskia_opt variants (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSKIA_NONE%   "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSKIA_OPT_NONE BUILD FAILED. See %LOGFILE% & exit /b 1 )
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSKIA_ARM%    "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSKIA_OPT_ARM BUILD FAILED. See %LOGFILE% & exit /b 1 )
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSKIA_SSE2%   "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSKIA_OPT_SSE2 BUILD FAILED. See %LOGFILE% & exit /b 1 )
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSKIA_SSE3%   "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSKIA_OPT_SSE3 BUILD FAILED. See %LOGFILE% & exit /b 1 )
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSKIA_SSE41%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSKIA_OPT_SSE41 BUILD FAILED. See %LOGFILE% & exit /b 1 )
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSKIA_SSE42%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSKIA_OPT_SSE42 BUILD FAILED. See %LOGFILE% & exit /b 1 )
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSKIA_AVX%    "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSKIA_OPT_AVX BUILD FAILED. See %LOGFILE% & exit /b 1 )
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBSKIA_HSW%    "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBSKIA_OPT_HSW BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libskia_opt variants OK.

echo Building libGraphics (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBGRAPHICS%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo LIBGRAPHICS BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo libGraphics OK.

echo Building stdscript (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_STDSCRIPT%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo STDSCRIPT BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo stdscript OK.

echo.
:: ----------------------------------------------------------
:: Bootstrap lc-compile.exe into the Release output dir.
::
:: lc-compile is a self-hosting LCB compiler whose toolchain
:: (stage2, gentle, reflex targets) only exists in the Debug
:: output.  Building the full Release toolchain requires those
:: Debug helpers.  For the Release build we copy the Debug
:: lc-compile.exe to the Release dir; the compiled .lcm bytecode
:: it produces is identical regardless of the host's optimisation
:: level.  The copied exe is also the one shipped in the installer.
:: ----------------------------------------------------------
set "DBG_DIR=%~dp0build-win-x86_64\hyperxtalk\Debug"
set "LC_COMPILE_DBG=%DBG_DIR%\lc-compile.exe"
set "LC_COMPILE_REL=%OUTDIR%\lc-compile.exe"
if not exist "%LC_COMPILE_DBG%" (
    echo ERROR: lc-compile.exe not found in Debug output.
    echo        Run the Debug build ^(build-engine-x64.bat^) first to produce it.
    exit /b 1
)
:: ----------------------------------------------------------
:: Bootstrap missing Release\lib entries from Debug\lib.
::
:: Many thirdparty and toolchain static libs (libgif, libpng,
:: libjpeg, libskia variants, libz, libpcre, libcairo, libzip,
:: ICU, OpenSSL, grts, lc-compile-lib-target, libcompile-target,
:: libGraphics, stdscript …) are only available in Debug\lib.
:: They are built from source by vcxproj files whose full Release
:: build chains (thirdparty, toolchain) are not in this script.
::
:: These are prebuilt data/utility libs whose Debug vs Release
:: difference is optimisation level only (no ABI difference).
:: We copy them once at bootstrap; any lib already built for
:: Release by this script is NOT overwritten.
:: ----------------------------------------------------------
echo Bootstrapping missing libs: Debug\lib -> Release\lib ...
if not exist "%RELEASE_LIB_DIR%" mkdir "%RELEASE_LIB_DIR%"
for %%F in ("%DEBUG_LIB_DIR%\*.lib") do (
    if not exist "%RELEASE_LIB_DIR%\%%~nxF" (
        copy /Y "%%F" "%RELEASE_LIB_DIR%\%%~nxF" > nul
    )
)
echo Lib bootstrap OK.

:: Also mirror ICU, OpenSSL, and curl prebuilt lib dirs (same stub issue as Thirdparty)
set "ICU_DBG=%~dp0prebuilt\unpacked\icu\x86_64-win32-v142_static_Debug\lib"
set "ICU_REL=%~dp0prebuilt\unpacked\icu\x86_64-win32-v142_static_Release\lib"
set "ICU_REL_BIN=%~dp0prebuilt\unpacked\icu\x86_64-win32-v142_static_Release\bin"
set "SSL_DBG=%~dp0prebuilt\unpacked\openssl3\x86_64-win32-v142_static_Debug\lib"
set "SSL_REL=%~dp0prebuilt\unpacked\openssl3\x86_64-win32-v142_static_Release\lib"
set "CURL_DBG=%~dp0prebuilt\unpacked\curl\x86_64-win32-v142_static_Debug\lib"
set "CURL_REL=%~dp0prebuilt\unpacked\curl\x86_64-win32-v142_static_Release\lib"
if not exist "%ICU_REL%"   mkdir "%ICU_REL%"
if not exist "%SSL_REL%"   mkdir "%SSL_REL%"
if not exist "%CURL_REL%"  mkdir "%CURL_REL%"
for %%F in ("%ICU_DBG%\*.lib")   do ( copy /Y "%%F" "%ICU_REL%\%%~nxF"   > nul )
for %%F in ("%SSL_DBG%\*.lib")   do ( copy /Y "%%F" "%SSL_REL%\%%~nxF"   > nul )
for %%F in ("%CURL_DBG%\*.lib")  do ( copy /Y "%%F" "%CURL_REL%\%%~nxF"  > nul )
echo ICU + OpenSSL + curl prebuilt lib bootstrap OK.

:: WebView2 static loader lib — copy into Release\lib so the Release linker finds
:: it.  We use WebView2LoaderStatic.lib (not WebView2Loader.dll.lib) so that
:: neither the IDE nor compiled standalones carry a hard load-time DLL dependency.
:: WebView2Loader.dll is therefore NOT needed in the output directory.
set "WV2_LIB=%~dp0packages\Microsoft.Web.WebView2.1.0.3912.50\build\native\x64\WebView2LoaderStatic.lib"
if exist "%WV2_LIB%" (
    copy /Y "%WV2_LIB%" "%RELEASE_LIB_DIR%\WebView2LoaderStatic.lib" > nul
    echo WebView2 bootstrap OK.
) else (
    echo WARNING: WebView2LoaderStatic.lib not found at %WV2_LIB%
)

echo Bootstrapping lc-compile.exe + ICU DLLs ...
copy /Y "%LC_COMPILE_DBG%"          "%LC_COMPILE_REL%"            > nul
:: ICU DLLs are not produced by the Debug engine build; copy directly from the
:: prebuilt unpacked directory so server-community.exe can find them at runtime.
:: ICU DLLs — lc-compile.exe links libScript → libFoundation → ICU and
:: therefore needs the ICU data DLLs at runtime.  The prebuilt ICU is a
:: static-only build with no DLLs; fall back to Debug\ (which got them from
:: ide\Runtime\Windows\x86-64\ during build-engine-x64.bat) or directly to
:: ide\Runtime\Windows\x86-64\ as a second fallback.
copy /Y "%ICU_REL_BIN%\icudt58.dll" "%OUTDIR%\icudt58.dll"        > nul 2>nul
copy /Y "%ICU_REL_BIN%\icuin58.dll" "%OUTDIR%\icuin58.dll"        > nul 2>nul
copy /Y "%ICU_REL_BIN%\icutu58.dll" "%OUTDIR%\icutu58.dll"        > nul 2>nul
copy /Y "%ICU_REL_BIN%\icuuc58.dll" "%OUTDIR%\icuuc58.dll"        > nul 2>nul
if not exist "%OUTDIR%\icuuc58.dll" (
    set "ICU_RT_SRC=%~dp0ide\Runtime\Windows\x86-64"
    for %%F in (icudt58.dll icuin58.dll icutu58.dll icuuc58.dll) do (
        if exist "%DBG_DIR%\%%F" (
            copy /Y "%DBG_DIR%\%%F" "%OUTDIR%\%%F" > nul
        ) else if exist "%ICU_RT_SRC%\%%F" (
            copy /Y "%ICU_RT_SRC%\%%F" "%OUTDIR%\%%F" > nul
        )
    )
    if not exist "%OUTDIR%\icuuc58.dll" (
        echo WARNING: ICU DLLs not found in Debug or Runtime dir.
        echo          lc-compile.exe may fail with STATUS_DLL_NOT_FOUND.
    ) else (
        echo ICU DLLs copied to Release from runtime/Debug fallback.
    )
)
copy /Y "%DBG_DIR%\libcrypto-3-x64.dll" "%OUTDIR%\libcrypto-3-x64.dll" > nul 2>nul
copy /Y "%DBG_DIR%\libssl-3-x64.dll"    "%OUTDIR%\libssl-3-x64.dll"    > nul 2>nul
:: libmysql.dll — runtime DLL required by dbmysql.dll (libmysql.lib is an import lib).
:: setup-mysql-win.bat copies it to Debug\; we mirror it to Release\ so the installer
:: can stage it alongside dbmysql.dll in Externals\Database Drivers\.
copy /Y "%DBG_DIR%\libmysql.dll" "%OUTDIR%\libmysql.dll" > nul 2>nul
if not exist "%OUTDIR%\libmysql.dll" (
    echo WARNING: libmysql.dll not found in Debug output. MySQL driver will not work.
    echo          Run setup-mysql-win.bat to copy libmysql.dll from your Scoop MySQL install.
)
echo lc-compile.exe bootstrap OK.
:: server-community.exe is used by lcs-extensions custom build rules to package
:: script libraries.  It is built by server.vcxproj (Debug only in this script),
:: so bootstrap it from Debug to Release the same way lc-compile.exe is bootstrapped.
copy /Y "%DBG_DIR%\server-community.exe" "%OUTDIR%\server-community.exe" > nul 2>nul
if not exist "%OUTDIR%\server-community.exe" (
    echo ERROR: server-community.exe not found in Debug output.
    echo        Run the Debug build ^(build-engine-x64.bat^) first to produce it.
    exit /b 1
)
echo server-community.exe bootstrap OK.
:: server-revzip.dll and server-revxml.dll are built in Release later in this
:: script (after Release libxml2/libxslt/libzip are ready) and land directly
:: in %OUTDIR% -- no bootstrap copy needed here.

:: ----------------------------------------------------------
:: Bootstrap startupstack.cpp into the Release shared_intermediate.
::
:: startupstack.cpp is generated by the encode_environment_stack
:: project (python compress_data.py → server-community.exe chain).
:: It compresses the IDE environment stack into a C++ byte array.
:: The content is deterministic from the source .livecode files and
:: is identical for Debug and Release configurations.
:: The development.vcxproj ClCompile item references it at:
::   $(obj)\..\shared_intermediate\src\startupstack.cpp
:: where $(obj) = Release\x64\obj\development\
:: so the resolved path is Release\x64\obj\shared_intermediate\src\
:: ----------------------------------------------------------
set "DBG_SHARED_SRC=%~dp0build-win-x86_64\hyperxtalk\engine\Debug\x64\obj\shared_intermediate\src"
set "REL_SHARED_SRC=%~dp0build-win-x86_64\hyperxtalk\engine\Release\x64\obj\shared_intermediate\src"
if not exist "%REL_SHARED_SRC%" mkdir "%REL_SHARED_SRC%"
if exist "%DBG_SHARED_SRC%\startupstack.cpp" (
    copy /Y "%DBG_SHARED_SRC%\startupstack.cpp" "%REL_SHARED_SRC%\startupstack.cpp" > nul
    echo startupstack.cpp bootstrap OK.
) else (
    echo ERROR: startupstack.cpp not found in Debug shared_intermediate.
    echo        Run the Debug build ^(build-engine-x64.bat^) first to produce it.
    exit /b 1
)

echo.
:: ----------------------------------------------------------
:: Build LCB engine modules (Release).
::
:: The engine_lcb_modules custom build action runs lc-compile.exe
:: from $(OutDir) (Release dir).  lc-compile.exe links against
:: libScript → libFoundation → ICU and requires the ICU data DLLs
:: at runtime.  The ICU DLLs were copied to Release\ above (with
:: fallback from Debug\ / ide\Runtime\Windows\x86-64\), so
:: lc-compile.exe should find them and run correctly.
:: ----------------------------------------------------------
echo Building LCB engine modules (Release) ...
echo Building LCB engine modules ... >> "%LOGFILE%"
set "LCB_MOD_LOG=%~dp0build-lcb-modules-release.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_LCB_MODULES%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%LCB_MOD_LOG%" 2>&1
set LCB_MOD_ERR=%ERRORLEVEL%
type "%LCB_MOD_LOG%"
type "%LCB_MOD_LOG%" >> "%LOGFILE%"
if %LCB_MOD_ERR% NEQ 0 (
    echo LCB MODULES BUILD FAILED - full output above.
    exit /b 1
)
echo LCB modules OK.

echo.
echo Building security-community (Release) ...
echo Building security-community ... >> "%LOGFILE%"
set "SECCOM_LOG=%~dp0build-security-community-release.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_SECURITY_COMMUNITY%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%SECCOM_LOG%" 2>&1
set SECCOM_ERR=%ERRORLEVEL%
type "%SECCOM_LOG%"
type "%SECCOM_LOG%" >> "%LOGFILE%"
if %SECCOM_ERR% NEQ 0 ( echo SECURITY-COMMUNITY BUILD FAILED. See %SECCOM_LOG% & exit /b 1 )
echo security-community OK.

echo.
:: ----------------------------------------------------------
:: Build kernel.lib (Release).
:: ----------------------------------------------------------
echo Building kernel (Release) ...
echo Building kernel ... >> "%LOGFILE%"
set "KERNEL_LOG=%~dp0build-kernel-release.log"
set "KERNEL_OUTDIR=%OUTDIR%"
"%MSBUILD%" %TOOLSET% %VCXPROJ_KERNEL% /t:Rebuild /p:Configuration=Release /p:Platform=x64 "/p:OutDir=%KERNEL_OUTDIR%\\" /p:BuildProjectReferences=false /v:minimal /nologo > "%KERNEL_LOG%" 2>&1
set KERNEL_ERR=%ERRORLEVEL%
type "%KERNEL_LOG%"
type "%KERNEL_LOG%" >> "%LOGFILE%"
if %KERNEL_ERR% NEQ 0 ( echo KERNEL BUILD FAILED. See %KERNEL_LOG% & exit /b 1 )
echo kernel OK.

echo.
:: ----------------------------------------------------------
:: Patch kernel.lib with printer base class (MCPrinter) and
:: Windows subclass — same requirement as the Debug build.
:: ----------------------------------------------------------
echo Patching kernel.lib with printer objects (Release) ...
echo Patching kernel.lib with printer objects ... >> "%LOGFILE%"
set "PRINTER_LOG=%~dp0compile-printer-Release.log"
call "%~dp0compile-printer.bat" Release >> "%LOGFILE%" 2>&1
if errorlevel 1 (
    echo PRINTER PATCH FAILED. See %PRINTER_LOG% for details.
    exit /b 1
)
echo Printer patch OK.

echo.
:: ----------------------------------------------------------
:: Build kernel-development.lib (Release).
:: ----------------------------------------------------------
echo Building perfect-target (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_PERFECT%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo PERFECT BUILD FAILED. See %LOGFILE% & exit /b 1 )
echo perfect-target OK.

echo Building kernel-development (Release) ...
echo Building kernel-development ... >> "%LOGFILE%"
set "KDEV_LOG=%~dp0build-kernel-development-release.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_KERNEL_DEVELOPMENT%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%KDEV_LOG%" 2>&1
set KDEV_ERR=%ERRORLEVEL%
type "%KDEV_LOG%"
type "%KDEV_LOG%" >> "%LOGFILE%"
if %KDEV_ERR% NEQ 0 ( echo KERNEL-DEVELOPMENT BUILD FAILED. See %KDEV_LOG% & exit /b 1 )
echo kernel-development OK.

echo.
echo Building libxml2 (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBXML% /t:Rebuild  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo libxml2 build failed. See %LOGFILE% & exit /b 1 )
echo libxml2 OK.

echo Building libxslt (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBXSLT% /t:Rebuild  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo libxslt build failed. See %LOGFILE% & exit /b 1 )
echo libxslt OK.

echo.
:: ----------------------------------------------------------
:: Build server-revxml.dll and server-revzip.dll (Release).
:: These must be built AFTER Release libxml2/libxslt/libzip are ready
:: and BEFORE the lcs-extensions step which loads them via server-community.exe.
:: They land directly in Release\ (matching server-community.exe's location)
:: so the bootstrap copy at the top of this script can be retired.
:: Using Release configuration avoids CRT mismatch crashes when
:: server-community.exe (Release) loads these DLLs via __EnsureExternal.
:: ----------------------------------------------------------
echo Building server-revxml.dll (Release -- required by lcs-extensions) ...
echo Building server-revxml.dll ... >> "%LOGFILE%"
set "SRVXML_LOG=%~dp0build-server-revxml.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVXML_SERVER% /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo > "%SRVXML_LOG%" 2>&1
set SRVXML_ERR=%ERRORLEVEL%
type "%SRVXML_LOG%"
type "%SRVXML_LOG%" >> "%LOGFILE%"
if %SRVXML_ERR% NEQ 0 (
    echo WARNING: server-revxml.dll Release build failed. Extension packaging may fail.
) else (
    echo server-revxml.dll OK.
)

echo Building server-revzip.dll (Release -- required by lcs-extensions) ...
echo Building server-revzip.dll ... >> "%LOGFILE%"
set "SRVZIP_LOG=%~dp0build-server-revzip.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVZIP_SERVER% /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo > "%SRVZIP_LOG%" 2>&1
set SRVZIP_ERR=%ERRORLEVEL%
type "%SRVZIP_LOG%"
type "%SRVZIP_LOG%" >> "%LOGFILE%"
if %SRVZIP_ERR% NEQ 0 (
    echo WARNING: server-revzip.dll Release build failed. Extension packaging may fail.
) else (
    echo server-revzip.dll OK.
)

echo.

:: ----------------------------------------------------------
:: Generate revsecurity.lib import library for Release.
:: The engine linker needs this import lib at link time;
:: the actual revsecurity.dll is built later.
::
:: lib.exe is required for the /def: import-lib generation step.
:: Locate it by walking up from MSBuild.exe to the VS root then
:: finding the x64-hosted x64 compiler tools directory.
:: ----------------------------------------------------------
set "FIND_CLEXE_PS1_REL=%TEMP%\hxt_find_clexe_rel.ps1"
echo $p = '%MSBUILD%'                                                    > "%FIND_CLEXE_PS1_REL%"
echo $p = Split-Path -Parent $p                                         >> "%FIND_CLEXE_PS1_REL%"
echo $p = Split-Path -Parent $p                                         >> "%FIND_CLEXE_PS1_REL%"
echo $p = Split-Path -Parent $p                                         >> "%FIND_CLEXE_PS1_REL%"
echo $p = Split-Path -Parent $p                                         >> "%FIND_CLEXE_PS1_REL%"
echo $vctools = Join-Path $p 'VC\Tools\MSVC'                            >> "%FIND_CLEXE_PS1_REL%"
echo $cl = Get-ChildItem $vctools -Recurse -Filter cl.exe -ErrorAction SilentlyContinue >> "%FIND_CLEXE_PS1_REL%"
echo $cl = $cl ^| Where-Object { $_.DirectoryName -like '*Hostx64\x64' } ^| Select-Object -First 1 >> "%FIND_CLEXE_PS1_REL%"
echo if ($cl) { $cl.FullName }                                          >> "%FIND_CLEXE_PS1_REL%"
for /f "tokens=*" %%i in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%FIND_CLEXE_PS1_REL%"') do set "CL_EXE=%%i"
del "%FIND_CLEXE_PS1_REL%" 2>nul
if not defined CL_EXE (
    echo ERROR: cl.exe not found. Install Visual Studio C++ tools.
    exit /b 1
)
set "LIB_EXE=%CL_EXE:cl.exe=lib.exe%"

echo Generating revsecurity.def (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_OPENSSL_SYMLIST% /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo ERROR: libopenssl_symbol_list_win failed. & exit /b 1 )
set "REVSEC_DEF=%~dp0build-win-x86_64\hyperxtalk\Release\obj\global_intermediate\src\revsecurity.def"
if not exist "%REVSEC_DEF%" ( echo ERROR: revsecurity.def was not generated at %REVSEC_DEF% & exit /b 1 )
echo Generating revsecurity.lib (Release) ...
"%LIB_EXE%" /def:"%REVSEC_DEF%" /out:"%OUTDIR%\revsecurity.lib" /machine:x64 >> "%LOGFILE%" 2>&1
if errorlevel 1 ( echo ERROR: lib.exe failed to create revsecurity.lib & exit /b 1 )
echo revsecurity.lib OK.

echo Building engine (Release) ...

set "EXE=%OUTDIR%\HyperXTalk.exe"
set "ENGINE_LOG=%~dp0build-engine-step-release.log"
set "LINK_TLOG=build-win-x86_64\hyperxtalk\engine\Release\x64\obj\development\development.tlog\link.write.1.tlog"

if not exist "%EXE%" (
    if exist "%LINK_TLOG%" del /F /Q "%LINK_TLOG%"
)

"%MSBUILD%" %TOOLSET% %VCXPROJ_ENGINE%  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%ENGINE_LOG%" 2>&1
set BUILD_ERR=%ERRORLEVEL%
type "%ENGINE_LOG%"
type "%ENGINE_LOG%" >> "%LOGFILE%"

if %BUILD_ERR% NEQ 0 ( echo. & echo ENGINE BUILD FAILED. & exit /b 1 )
if not exist "%EXE%" ( echo. & echo ENGINE BUILD FAILED - HyperXTalk.exe was not produced. & exit /b 1 )
echo Engine built: %EXE%

echo.
echo Building kernel-standalone (Release) ...
echo Building kernel-standalone ... >> "%LOGFILE%"
set "KSTD_LOG=%~dp0build-kernel-standalone-release.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_KERNEL_STANDALONE% /t:Rebuild  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%KSTD_LOG%" 2>&1
set KSTD_ERR=%ERRORLEVEL%
type "%KSTD_LOG%"
type "%KSTD_LOG%" >> "%LOGFILE%"
if %KSTD_ERR% NEQ 0 ( echo KERNEL-STANDALONE BUILD FAILED. See %KSTD_LOG% & exit /b 1 )
echo kernel-standalone OK.

echo.
echo Building standalone-community.exe (Release) ...
echo Building standalone-community.exe ... >> "%LOGFILE%"
set "STANDALONE_LOG=%~dp0build-standalone-release.log"
set "STANDALONE_EXE=%OUTDIR%\standalone-community.exe"
"%MSBUILD%" %TOOLSET% %VCXPROJ_STANDALONE% /t:Rebuild  "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo > "%STANDALONE_LOG%" 2>&1
set STANDALONE_ERR=%ERRORLEVEL%
type "%STANDALONE_LOG%"
type "%STANDALONE_LOG%" >> "%LOGFILE%"
if %STANDALONE_ERR% NEQ 0 goto standalone_failed_rel
if not exist "%STANDALONE_EXE%" goto standalone_failed_rel
echo standalone-community.exe OK.
:: Keep the development-mode Runtime template in sync with the freshly linked binary.
:: The IDE (run directly from Release\, not installed) reads the standalone template from
:: ide\Runtime\Windows\x86-64\Standalone.  Without this copy, that file stays stale and
:: any compiled standalone inherits the old binary's DLL import table.
copy /Y "%STANDALONE_EXE%" "%~dp0ide\Runtime\Windows\x86-64\Standalone" > nul
echo Runtime\Standalone template updated.
goto standalone_done_rel
:standalone_failed_rel
set "STANDALONE_ERRORS=%~dp0build-standalone-errors-release.log"
findstr /v /r "LNK4099\|LNK4075" "%STANDALONE_LOG%" > "%STANDALONE_ERRORS%"
echo STANDALONE BUILD FAILED. Errors:
type "%STANDALONE_ERRORS%"
echo ERROR: standalone-community.exe missing or build failed.
exit /b 1
:standalone_done_rel

echo.
echo Building revxml (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVXML% /t:Rebuild "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 ( echo revxml build failed. See %LOGFILE% & exit /b 1 )
echo revxml OK.

echo.
:: ----------------------------------------------------------
:: External DLLs: revbrowser, revdb, revsecurity.
::
:: These have deep ProjectReference chains (CEF, OpenSSL wrappers)
:: that don't fully resolve with BuildProjectReferences=false.
:: Strategy: attempt a Release build with /p:OutDir forced to the
:: Release output dir; if it fails or produces no DLL, fall back
:: to the Debug output.  The DLLs are thin wrappers whose Debug vs
:: Release difference is optimisation only — ABI is identical.
:: ----------------------------------------------------------
echo Building revbrowser (Release) ...
set "REVBROWSER_REL_LOG=%~dp0build-revbrowser-release.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVBROWSER% /p:Configuration=Release /p:Platform=x64 "/p:OutDir=%OUTDIR%\\" /p:BuildProjectReferences=false /v:minimal /nologo > "%REVBROWSER_REL_LOG%" 2>&1
set REVBROWSER_REL_ERR=%ERRORLEVEL%
type "%REVBROWSER_REL_LOG%"
type "%REVBROWSER_REL_LOG%" >> "%LOGFILE%"
if %REVBROWSER_REL_ERR% NEQ 0 goto revbrowser_fallback
if not exist "%OUTDIR%\revbrowser.dll" goto revbrowser_fallback
echo revbrowser Release OK.
goto revbrowser_done
:revbrowser_fallback
if not exist "%DBG_DIR%\revbrowser.dll" (
    echo WARNING: revbrowser.dll not available ^(CEF removed^) -- skipping.
    goto revbrowser_done
)
copy /Y "%DBG_DIR%\revbrowser.dll" "%OUTDIR%\revbrowser.dll" > nul
echo revbrowser: using Debug bootstrap.
:revbrowser_done

echo.
echo Building revdb (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVDB% /p:Configuration=Release /p:Platform=x64 "/p:OutDir=%OUTDIR%\\" /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 goto revdb_fallback
if not exist "%OUTDIR%\revdb.dll" goto revdb_fallback
echo revdb Release OK.
goto revdb_done
:revdb_fallback
if not exist "%DBG_DIR%\revdb.dll" ( echo ERROR: revdb.dll missing from both Release build and Debug output. & exit /b 1 )
copy /Y "%DBG_DIR%\revdb.dll" "%OUTDIR%\revdb.dll" > nul
echo revdb: using Debug bootstrap.
:revdb_done

echo.
echo Building revsecurity (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVSECURITY% /p:Configuration=Release /p:Platform=x64 "/p:OutDir=%OUTDIR%\\" /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 goto revsecurity_fallback
if not exist "%OUTDIR%\revsecurity.dll" goto revsecurity_fallback
echo revsecurity Release OK.
goto revsecurity_done
:revsecurity_fallback
if not exist "%DBG_DIR%\revsecurity.dll" ( echo ERROR: revsecurity.dll missing from both Release build and Debug output. & exit /b 1 )
copy /Y "%DBG_DIR%\revsecurity.dll" "%OUTDIR%\revsecurity.dll" > nul
echo revsecurity: using Debug bootstrap.
:revsecurity_done

echo.
echo Building revzip (Release) ...
set "REVZIP_LOG=%~dp0build-revzip-release.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVZIP% /p:Configuration=Release /p:Platform=x64 "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:BuildProjectReferences=false /v:minimal /nologo > "%REVZIP_LOG%" 2>&1
set REVZIP_ERR=%ERRORLEVEL%
type "%REVZIP_LOG%"
type "%REVZIP_LOG%" >> "%LOGFILE%"
if %REVZIP_ERR% NEQ 0 goto revzip_fallback
if not exist "%OUTDIR%\revzip.dll" goto revzip_fallback
echo revzip Release OK.
goto revzip_done
:revzip_fallback
echo revzip Release build failed -- attempting Debug bootstrap build ...
:: Build libzip Debug so Debug\lib\libzip.lib exists for revzip Debug link.
:: /t:Rebuild ensures fresh compilation picks up config.h changes.
"%MSBUILD%" %TOOLSET% %VCXPROJ_LIBZIP% /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo >> "%LOGFILE%" 2>&1
:: Build revzip Debug into DBG_DIR so the copy below succeeds.
set "REVZIP_DBG_LOG=%~dp0build-revzip-debug.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVZIP% /p:Configuration=Debug /p:Platform=x64 "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:BuildProjectReferences=false /v:minimal /nologo > "%REVZIP_DBG_LOG%" 2>&1
type "%REVZIP_DBG_LOG%"
type "%REVZIP_DBG_LOG%" >> "%LOGFILE%"
if not exist "%DBG_DIR%\revzip.dll" ( echo ERROR: revzip.dll missing from both Release build and Debug output. & exit /b 1 )
copy /Y "%DBG_DIR%\revzip.dll" "%OUTDIR%\revzip.dll" > nul
echo revzip: using Debug bootstrap.
:revzip_done

echo.
echo Building revspeech (Release) ...
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVSPEECH% /p:Configuration=Release /p:Platform=x64 "/p:OutDir=%OUTDIR%\\" /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 goto revspeech_fallback
if not exist "%OUTDIR%\revspeech.dll" goto revspeech_fallback
echo revspeech Release OK.
goto revspeech_done
:revspeech_fallback
if not exist "%DBG_DIR%\revspeech.dll" ( echo ERROR: revspeech.dll missing from both Release build and Debug output. & exit /b 1 )
copy /Y "%DBG_DIR%\revspeech.dll" "%OUTDIR%\revspeech.dll" > nul
echo revspeech: using Debug bootstrap.
:revspeech_done

echo.
echo Building revpdfprinter (Release) ...
set "REVPDF_LOG=%~dp0build-revpdfprinter-release.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVPDFPRINTER% /p:Configuration=Release /p:Platform=x64 "/p:OutDir=%OUTDIR%\\" /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo > "%REVPDF_LOG%" 2>&1
set REVPDF_ERR=%ERRORLEVEL%
type "%REVPDF_LOG%"
type "%REVPDF_LOG%" >> "%LOGFILE%"
if %REVPDF_ERR% NEQ 0 goto revpdf_fallback
if not exist "%OUTDIR%\revpdfprinter.dll" goto revpdf_fallback
echo revpdfprinter Release OK.
goto revpdf_done
:revpdf_fallback
echo revpdfprinter Release build failed -- attempting Debug bootstrap build ...
set "REVPDF_DBG_LOG=%~dp0build-revpdfprinter-debug.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_REVPDFPRINTER% /p:Configuration=Debug /p:Platform=x64 "/p:OutDir=%DBG_DIR%\\" /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo > "%REVPDF_DBG_LOG%" 2>&1
type "%REVPDF_DBG_LOG%"
type "%REVPDF_DBG_LOG%" >> "%LOGFILE%"
if exist "%DBG_DIR%\revpdfprinter.dll" (
    copy /Y "%DBG_DIR%\revpdfprinter.dll" "%OUTDIR%\revpdfprinter.dll" > nul
    echo revpdfprinter: using Debug bootstrap.
) else (
    echo WARNING: revpdfprinter.dll missing from both Release and Debug output. PDF printing will not work.
)
:revpdf_done

echo.
:: ----------------------------------------------------------
:: Build LCB extensions (module libraries + widgets).
::
:: Uses server-community.exe with extension-utils.lc in
:: buildlcbextensions mode.  The script handles dependency
:: ordering, generates manifest.xml in each source directory,
:: compiles .lcb → .lcm via lc-compile, packages each into a
:: zip, and extracts to Release\packaged_extensions\<module-id>\.
::
:: Cross-platform extensions only; Mac/iOS/Android-specific
:: widgets (macbutton, iosbutton, html5button, etc.) are skipped.
::
:: The LCI dir receives new .lci interface files as each library
:: is compiled, allowing later widgets that depend on them to resolve
:: their imports automatically.
::
:: Prerequisites: server-community.exe, server-revzip.dll, and
:: server-revxml.dll must all be present in %OUTDIR%.
:: ----------------------------------------------------------
echo Building LCB extensions (libraries and widgets) ...
echo Building LCB extensions ... >> "%LOGFILE%"

set "LC_COMPILE=%OUTDIR%\lc-compile.exe"
set "LCI_DIR=%OUTDIR%\modules\lci"
set "EXT_LOG=%~dp0build-extensions-release.log"
set "EXT_UTILS=%~dp0extensions\script-libraries\extension-utils\resources\extension-utils.lc"
set "PACKAGED_EXT=%OUTDIR%\packaged_extensions"

if not exist "%LC_COMPILE%" (
    echo ERROR: lc-compile.exe not found in Release output. Build the engine first.
    exit /b 1
)

if not exist "%PACKAGED_EXT%" mkdir "%PACKAGED_EXT%"

:: Build yyjson native DLL for Release (required by json.lcb for JSON import on Windows)
echo Building yyjson native DLL (Release)...
set "VCXPROJ_YYJSON=%~dp0build-win-x86_64\hyperxtalk\extensions\libraries\json\yyjson\yyjson-build.vcxproj"
"%MSBUILD%" %TOOLSET% "%VCXPROJ_YYJSON%" "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo >> "%LOGFILE%" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: yyjson Release build failed -- JSON import extension may not work. See %LOGFILE%
) else (
    if not exist "%PACKAGED_EXT%\com.hyperxtalk.library.json\code\x86_64-win32" mkdir "%PACKAGED_EXT%\com.hyperxtalk.library.json\code\x86_64-win32"
    copy /Y "%OUTDIR%\yyjson.dll" "%PACKAGED_EXT%\com.hyperxtalk.library.json\code\x86_64-win32\yyjson.dll" >> "%LOGFILE%" 2>&1
    echo yyjson.dll OK.
)

:: Bootstrap modules/lci from Debug if Release hasn't produced it yet.
if not exist "%LCI_DIR%" (
    if exist "%DBG_DIR%\modules\lci" (
        xcopy /E /I /Y /Q "%DBG_DIR%\modules\lci" "%LCI_DIR%" > nul
        echo Bootstrapped modules/lci from Debug output.
    )
)

"%OUTDIR%\server-community.exe" "%EXT_UTILS%" buildlcbextensions "%~dp0ide-support\revdocsparser.livecodescript" "%PACKAGED_EXT%" false "%LC_COMPILE%" "%LCI_DIR%" "" "%~dp0extensions\modules\widget-utils\widget-utils.lcb" "%~dp0extensions\modules\scriptitems\scriptitems.lcb" "%~dp0extensions\libraries\canvas\canvas.lcb" "%~dp0extensions\libraries\iconsvg\iconsvg.lcb" "%~dp0extensions\libraries\json\json.lcb" "%~dp0extensions\libraries\objectrepository\objectrepository.lcb" "%~dp0extensions\libraries\ini\ini.lcb" "%~dp0extensions\libraries\timezone\timezone.lcb" "%~dp0extensions\widgets\svgpath\svgpath.lcb" "%~dp0extensions\widgets\clock\clock.lcb" "%~dp0extensions\widgets\graph\graph.lcb" "%~dp0extensions\widgets\header\header.lcb" "%~dp0extensions\widgets\iconpicker\iconpicker.lcb" "%~dp0extensions\widgets\navbar\navbar.lcb" "%~dp0extensions\widgets\paletteactions\paletteactions.lcb" "%~dp0extensions\widgets\segmented\segmented.lcb" "%~dp0extensions\widgets\switchbutton\switchbutton.lcb" "%~dp0extensions\widgets\treeview\treeview.lcb" "%~dp0extensions\widgets\colorswatch\colorswatch.lcb" "%~dp0extensions\widgets\gradientrampeditor\gradientrampeditor.lcb" "%~dp0extensions\widgets\tile\tile.lcb" "%~dp0extensions\widgets\spinner\spinner.lcb" > "%EXT_LOG%" 2>&1
set EXT_ERR=%ERRORLEVEL%
type "%EXT_LOG%"
type "%EXT_LOG%" >> "%LOGFILE%"
if %EXT_ERR% NEQ 0 (
    echo WARNING: LCB extension build had errors. IDE palette may be incomplete. See %EXT_LOG%
) else (
    echo LCB extensions OK.
)

echo.
:: ----------------------------------------------------------
:: Compile browser widget directly with lc-compile (no -Werror,
:: writes module.lcm straight into packaged_extensions).
::
:: The browser widget is compiled separately from the other LCB
:: extensions because the buildlcbextensions pipeline uses -Werror
:: inside extensionPackageBuilderExtension, which can cause the
:: compilation to fail silently, leaving a stale module.lcm in
:: packaged_extensions.  The dedicated lc-compile invocation below
:: matches the approach used in the committed bat and is the proven
:: working method for Windows builds.
:: ----------------------------------------------------------
echo Compiling browser widget (Release) ...
echo Compiling browser widget ... >> "%LOGFILE%"
set "BROWSER_PKG=%PACKAGED_EXT%\com.livecode.widget.browser"
set "BROWSER_LCB=%~dp0extensions\widgets\browser\browser.lcb"
if not exist "%BROWSER_PKG%" mkdir "%BROWSER_PKG%"
if not exist "%BROWSER_PKG%\manifest.xml" (
    copy /Y "%~dp0extensions\widgets\browser\manifest.xml" "%BROWSER_PKG%\manifest.xml" > nul
)
if exist "%BROWSER_LCB%" (
    "%LC_COMPILE%" --modulepath "%LCI_DIR%" --manifest "%BROWSER_PKG%\manifest.xml" --output "%BROWSER_PKG%\module.lcm" "%BROWSER_LCB%" > nul 2>&1
    if exist "%BROWSER_PKG%\module.lcm" (
        echo Browser widget compiled OK.
    ) else (
        echo WARNING: browser widget compilation failed -- skipping.
    )
) else (
    echo WARNING: browser.lcb not found -- skipping browser widget compilation.
)

echo.
:: ----------------------------------------------------------
:: Build lcs-extensions (Loadable Code Script library extensions).
::
:: These are .livecodescript files (oauth2, getopt, mime, etc.)
:: packaged via server-community.exe + extension-utils.lc in
:: buildextension mode.  lcs-extensions.vcxproj drives one
:: server-community.exe invocation per script library file.
:: ----------------------------------------------------------
echo Building lcs-extensions (script libraries) ...
echo Building lcs-extensions ... >> "%LOGFILE%"
set "LCS_LOG=%~dp0build-lcs-extensions-release.log"
"%MSBUILD%" %TOOLSET% %VCXPROJ_LCS_EXTENSIONS% /p:Configuration=Release /p:Platform=x64 /p:BuildProjectReferences=false "/p:SolutionDir=%~dp0build-win-x86_64\hyperxtalk\\" /v:minimal /nologo > "%LCS_LOG%" 2>&1
set LCS_ERR=%ERRORLEVEL%
type "%LCS_LOG%"
type "%LCS_LOG%" >> "%LOGFILE%"
if %LCS_ERR% NEQ 0 (
    echo WARNING: lcs-extensions build had errors. Script library extensions may be missing. See %LCS_LOG%
) else (
    echo lcs-extensions OK.
)

:: ==============================================================
:: Copy VLC runtime DLLs and plugins into the Release output dir
:: so the engine can find them at run-time (and so the installer
:: can package them).
::
:: Layout placed into Release\:
::   libvlc.dll
::   libvlccore.dll
::   vlc-plugins\plugins\<codec dirs…>   ← EnsureVLCInstance() probes this first
::
:: EnsureVLCInstance() probe order (see engine/src/vlc-player.cpp):
::   1. <exedir>\vlc-plugins\plugins   → --plugin-path=<that path>
::   2. <exedir>\vlc-plugins            → --plugin-path=<that path>
::   3. C:\Program Files\VideoLAN\VLC\plugins (fallback, system VLC)
::
:: If VLC is not installed the copy is skipped with a WARNING so
:: the rest of the build succeeds without VLC support.
:: ==============================================================

echo.
echo Copying VLC runtime files ...
echo Copying VLC runtime files ... >> "%LOGFILE%"

set "VLC_SRC=C:\Program Files\VideoLAN\VLC"

if not exist "%VLC_SRC%\libvlc.dll" (
    echo WARNING: VLC not found at "%VLC_SRC%" -- skipping VLC runtime copy.
    echo          Install VLC 3.x to enable video playback in HyperXTalk.
    echo WARNING: VLC not found -- video playback will not work. >> "%LOGFILE%"
    goto vlc_done
)

:: Core DLLs
copy /Y "%VLC_SRC%\libvlc.dll"     "%OUTDIR%\libvlc.dll"     > nul
copy /Y "%VLC_SRC%\libvlccore.dll" "%OUTDIR%\libvlccore.dll" > nul
echo   libvlc.dll, libvlccore.dll copied.

:: MinGW runtime DLLs that libvlccore.dll depends on.
:: VLC 3.x on Windows is compiled with GCC (MinGW-w64), so these are
:: bundled in VLC's install dir but not in the Windows system dirs.
:: Without them libvlccore.dll fails to load and libvlc_new() returns null.
for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%VLC_SRC%\%%D" (
        copy /Y "%VLC_SRC%\%%D" "%OUTDIR%\%%D" > nul
        echo   %%D copied.
    ) else (
        echo   NOTE: %%D not found in VLC dir -- may already be on system PATH.
    )
)

:: plugins tree → Release\vlc-plugins\plugins\
set "VLC_PLUGINS_DST=%OUTDIR%\vlc-plugins\plugins"
if not exist "%VLC_PLUGINS_DST%" mkdir "%VLC_PLUGINS_DST%"

xcopy /E /I /Y /Q "%VLC_SRC%\plugins" "%VLC_PLUGINS_DST%" > nul
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: xcopy of VLC plugins returned error %ERRORLEVEL% -- some plugins may be missing.
    echo WARNING: xcopy VLC plugins error %ERRORLEVEL% >> "%LOGFILE%"
) else (
    echo   VLC plugins copied to vlc-plugins\plugins\.
)

:vlc_done

:: Stage VLC into ide\Runtime\Windows\x86-64\vlc\ so Mac can bundle it
:: when building Windows standalones without a local VLC install.
:: Source is %OUTDIR% (already has libvlc.dll copied there above) so this
:: works regardless of the VLC install path.
if not exist "%OUTDIR%\libvlc.dll" goto vlc_stage_skip
if not exist "%~dp0ide\Runtime\Windows\x86-64\vlc"         mkdir "%~dp0ide\Runtime\Windows\x86-64\vlc"
if not exist "%~dp0ide\Runtime\Windows\x86-64\vlc\plugins" mkdir "%~dp0ide\Runtime\Windows\x86-64\vlc\plugins"
copy /Y "%OUTDIR%\libvlc.dll"     "%~dp0ide\Runtime\Windows\x86-64\vlc\libvlc.dll"     > nul
copy /Y "%OUTDIR%\libvlccore.dll" "%~dp0ide\Runtime\Windows\x86-64\vlc\libvlccore.dll" > nul
for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    if exist "%OUTDIR%\%%D" copy /Y "%OUTDIR%\%%D" "%~dp0ide\Runtime\Windows\x86-64\vlc\%%D" > nul
)
if exist "%OUTDIR%\vlc-plugins\plugins" (
    xcopy /E /I /Y /Q "%OUTDIR%\vlc-plugins\plugins" "%~dp0ide\Runtime\Windows\x86-64\vlc\plugins" > nul
)
echo VLC runtime staged to ide\Runtime\Windows\x86-64\vlc\
echo VLC runtime staged to ide\Runtime\Windows\x86-64\vlc\ >> "%LOGFILE%"
:vlc_stage_skip

:: ----------------------------------------------------------
:: Stage Windows Runtime files into ide\Runtime\Windows\x86-64\
:: so that macOS (and any other platform) can build Windows standalones
:: without needing a separate Windows machine.
:: These files are tracked in git via the .gitignore exemption
:: !ide/Runtime/Windows/**/**.dll
:: ----------------------------------------------------------
echo.
echo Staging Windows Runtime files for cross-platform standalone building ...

set "RT=ide\Runtime\Windows\x86-64"
if not exist "%~dp0%RT%\Support"                  mkdir "%~dp0%RT%\Support"
if not exist "%~dp0%RT%\Externals\Database Drivers" mkdir "%~dp0%RT%\Externals\Database Drivers"

:: Engine (already copied above, but re-copy here for completeness)
copy /Y "%STANDALONE_EXE%" "%~dp0%RT%\Standalone" > nul 2>nul

:: Support DLLs
for %%F in (revsecurity.dll revpdfprinter.dll) do (
    if exist "%OUTDIR%\%%F" (
        copy /Y "%OUTDIR%\%%F" "%~dp0%RT%\Support\%%F" > nul
        echo   %%F staged to %RT%\Support\
    ) else (
        echo   WARNING: %%F not found in %OUTDIR% -- skipping.
    )
)

:: Externals DLLs
for %%F in (revxml.dll revdb.dll revzip.dll revspeech.dll revbrowser.dll) do (
    if exist "%OUTDIR%\%%F" (
        copy /Y "%OUTDIR%\%%F" "%~dp0%RT%\Externals\%%F" > nul
        echo   %%F staged to %RT%\Externals\
    ) else (
        echo   WARNING: %%F not found in %OUTDIR% -- skipping.
    )
)

:: Database driver DLLs
for %%F in (dbmysql.dll dbodbc.dll dbpostgresql.dll dbsqlite.dll) do (
    if exist "%OUTDIR%\%%F" (
        copy /Y "%OUTDIR%\%%F" "%~dp0%RT%\Externals\Database Drivers\%%F" > nul
        echo   %%F staged to %RT%\Externals\Database Drivers\
    ) else (
        echo   WARNING: %%F not found in %OUTDIR% -- skipping.
    )
)

:: libmysql.dll and OpenSSL DLLs (MySQL runtime deps)
for %%F in (libmysql.dll libssl-3-x64.dll libcrypto-3-x64.dll) do (
    if exist "%OUTDIR%\%%F" (
        copy /Y "%OUTDIR%\%%F" "%~dp0%RT%\Externals\Database Drivers