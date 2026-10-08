@echo off
rem Gets Emscripten (emsdk) ready for web exports. Run it once.
rem
rem   scripts\setup-web.bat                         uses (or downloads) %USERPROFILE%\emsdk
rem   scripts\setup-web.bat C:\Users\me\Desktop\emsdk   an emsdk folder you already have
rem
rem Emscripten has no Windows ARM64 version, so the x64 one is used everywhere
rem (ARM64 PCs run it through Windows' x64 emulation).
setlocal
set "EMSDK_DIR=%~1"
if not defined EMSDK_DIR set "EMSDK_DIR=%USERPROFILE%\emsdk"

if not exist "%EMSDK_DIR%\emsdk.py" (
    where git >nul 2>&1
    if errorlevel 1 (
        echo emsdk isn't in %EMSDK_DIR% and Git isn't installed to download it.
        echo Install Git from https://git-scm.com/ or download emsdk from
        echo https://github.com/emscripten-core/emsdk and run:  scripts\setup-web.bat ^<its folder^>
        exit /b 1
    )
    echo == Downloading emsdk to %EMSDK_DIR% ==
    git clone https://github.com/emscripten-core/emsdk.git "%EMSDK_DIR%" || exit /b 1
)

set "EMSDK_ARCH=x86_64"
pushd "%EMSDK_DIR%"
echo == emsdk install latest (about 1 GB the first time) ==
call emsdk.bat install latest || goto failed
echo == emsdk activate latest ==
call emsdk.bat activate latest || goto failed
popd

where ninja >nul 2>&1
if errorlevel 1 (
    echo.
    echo Web builds also need Ninja. If the editor says it's missing, run:
    echo     winget install Ninja-build.Ninja
)
echo.
echo Emscripten is ready in %EMSDK_DIR%.
echo Export the web version with:  scripts\export.bat web
echo (The editor finds emsdk in your home folder, Desktop or Documents by itself;
echo  anywhere else, pick it with "Choose..." in its Export window.)
exit /b 0
:failed
popd
echo.
echo Setting up Emscripten failed (see above). If Python wasn't found, install it from
echo https://www.python.org/downloads/ (tick "Add python.exe to PATH") and run this again.
exit /b 1
