@echo off
setlocal

cd /d "%~dp0"
set BASE=build-win-x86_64\hyperxtalk\thirdparty
set FLAGS=/t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo
set LOGFILE=%~dp0build-thirdparty-x64.log
set FAILED=0

:: ----------------------------------------------------------
:: Locate MSBuild.  Use the one on the PATH when there is one (a Visual
:: Studio developer prompt), otherwise ask vswhere, so the script also runs
:: from an ordinary command prompt.  The lookup goes through a temp PS1 file
:: because cmd mishandles the parentheses in %ProgramFiles(x86)%.
:: ----------------------------------------------------------
set "MSBUILD="
where /q msbuild
if not errorlevel 1 set "MSBUILD=msbuild"
if defined MSBUILD goto :have_msbuild
set "FIND_PS1=%TEMP%\hxt_find_msbuild_tp.ps1"
echo $pf = [System.Environment]::GetEnvironmentVariable('ProgramFiles(x86)')> "%FIND_PS1%"
echo $vs = "$pf\Microsoft Visual Studio\Installer\vswhere.exe">> "%FIND_PS1%"
echo if (Test-Path $vs) { ^& $vs -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' ^| Select-Object -First 1 }>> "%FIND_PS1%"
for /f "tokens=*" %%i in ('powershell -NoProfile -ExecutionPolicy Bypass -File "%FIND_PS1%"') do set "MSBUILD=%%i"
del "%FIND_PS1%" 2>nul
if not defined MSBUILD (
    echo ERROR: MSBuild.exe not found. Install Visual Studio with the C++ workload.
    exit /b 1
)
:have_msbuild
echo Using MSBuild: %MSBUILD%

echo Build started: %DATE% %TIME% > "%LOGFILE%"
echo. >> "%LOGFILE%"

call :build %BASE%\libgif\libgif.vcxproj
call :build %BASE%\libjpeg\libjpeg.vcxproj
call :build %BASE%\libpng\libpng.vcxproj
call :build %BASE%\libwebp\libwebp.vcxproj
call :build %BASE%\libpcre2\libpcre2.vcxproj
call :build %BASE%\libskia\libskia.vcxproj
call :build %BASE%\libskia\libskia_opt_none.vcxproj
call :build %BASE%\libskia\libskia_opt_sse2.vcxproj
call :build %BASE%\libskia\libskia_opt_sse3.vcxproj
call :build %BASE%\libskia\libskia_opt_sse41.vcxproj
call :build %BASE%\libskia\libskia_opt_sse42.vcxproj
call :build %BASE%\libskia\libskia_opt_avx.vcxproj
call :build %BASE%\libskia\libskia_opt_hsw.vcxproj
call :build %BASE%\libskia\libskia_opt_arm.vcxproj
call :build %BASE%\libcairo\libcairo.vcxproj
call :build %BASE%\libxml\libxml.vcxproj
call :build %BASE%\libxslt\libxslt.vcxproj

if not %FAILED%==0 goto :thirdparty_failed

echo.
echo All thirdparty libs built successfully.

:: development.vcxproj compiles sources that build-engine-x64.bat generates
:: (startupstack.cpp, engine_lcb_modules.cpp).  Until that script has run
:: once there is nothing to rebuild here.
if not exist "%~d0\shared_intermediate\src\startupstack.cpp" (
    echo.
    echo Now run build-engine-x64.bat to build the engine.
    goto :eof
)

echo Building HyperXTalk.exe...
echo. >> "%LOGFILE%"
echo === development.vcxproj === >> "%LOGFILE%"
"%MSBUILD%" build-win-x86_64\hyperxtalk\engine\development.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
"%MSBUILD%" build-win-x86_64\hyperxtalk\engine\development.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo
goto :eof

:thirdparty_failed
echo.
echo One or more thirdparty builds failed.
echo.
echo Errors summary:
findstr /i " error " "%LOGFILE%"
echo.
echo Full log saved to: %LOGFILE%
goto :eof

:build
echo.
echo Building %1 ...
echo. >> "%LOGFILE%"
echo === %1 === >> "%LOGFILE%"
"%MSBUILD%" %1 %FLAGS% >> "%LOGFILE%" 2>&1
if errorlevel 1 (
    echo FAILED: %1
    echo FAILED: %1 >> "%LOGFILE%"
    set FAILED=1
) else (
    echo OK: %1
)
goto :eof
