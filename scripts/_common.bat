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

if not defined ARCH (
    if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" (set "ARCH=arm64") else (set "ARCH=x64")
)
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
