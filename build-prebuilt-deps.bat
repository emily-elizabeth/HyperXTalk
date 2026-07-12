@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

REM ============================================================================
REM  HyperXTalk Windows x64 Prebuilt Dependency Setup Script
REM  Run this ONCE before building hyperxtalk.sln for the first time.
REM  Must be run from the repository root (where this file lives).
REM  Requires: VS2019 Community (or Build Tools) with v142 toolset.
REM ============================================================================

SET REPO_ROOT=%~dp0
IF "%REPO_ROOT:~-1%"=="\" SET REPO_ROOT=%REPO_ROOT:~0,-1%

ECHO.
ECHO === HyperXTalk Prebuilt Dependency Setup ===
ECHO Repo root: %REPO_ROOT%
ECHO.

REM ---------------------------------------------------------------------------
REM 1. Locate vcvarsall.bat for VS2019 (or any VS with v142)
REM ---------------------------------------------------------------------------
SET VCVARS=

FOR %%P IN (
    "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
) DO (
    IF EXIST %%~P (
        SET VCVARS=%%~P
        GOTO :found_vcvars
    )
)

ECHO ERROR: Could not find VS2019 vcvarsall.bat.
ECHO Please install Visual Studio 2019 Community or Build Tools with C++ workload.
EXIT /B 1

:found_vcvars
ECHO Found VS: %VCVARS%

REM ---------------------------------------------------------------------------
REM 2. Set up x64 MSVC environment
REM ---------------------------------------------------------------------------
ECHO.
ECHO Setting up MSVC x64 environment...
CALL "%VCVARS%" x64
IF %ERRORLEVEL% NEQ 0 (
    ECHO ERROR: vcvarsall.bat failed.
    EXIT /B 1
)
ECHO Done.

REM ---------------------------------------------------------------------------
REM 3. Verify tools are available
REM ---------------------------------------------------------------------------
WHERE lib.exe >NUL 2>&1
IF %ERRORLEVEL% NEQ 0 (
    ECHO ERROR: lib.exe not found. MSVC environment not set up correctly.
    EXIT /B 1
)
WHERE msbuild.exe >NUL 2>&1
IF %ERRORLEVEL% NEQ 0 (
    ECHO ERROR: msbuild.exe not found.
    EXIT /B 1
)
ECHO Tools verified: lib.exe and msbuild.exe found.

REM ---------------------------------------------------------------------------
REM 4. Create ICU import libs from DEF files (Debug)
REM ---------------------------------------------------------------------------
ECHO.
ECHO === Creating ICU x64 import libs (Debug) ===
SET ICU_LIB_DIR=%REPO_ROOT%\prebuilt\unpacked\icu\x86_64-win32-v142_static_Debug\lib

IF NOT EXIST "%ICU_LIB_DIR%" MKDIR "%ICU_LIB_DIR%"

SET ICU_DLLS=icuuc58 icuin58 icutu58 icudt58
SET ICU_LIBS=sicuuc   sicuin   sicutu   sicudt

SET I=0
FOR %%D IN (%ICU_DLLS%) DO (
    SET /A I+=1
    SET DLL=%%D
)

REM Process each DLL/lib pair
CALL :make_icu_lib "%ICU_LIB_DIR%\icuuc58.def" "%ICU_LIB_DIR%\sicuuc.lib"
CALL :make_icu_lib "%ICU_LIB_DIR%\icuin58.def"  "%ICU_LIB_DIR%\sicuin.lib"
CALL :make_icu_lib "%ICU_LIB_DIR%\icutu58.def"  "%ICU_LIB_DIR%\sicutu.lib"
CALL :make_icu_lib "%ICU_LIB_DIR%\icudt58.def"  "%ICU_LIB_DIR%\sicudt.lib"
REM sicuio.lib stays as stub (ICU I/O not used by HyperXTalk engine)

ECHO.
ECHO ICU import libs created.

REM ---------------------------------------------------------------------------
REM 5. Create ICU import libs (Release) - same DLLs
REM ---------------------------------------------------------------------------
ECHO.
ECHO === Creating ICU x64 import libs (Release) ===
SET ICU_LIB_DIR_REL=%REPO_ROOT%\prebuilt\unpacked\icu\x86_64-win32-v142_static_Release\lib

IF NOT EXIST "%ICU_LIB_DIR_REL%" MKDIR "%ICU_LIB_DIR_REL%"

REM Copy DEF files from Debug to Release directory
COPY /Y "%REPO_ROOT%\prebuilt\unpacked\icu\x86_64-win32-v142_static_Debug\lib\*.def" "%ICU_LIB_DIR_REL%\" >NUL

CALL :make_icu_lib "%ICU_LIB_DIR_REL%\icuuc58.def" "%ICU_LIB_DIR_REL%\sicuuc.lib"
CALL :make_icu_lib "%ICU_LIB_DIR_REL%\icuin58.def"  "%ICU_LIB_DIR_REL%\sicuin.lib"
CALL :make_icu_lib "%ICU_LIB_DIR_REL%\icutu58.def"  "%ICU_LIB_DIR_REL%\sicutu.lib"
CALL :make_icu_lib "%ICU_LIB_DIR_REL%\icudt58.def"  "%ICU_LIB_DIR_REL%\sicudt.lib"

REM Copy sicuio.lib stub from Debug
IF EXIST "%ICU_LIB_DIR%\sicuio.lib" (
    COPY /Y "%ICU_LIB_DIR%\sicuio.lib" "%ICU_LIB_DIR_REL%\sicuio.lib" >NUL
)

ECHO ICU Release import libs created.

REM ---------------------------------------------------------------------------
REM 6. Copy ICU DLLs to x86_64 bin directories (for runtime)
REM ---------------------------------------------------------------------------
ECHO.
ECHO === Copying ICU x64 DLLs to x86_64 directories ===
SET ICU_SRC_BIN=%REPO_ROOT%\prebuilt\unpacked\icu\x64-win32-v142_static_Debug\bin
SET ICU_DST_BIN=%REPO_ROOT%\prebuilt\unpacked\icu\x86_64-win32-v142_static_Debug\bin

IF NOT EXIST "%ICU_DST_BIN%" MKDIR "%ICU_DST_BIN%"
FOR %%F IN ("%ICU_SRC_BIN%\*.dll" "%ICU_SRC_BIN%\*.exe") DO (
    IF NOT EXIST "%ICU_DST_BIN%\%%~nxF" (
        COPY /Y "%%F" "%ICU_DST_BIN%\" >NUL
    )
)

REM ---------------------------------------------------------------------------
REM 7. Build thirdparty libraries from source
REM ---------------------------------------------------------------------------
ECHO.
ECHO === Building Thirdparty libraries from source (Debug^|x64) ===
ECHO This compiles: libz, libffi, libgif, libpng, libjpeg, libpcre, libskia, etc.
ECHO This may take 10-30 minutes...
ECHO.

SET SLN=%REPO_ROOT%\build-win-x86_64\hyperxtalk\hyperxtalk.sln

msbuild "%SLN%" /nologo /m /t:thirdparty-prebuilts /p:Configuration=Debug /p:Platform=x64 /fl /flp:LogFile=thirdparty-build-debug.log;Verbosity=normal
IF %ERRORLEVEL% NEQ 0 (
    ECHO.
    ECHO ERROR: Thirdparty build failed. Check thirdparty-build-debug.log for details.
    EXIT /B 1
)
ECHO.
ECHO Thirdparty Debug build succeeded.

REM Verify key outputs
SET TP_OUT=%REPO_ROOT%\build-win-x86_64\hyperxtalk\Debug\lib
IF NOT EXIST "%TP_OUT%\libz.lib" (
    ECHO WARNING: libz.lib not found in %TP_OUT%
    ECHO The build may not have placed libs where expected.
) ELSE (
    ECHO Key libs verified in %TP_OUT%
)

REM ---------------------------------------------------------------------------
REM 8. Copy ICU DLLs to build output directory (for running the exe)
REM ---------------------------------------------------------------------------
ECHO.
ECHO === Copying ICU DLLs to build output directory ===
SET DBG_OUT=%REPO_ROOT%\build-win-x86_64\hyperxtalk\Debug
IF NOT EXIST "%DBG_OUT%" MKDIR "%DBG_OUT%"

FOR %%F IN ("%ICU_SRC_BIN%\*.dll") DO (
    COPY /Y "%%F" "%DBG_OUT%\" >NUL
    ECHO   Copied %%~nxF
)

REM ---------------------------------------------------------------------------
REM Done
REM ---------------------------------------------------------------------------
ECHO.
ECHO ============================================================================
ECHO Setup complete! You can now build HyperXTalk.exe:
ECHO.
ECHO   msbuild "%SLN%" /nologo /m /t:development /p:Configuration=Debug /p:Platform=x64
ECHO.
ECHO Or open hyperxtalk.sln in Visual Studio 2019 and build the 'development' project.
ECHO.
ECHO Note: ICU DLLs (icuuc58.dll, icuin58.dll, etc.) have been copied to:
ECHO   %DBG_OUT%\
ECHO These must remain alongside HyperXTalk.exe for it to run.
ECHO ============================================================================

EXIT /B 0

REM ---------------------------------------------------------------------------
REM Subroutine: make_icu_lib <def_file> <lib_file>
REM ---------------------------------------------------------------------------
:make_icu_lib
SET DEF=%~1
SET LIB_OUT=%~2
SET LIB_BASENAME=%~n2

IF NOT EXIST "%DEF%" (
    ECHO   WARNING: DEF file not found: %DEF%
    GOTO :eof
)

ECHO   Creating %LIB_BASENAME%.lib from %~n1.def...
lib.exe /def:"%DEF%" /machine:x64 /out:"%LIB_OUT%" /nologo
IF %ERRORLEVEL% NEQ 0 (
    ECHO   ERROR: lib.exe failed for %LIB_BASENAME%.lib
    EXIT /B 1
)
ECHO   OK: %LIB_BASENAME%.lib

GOTO :eof
