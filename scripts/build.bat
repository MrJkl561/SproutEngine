@echo off
rem Builds everything: the engine, the examples, the games and the editor.
rem
rem   scripts\build.bat                  Release, for this PC's CPU
rem   scripts\build.bat debug            a Debug build (slower, easier to debug)
rem   scripts\build.bat release x64      for a specific CPU: x64 or arm64
rem
rem The programs end up in build-x64\bin\ (or build-arm64\bin\).
setlocal
call "%~dp0_common.bat" %* || exit /b 1
echo == Building Sprout (%CONFIG%, %ARCH%) in %BUILD% ==
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || goto failed
cmake --build "%BUILD%" --config %CONFIG% --parallel || goto failed
echo.
echo Done. Programs are in %BUILD%\bin\
exit /b 0
:failed
echo.
echo The build failed (see the messages above).
exit /b 1
