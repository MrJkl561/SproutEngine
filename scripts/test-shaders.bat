@echo off
rem Tests custom shaders: plays editor\tests\shaders for a few frames and checks
rem the colours it draws, and that a broken shader's error names its line.
rem
rem   scripts\test-shaders.bat            Release
rem   scripts\test-shaders.bat debug      Debug
setlocal
call "%~dp0_common.bat" %* || exit /b 1
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || goto failed
cmake --build "%BUILD%" --config %CONFIG% --target sprout_editor --parallel || goto failed
set "SPROUT_TEST_PIXELS=40,45=0,0,255;81,1=0,0,0;158,88=255,255,0;120,45=128,128,0;200,45=0,255,0;280,45=255,0,255"
"%EDITOR%" --project "%ROOT%\editor\tests\shaders" --play --frames 10 > "%TEMP%\sprout-shaders.txt" 2>&1
type "%TEMP%\sprout-shaders.txt"
findstr /c:"PIXELS OK" "%TEMP%\sprout-shaders.txt" >nul || goto failed
findstr /c:"broken.shader:4:" "%TEMP%\sprout-shaders.txt" >nul || goto failed
echo.
echo Shaders test passed.
exit /b 0
:failed
echo.
echo The shaders test failed (see the messages above).
exit /b 1
