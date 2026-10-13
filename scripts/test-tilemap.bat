@echo off
rem Tests tile maps and the UI nodes: imports the Tiled maps in
rem editor\tests\tilemap\maps into a copy of the test project and checks the
rem nodes they become, then plays its main scene: bodies on solid tiles, the
rem TileMap functions, clicks on buttons, a container's layout and the colours.
rem
rem   scripts\test-tilemap.bat            Release
rem   scripts\test-tilemap.bat debug      Debug
setlocal
call "%~dp0_common.bat" %* || exit /b 1
cmake -S "%ROOT%" -B "%BUILD%" %ARCHFLAG% || goto failed
cmake --build "%BUILD%" --config %CONFIG% --target sprout_editor --parallel || goto failed
set "WORK=%TEMP%\sprout-tilemap-test"
if exist "%WORK%" rmdir /s /q "%WORK%"
xcopy /e /i /q "%ROOT%\editor\tests\tilemap" "%WORK%" >nul || goto failed
set "PROJECT=%WORK%\project"
set "OUT=%TEMP%\sprout-tilemap.txt"
set FAILED=0

echo Importing the Tiled maps...
for %%M in (level_csv.tmx level_zlib.tmx level_xml.tmx level_gzip.tmj level_infinite.tmj) do (
    "%EDITOR%" --project "%PROJECT%" --import-tiled "%WORK%\maps\%%M" > "%OUT%" 2>&1
    findstr /c:"Imported %%M" "%OUT%" >nul || (echo   FAILED: %%M & type "%OUT%" & set FAILED=1)
)
"%EDITOR%" --project "%PROJECT%" --import-tiled "%WORK%\maps\broken.tmx" > "%OUT%" 2>&1
call :expect "a zstd map says what to do" "compressed with zstd can't be read"

echo Checking the imported scenes...
for %%S in (level_csv level_zlib level_xml level_gzip) do (
    "%EDITOR%" --project "%PROJECT%" --play --scene scenes/%%S.scene.json --frames 2 > "%OUT%" 2>&1
    findstr /c:"NODE %%S/Ground TileMap at=0,0 cells=12 src=assets/tiles/tiles.png solid=0;2; type1=spikes" "%OUT%" >nul || (echo   FAILED: %%S & set FAILED=1)
    findstr /c:"list=0,0:0/3/1 0,3:0/0/0 1,3:0/0/0 2,3:0/0/0 3,3:0/0/0 4,2:0/1/0 4,3:0/0/0" "%OUT%" >nul || (echo   FAILED: %%S cells & set FAILED=1)
)
"%EDITOR%" --project "%PROJECT%" --play --scene scenes/level_csv.scene.json --frames 2 > "%OUT%" 2>&1
call :expect "a layer's offset" "NODE level_csv/Decor TileMap at=8,4 cells=1"
call :expect "a rectangle object" "NODE level_csv/Objects/Goal/CollisionShape2D CollisionShape2D at=8,16 size=16x32"
call :expect "a solid object" "NODE level_csv/Objects/solid StaticBody2D at=0,0"
call :expect "a text object" "NODE level_csv/Objects/Sign Label at=20,4 text=Hello"
call :expect "a tile object" "NODE level_csv/Objects/Coin Sprite2D at=64,48 tex=assets/tiles/tiles.png frame=1"
call :expect "a group's layers" "NODE level_csv/Group/Inner TileMap at=0,0 cells=1"
"%EDITOR%" --project "%PROJECT%" --play --scene scenes/level_infinite.scene.json --frames 2 > "%OUT%" 2>&1
call :expect "an infinite map's chunks" "list=-1,15:0/3/2 0,0:0/0/0 1,1:0/1/0"

echo Playing the main scene...
set "SPROUT_TEST_CLICKS=250,20@5;210,90@10;110,70@15;5,5@20"
set "SPROUT_TEST_PIXELS=60,136=255,0,0;58,40=0,0,0;70,40=255,255,255;74,40=255,255,255;86,40=0,0,0;220,155=0,255,0;290,155=0,0,0"
"%EDITOR%" --project "%PROJECT%" --play --frames 70 > "%OUT%" 2>&1
set SPROUT_TEST_CLICKS=
set SPROUT_TEST_PIXELS=
call :expect "the TileMap functions" "MAPAPI cell=5,8 tile=0 now=2 solid=100 type=grass empty=-1"
call :expect "clicks, signals and layout" "UI play=1 world=1 toggles=1 sound=on quitY=34 quitW=110 menuH=96"
call :expect "a body lands on solid tiles" "BODY Faller floor=yes y=122"
call :expect "a body falls through" "BODY Ghost floor=no"
call :expect "the colours drawn" "PIXELS OK"

"%EDITOR%" --project "%PROJECT%" --play --frames 300 -- --quit-test > "%OUT%" 2>&1
call :expect "quit() ends the game" "QUITTING"
findstr /c:"BODY Faller" "%OUT%" >nul && (echo   FAILED: the game kept running after quit^(^) & set FAILED=1)

rmdir /s /q "%WORK%"
if "%FAILED%"=="1" goto failed
echo.
echo TileMap and UI test passed.
exit /b 0

:expect
findstr /c:%2 "%OUT%" >nul && (echo   ok: %~1) || (echo   FAILED: %~1 & set FAILED=1)
exit /b 0

:failed
echo.
echo The TileMap and UI test failed (see the messages above).
exit /b 1
