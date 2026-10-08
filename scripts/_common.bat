@echo off
rem Shared by the other scripts: reads [debug|release] [x64|arm64] and sets
rem   ROOT    the repository folder
rem   CONFIG  Release or Debug
rem   ARCH    x64 or arm64 (default: this PC's own CPU)
rem   BUILD   the build folder (build-x64 or build-arm64)
rem   EDITOR  the editor program inside it
rem   ARCHFLAG  -A x64 / -A ARM64 for Visual Studio
rem   ARG1, ARG2, ARG3  the other arguments (quotes removed, spaces kept)
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "CONFIG=Release"
set "ARCH="
set "ARG1=" & set "ARG2=" & set "ARG3="

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="debug"   (set "CONFIG=Debug"   & shift & goto parse)
if /i "%~1"=="release" (set "CONFIG=Release" & shift & goto parse)
if /i "%~1"=="x64"     (set "ARCH=x64"       & shift & goto parse)
if /i "%~1"=="amd64"   (set "ARCH=x64"       & shift & goto parse)
if /i "%~1"=="arm64"   (set "ARCH=arm64"     & shift & goto parse)
if not defined ARG1 (set "ARG1=%~1" & shift & goto parse)
if not defined ARG2 (set "ARG2=%~1" & shift & goto parse)
if not defined ARG3 (set "ARG3=%~1" & shift & goto parse)
shift
goto parse
:parsed

set "AUTOARCH="
if not defined ARCH (
    set "AUTOARCH=1"
    if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" (set "ARCH=arm64") else (set "ARCH=x64")
)

rem Which Visual Studio to use: the newest one that has the C++ tools for this
rem CPU. (CMake would pick the newest one installed, even one without C++.)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSGEN="
set "VSPATH="
if defined CMAKE_GENERATOR goto archok
if not exist "%VSWHERE%" goto archok
if /i "%ARCH%"=="arm64" (call :findvs Microsoft.VisualStudio.Component.VC.Tools.ARM64) else (call :findvs Microsoft.VisualStudio.Component.VC.Tools.x86.x64)
if defined VSPATH goto archok
if /i not "%ARCH%"=="arm64" goto novs
if not defined AUTOARCH goto noarm
echo Note: Visual Studio's ARM64 C++ tools aren't installed, so this builds for x64.
echo       x64 programs run fine on ARM64 Windows. To build natively for ARM64, add
echo       "MSVC ... ARM64/ARM64EC build tools (Latest)" in the Visual Studio Installer.
echo.
set "ARCH=x64"
call :findvs Microsoft.VisualStudio.Component.VC.Tools.x86.x64
if defined VSPATH goto archok
:novs
echo No Visual Studio with the C++ tools was found.
echo Open the Visual Studio Installer, click Modify, and tick the workload
echo "Desktop development with C++" (it includes "MSVC ... x64/x86 build tools").
exit /b 1
:noarm
echo Visual Studio's ARM64 C++ tools aren't installed, so it can't build for arm64.
echo Add them in the Visual Studio Installer: Modify, Individual components,
echo "MSVC ... ARM64/ARM64EC build tools (Latest)". Or build for x64 instead:
echo     scripts\build.bat x64      (the same goes for the other scripts)
exit /b 1
:archok
if /i "%ARCH%"=="arm64" (set "VSARCH=ARM64") else (set "VSARCH=x64")
set "BUILD=%ROOT%\build-%ARCH%"
set "EDITOR=%BUILD%\bin\sprout_editor\sprout_editor.exe"

rem Visual Studio needs to be told the CPU (another generator picks its own)
set "ARCHFLAG="
if not defined CMAKE_GENERATOR set "ARCHFLAG=-A %VSARCH%"
echo %CMAKE_GENERATOR% | findstr /i /c:"Visual Studio" >nul && set "ARCHFLAG=-A %VSARCH%"
if defined VSGEN set ARCHFLAG=-G "%VSGEN%" -A %VSARCH% "-DCMAKE_GENERATOR_INSTANCE=%VSPATH%"

where cmake >nul 2>&1
if errorlevel 1 (
    echo CMake was not found. Install it from https://cmake.org/download/ ^(tick "Add CMake to the PATH"^)
    echo or install Visual Studio with "Desktop development with C++", then run this again.
    exit /b 1
)
exit /b 0

rem :findvs <component>  ->  VSPATH (its folder) and VSGEN (CMake's name for it)
:findvs
set "VSPATH="
set "VSGEN="
set "VSVER="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires %1 -property installationPath`) do set "VSPATH=%%i"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires %1 -property installationVersion`) do set "VSVER=%%i"
if not defined VSPATH goto :eof
for /f "tokens=1 delims=." %%v in ("%VSVER%") do (
    if "%%v"=="18" set "VSGEN=Visual Studio 18 2026"
    if "%%v"=="17" set "VSGEN=Visual Studio 17 2022"
    if "%%v"=="16" set "VSGEN=Visual Studio 16 2019"
)
goto :eof
