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

rem Visual Studio only builds for ARM64 if its ARM64 C++ tools are installed
rem (an optional part of the installer). Ask its installer.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSCHECK="
if not defined CMAKE_GENERATOR if exist "%VSWHERE%" set "VSCHECK=1"
set "HAVEARM64="
if defined VSCHECK for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.ARM64 -property installationPath`) do set "HAVEARM64=1"
if /i not "%ARCH%"=="arm64" goto archok
if not defined VSCHECK goto archok
if defined HAVEARM64 goto archok
if defined AUTOARCH goto archfallback
echo Visual Studio's ARM64 C++ tools aren't installed, so it can't build for arm64.
echo Add them in the Visual Studio Installer: Modify, Individual components,
echo "MSVC ... ARM64/ARM64EC build tools (Latest)". Or build for x64 instead:
echo     scripts\build.bat x64      (the same goes for the other scripts)
exit /b 1
:archfallback
echo Note: Visual Studio's ARM64 C++ tools aren't installed, so this builds for x64.
echo       x64 programs run fine on ARM64 Windows. To build natively for ARM64, add
echo       "MSVC ... ARM64/ARM64EC build tools (Latest)" in the Visual Studio Installer.
echo.
set "ARCH=x64"
:archok
if /i "%ARCH%"=="arm64" (set "VSARCH=ARM64") else (set "VSARCH=x64")
set "BUILD=%ROOT%\build-%ARCH%"
set "EDITOR=%BUILD%\bin\sprout_editor\sprout_editor.exe"

rem Visual Studio needs to be told the CPU (another generator picks its own)
set "ARCHFLAG="
if not defined CMAKE_GENERATOR set "ARCHFLAG=-A %VSARCH%"
echo %CMAKE_GENERATOR% | findstr /i /c:"Visual Studio" >nul && set "ARCHFLAG=-A %VSARCH%"

where cmake >nul 2>&1
if errorlevel 1 (
    echo CMake was not found. Install it from https://cmake.org/download/ ^(tick "Add CMake to the PATH"^)
    echo or install Visual Studio with "Desktop development with C++", then run this again.
    exit /b 1
)
exit /b 0
