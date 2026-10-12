@echo off
rem Builds the editor and puts a portable copy of it in one folder: the program
rem plus everything it needs (its sources, the libraries, the templates). Move
rem or copy that folder anywhere.
rem
rem   scripts\make-portable.bat                    into dist\SproutEditor
rem   scripts\make-portable.bat C:\Tools\Sprout    into that folder
setlocal
call "%~dp0_common.bat" %* || exit /b 1
set "OUT=%ROOT%\dist\SproutEditor"
if defined ARG1 set "OUT=%ARG1%"
rem Only ever replace an earlier portable copy, never some other folder
if exist "%OUT%\editor\src\script.hpp" goto outok
dir /b /a "%OUT%" 2>nul | findstr "^" >nul && (
    echo %OUT% already has other files in it: pick an empty or new folder.
    exit /b 1
)
:outok
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || goto failed
cmake --build "%BUILD%" --config %CONFIG% --target sprout_editor --parallel || goto failed
if exist "%OUT%\editor\src\script.hpp" rmdir /s /q "%OUT%"
cmake --install "%BUILD%" --config %CONFIG% --component sprout_editor --prefix "%OUT%" || goto failed
echo.
echo Done: a portable Sprout Editor is in %OUT%
echo Start it with %OUT%\sprout_editor.exe
exit /b 0
:failed
echo.
echo The portable copy couldn't be made (see the messages above).
exit /b 1
