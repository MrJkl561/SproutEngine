@echo off
rem Tests the Script tab's extra checks (editor\src\script_lint.cpp): as many
rem warnings as editor\tests\lint\scripts\lint_me.cpp has lines marked "// warn".
rem
rem   scripts\test-lint.bat            Release
rem   scripts\test-lint.bat debug      Debug
setlocal
call "%~dp0_common.bat" %* || exit /b 1
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || goto failed
cmake --build "%BUILD%" --config %CONFIG% --target sprout_editor --parallel || goto failed
set "FILE=%ROOT%\editor\tests\lint\scripts\lint_me.cpp"
"%EDITOR%" --project "%ROOT%\editor\tests\lint" --check-file "%FILE%" > "%TEMP%\sprout-lint.txt" 2>&1
type "%TEMP%\sprout-lint.txt"
for /f %%n in ('findstr /c:"// warn" "%FILE%" ^| find /c /v ""') do set "WANT=%%n"
for /f %%n in ('findstr /c:"here runs when the script is made" "%TEMP%\sprout-lint.txt" ^| find /c /v ""') do set "GOT=%%n"
echo Warnings wanted: %WANT%, got: %GOT%
if not "%WANT%"=="%GOT%" goto failed
findstr /c:" 0 error(s)" "%TEMP%\sprout-lint.txt" >nul || goto failed
echo.
echo Script checks test passed.
exit /b 0
:failed
echo.
echo The script checks test failed (see the messages above).
exit /b 1
