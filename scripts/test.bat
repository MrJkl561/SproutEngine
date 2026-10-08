@echo off
rem Builds the engine's unit tests and runs them.
rem
rem   scripts\test.bat            Release
rem   scripts\test.bat debug      Debug
setlocal
call "%~dp0_common.bat" %* || exit /b 1
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% -DSPROUT_BUILD_TESTS=ON || goto failed
cmake --build "%BUILD%" --config %CONFIG% --parallel || goto failed
ctest --test-dir "%BUILD%" -C %CONFIG% --output-on-failure || goto failed
echo.
echo All tests passed.
exit /b 0
:failed
echo.
echo Something failed (see the messages above).
exit /b 1
