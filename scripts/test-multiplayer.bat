@echo off
rem Tests the editor's multiplayer for real: starts a dedicated server and two
rem clients (separate programs, no windows) on this PC, and checks that they
rem connect, call each other's RPCs, keep nodes in sync (interpolated), spawn
rem players, predict movement, compensate for lag and see each other leave.
rem Uses editor\tests\multiplayer.
rem
rem   scripts\test-multiplayer.bat            Release
rem   scripts\test-multiplayer.bat debug      Debug
setlocal
call "%~dp0_common.bat" %* || exit /b 1
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || goto failed
cmake --build "%BUILD%" --config %CONFIG% --target sprout_editor --parallel || goto failed
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-multiplayer.ps1" -Editor "%EDITOR%" || goto failed
exit /b 0
:failed
echo.
echo The multiplayer test failed (see the messages above).
exit /b 1
