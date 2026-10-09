@echo off
rem Tests autoloads: runs editor\tests\autoload without a window and checks that
rem they're made before the scene, kept across a scene change, and found by scripts.
rem
rem   scripts\test-autoload.bat            Release
rem   scripts\test-autoload.bat debug      Debug
setlocal
call "%~dp0_common.bat" %* || exit /b 1
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || goto failed
cmake --build "%BUILD%" --config %CONFIG% --target sprout_editor --parallel || goto failed
"%EDITOR%" --project "%ROOT%\editor\tests\autoload" --check-scripts || goto failed
"%EDITOR%" --project "%ROOT%\editor\tests\autoload" --play --headless --frames 120 > "%TEMP%\sprout-autoload.txt" 2>&1
type "%TEMP%\sprout-autoload.txt"
findstr /c:"AUTOLOAD TEST PASSED" "%TEMP%\sprout-autoload.txt" >nul || goto failed
echo.
echo Autoload test passed.
exit /b 0
:failed
echo.
echo The autoload test failed (see the messages above).
exit /b 1
