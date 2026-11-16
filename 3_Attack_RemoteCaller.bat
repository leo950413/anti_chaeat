@echo off
title Attack Vector 3 - Remote Function Caller (DLL Injection)
cd /d "%~dp0tools"
if not exist "remote_caller.exe" (
    echo [ERROR] remote_caller.exe not found!
    pause
    exit /b 1
)
remote_caller.exe
