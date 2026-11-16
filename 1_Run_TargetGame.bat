@echo off
title TargetGame (3D FPS Arena)
echo Starting TargetGame...
cd /d "%~dp0out\build\x64-debug"
if not exist "TargetGame.exe" (
    echo [ERROR] TargetGame.exe not found! Please build the project first.
    pause
    exit /b 1
)
start "" TargetGame.exe
