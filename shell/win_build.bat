@echo off
REM ============================================================
REM Windows build script, the counterpart of build.sh
REM
REM   win_build.bat             incremental build and install
REM   win_build.bat debug       switch to Debug, build and install
REM   win_build.bat release     switch to Release, build and install
REM   win_build.bat refresh     drop cmake cache, reconfigure, build, install
REM   win_build.bat <target>    build one target only, e.g. llhttp
REM   win_build.bat verbose ... print full compiler command line
REM
REM Build dir : server\var\win-x64   (cmake cache lives here)
REM Output    : server\var          (master.exe, dll, pdb)
REM Install   : server\bin          (CMAKE_INSTALL_PREFIX)
REM Run cwd   : server\var
REM
REM Everything comes from Visual Studio itself (vcvars64 + bundled
REM cmake/ninja), so no extra tool needs to be installed.
REM ============================================================
setlocal

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%~dp0..\server\var\win-x64"
REM build.sh uses ../../../engine because it runs inside the build dir;
REM this script runs in shell/, so only one level up is needed
set "ENGINE_DIR=%~dp0..\engine"

REM cmake records the /showIncludes prefix of the compiler VERBATIM, and the
REM bytes cl writes depend on the console codepage. If configure and build use
REM different codepages the prefix never matches: ninja then dumps every
REM dependency line to the console AND loses header dependency tracking
REM (editing a header would not trigger a rebuild). So pin the codepage here,
REM and remember it - a build dir configured under another codepage is
REM re-detected once below.
set "DEP_CP=65001"
chcp %DEP_CP% >nul

REM -------- optional verbose flag --------
set "VERBOSE_FLAG="
if /i "%~1"=="verbose" (
    set "VERBOSE_FLAG=--verbose"
    shift
)
set "OPT=%~1"

REM -------- locate Visual Studio and enter its build environment --------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_DIR="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_DIR=%%i"
)
if not defined VS_DIR set "VS_DIR=D:\Program Files\Microsoft Visual Studio\2022\Community"
if not exist "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" (
    echo [build] error: vcvars64.bat not found, check your VS installation
    exit /b 1
)

call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [build] error: vcvars64.bat failed
    exit /b 1
)
REM vcvars64 needs reg.exe/registry access; in a restricted shell it returns
REM without setting INCLUDE, which shows up as "cannot open stdio.h"
if not defined INCLUDE (
    echo [build] error: vcvars64.bat did not set INCLUDE
    echo [build] run this script from a normal cmd window
    exit /b 1
)

REM cmake/ninja bundled with VS are on PATH now, keep a fallback
set "CMAKE=cmake"
where cmake >nul 2>nul
if errorlevel 1 set "CMAKE=%VS_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
echo [build] VS      : %VS_DIR%
echo [build] cmake   : %CMAKE%
echo [build] builddir: %BUILD_DIR%

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"

REM -------- decide what to do --------
set "MODE=normal"
set "CFG="
if /i "%OPT%"=="debug"   ( set "MODE=cfg" & set "CFG=Debug" )
if /i "%OPT%"=="release" ( set "MODE=cfg" & set "CFG=Release" )
if /i "%OPT%"=="refresh" ( set "MODE=refresh" )
if "%MODE%"=="normal" if not "%OPT%"=="" set "MODE=target"

if "%MODE%"=="refresh" (
    echo [build] removing cmake cache
    if exist CMakeCache.txt del /q /f CMakeCache.txt
    if exist CMakeFiles rd /s /q CMakeFiles
    if exist build.ninja del /q /f build.ninja
    if exist cmake_install.cmake del /q /f cmake_install.cmake
    if exist .cmake rd /s /q .cmake
    if exist .dep_cp del /q /f .dep_cp
)

REM -------- does the cache match the codepage we build with? --------
set "NEED_CFG="
set "LAST_CP="
if exist ".dep_cp" set /p LAST_CP=<".dep_cp"
if not exist CMakeCache.txt set "NEED_CFG=1"
if "%MODE%"=="cfg" set "NEED_CFG=1"
if /i not "%LAST_CP%"=="%DEP_CP%" (
    if exist CMakeCache.txt (
        echo [build] build dir was configured with another codepage, re-detect toolchain
        for /d %%d in (CMakeFiles\*-msvc*) do rd /s /q "%%~d"
        set "NEED_CFG=1"
    )
)

REM -------- configure if needed --------
if defined NEED_CFG (
    echo [build] cmake configure [Ninja]
    if "%MODE%"=="cfg" (
        "%CMAKE%" -S "%ENGINE_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CFG%
    ) else (
        "%CMAKE%" -S "%ENGINE_DIR%" -B "%BUILD_DIR%" -G Ninja
    )
    if errorlevel 1 goto :fail
    > ".dep_cp" echo %DEP_CP%
)

if "%MODE%"=="target" goto :build_target

REM -------- incremental build and install --------
echo [build] building...
"%CMAKE%" --build "%BUILD_DIR%" %VERBOSE_FLAG%
if errorlevel 1 goto :fail

echo [build] installing to server\bin
"%CMAKE%" --install "%BUILD_DIR%"
if errorlevel 1 goto :fail

echo [build] done: server\bin\master.exe updated
popd
exit /b 0

:build_target
echo [build] building target %OPT%
"%CMAKE%" --build "%BUILD_DIR%" --target %OPT% %VERBOSE_FLAG%
if errorlevel 1 goto :fail
popd
exit /b 0

:fail
echo [build] FAILED
popd
exit /b 1
