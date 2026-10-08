@echo off
rem Builds the editor (if anything changed) and opens it.
rem
rem   scripts\run-editor.bat                     opens your last project
rem   scripts\run-editor.bat C:\Games\MyGame     opens that project folder
rem   scripts\run-editor.bat debug               a Debug build
setlocal
call "%~dp0_common.bat" %* || exit /b 1
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || goto failed
cmake --build "%BUILD%" --config %CONFIG% --target sprout_editor --parallel || goto failed
if defined ARG1 (start "" "%EDITOR%" "%ARG1%") else (start "" "%EDITOR%")
exit /b 0
:failed
echo.
echo The editor didn't build (see the messages above).
exit /b 1
