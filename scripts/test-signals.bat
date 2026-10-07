@echo off
rem Tests the editor's signals: runs editor\tests\signals without a window and
rem checks that every way of connecting and sending signals works.
rem
rem   scripts\test-signals.bat            Release
rem   scripts\test-signals.bat debug      Debug
setlocal
call "%~dp0_common.bat" %* || exit /b 1
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || goto failed
cmake --build "%BUILD%" --config %CONFIG% --target sprout_editor --parallel || goto failed
"%EDITOR%" --project "%ROOT%\editor\tests\signals" --check-scripts || goto failed
"%EDITOR%" --project "%ROOT%\editor\tests\signals" --play --headless --frames 120 > "%TEMP%\sprout-signals.txt" 2>&1
type "%TEMP%\sprout-signals.txt"
findstr /c:"SIGNALS TEST PASSED" "%TEMP%\sprout-signals.txt" >nul || goto failed
echo.
echo Signals test passed.
exit /b 0
:failed
echo.
echo The signals test failed (see the messages above).
exit /b 1
