@echo off
title Attack Vector 2 - Memory Modifier
cd /d "%~dp0tools"
if not exist "MemoryModifier.exe" (
    echo [ERROR] MemoryModifier.exe not found!
    pause
    exit /b 1
)
MemoryModifier.exe
