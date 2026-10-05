@echo off
rem Exports a game made with the editor, without opening the editor.
rem
rem   scripts\export.bat windows                       the demo project, as a Windows game
rem   scripts\export.bat windows,web                   several platforms at once
rem   scripts\export.bat windows C:\Games\MyGame       another project
rem   scripts\export.bat windows C:\Games\MyGame D:\out   ...and where to put it
rem
rem Platforms on Windows: windows (x64, runs on every Windows PC),
rem windows-arm64, web (run scripts\setup-web.bat once first).
rem The game goes in the project's exports\ folder unless you give one.
setlocal
call "%~dp0_common.bat" %* || exit /b 1
set "TARGETS=%ARG1%"
set "PROJECT=%ARG2%"
set "OUT=%ARG3%"
if not defined TARGETS (
    echo Which platforms? For example:  scripts\export.bat windows,web
    exit /b 1
)
if not exist "%EDITOR%" (
    echo == Building the editor first ==
    cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || exit /b 1
    cmake --build "%BUILD%" --config %CONFIG% --target sprout_editor --parallel || exit /b 1
)
if not defined PROJECT set "PROJECT=%ROOT%\editor\project"
if defined OUT (
    "%EDITOR%" --project "%PROJECT%" --export %TARGETS% "%OUT%"
) else (
    "%EDITOR%" --project "%PROJECT%" --export %TARGETS%
)
exit /b %ERRORLEVEL%
