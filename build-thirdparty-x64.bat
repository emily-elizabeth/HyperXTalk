@echo off
setlocal

cd /d "%~dp0"
set BASE=build-win-x86_64\hyperxtalk\thirdparty
set FLAGS=/t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo
set LOGFILE=%~dp0build-thirdparty-x64.log
set FAILED=0

echo Build started: %DATE% %TIME% > "%LOGFILE%"
echo. >> "%LOGFILE%"

call :build %BASE%\libgif\libgif.vcxproj
call :build %BASE%\libjpeg\libjpeg.vcxproj
call :build %BASE%\libpng\libpng.vcxproj
call :build %BASE%\libpcre\libpcre.vcxproj
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

if %FAILED%==0 (
    echo.
    echo All thirdparty libs built successfully. Building HyperXTalk.exe...
    echo. >> "%LOGFILE%"
    echo === development.vcxproj === >> "%LOGFILE%"
    msbuild build-win-x86_64\hyperxtalk\engine\development.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo >> "%LOGFILE%" 2>&1
    msbuild build-win-x86_64\hyperxtalk\engine\development.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:BuildProjectReferences=false /v:minimal /nologo
) else (
    echo.
    echo One or more thirdparty builds failed.
    echo.
    echo Errors summary:
    findstr /i " error " "%LOGFILE%"
    echo.
    echo Full log saved to: %LOGFILE%
)
goto :eof

:build
echo.
echo Building %1 ...
echo. >> "%LOGFILE%"
echo === %1 === >> "%LOGFILE%"
msbuild %1 %FLAGS% >> "%LOGFILE%" 2>&1
if errorlevel 1 (
    echo FAILED: %1
    echo FAILED: %1 >> "%LOGFILE%"
    set FAILED=1
) else (
    echo OK: %1
)
goto :eof
