@echo off
setlocal enabledelayedexpansion
title Attack Vector 1 - Wallhack Texture Switcher

set "OUT_WALL=%~dp0out\build\x64-debug\assets\wall.png"
set "ASSET_REAL=%~dp0assets\wall.png"
set "ASSET_FAKE=%~dp0assets\fake_wall.png"

echo ========================================================
echo   Attack Vector 1: Texture Wallhack Switcher
echo ========================================================
echo.
echo [1] Switch to FAKE Transparent Wall (Activate Wallhack)
echo [2] Switch to REAL Opaque Brick Wall (Normal State)
echo [0] Exit
echo.
set /p choice="Enter choice [1/2]: "

if "%choice%"=="1" (
    copy /y "%ASSET_FAKE%" "%OUT_WALL%" >nul
    echo.
    echo [+] SUCCESS: Wallhack ACTIVATED! (wall.png is now transparent)
    echo     Next time you launch TargetGame.exe, walls will be see-through.
) else if "%choice%"=="2" (
    copy /y "%ASSET_REAL%" "%OUT_WALL%" >nul
    echo.
    echo [+] SUCCESS: Restored normal opaque brick wall.
) else (
    echo Exiting.
)
echo.
pause
