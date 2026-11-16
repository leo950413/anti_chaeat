@echo off
setlocal enabledelayedexpansion
title AntiCheat Lab - Build All Binaries
echo ========================================================
echo   AntiCheat Lab: Building All Targets (MSVC x64)
echo ========================================================
echo.

:: Initialize MSVC 64-bit environment
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [!] Standard MSVC path not found, trying vswhere...
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do (
        if exist "%%i\VC\Auxiliary\Build\vcvarsall.bat" (
            call "%%i\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
        )
    )
)

echo [*] MSVC Environment Initialized.
echo.

:: 1. Build Tools (misc_dll.dll, remote_caller.exe, MemoryModifier.exe)
echo [1/3] Compiling Attack Tools in tools\...
call "%~dp0tools\build.bat"

:: 2. Build TargetGame.exe via CMake / Ninja if available
echo.
echo [2/3] Checking TargetGame.exe build...
set "NINJA_EXE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if exist "%NINJA_EXE%" (
    if exist "%~dp0out\build\x64-debug\build.ninja" (
        echo [*] Running Ninja incremental build for TargetGame...
        "%NINJA_EXE%" -C "%~dp0out\build\x64-debug" TargetGame
    ) else if exist "C:\Users\leo95\OneDrive\桌面\OS\anti_chaeat\out\build\x64-debug\build.ninja" (
        echo [*] Running Ninja incremental build from OS repo...
        "%NINJA_EXE%" -C "C:\Users\leo95\OneDrive\桌面\OS\anti_chaeat\out\build\x64-debug" TargetGame
        copy /y "C:\Users\leo95\OneDrive\桌面\OS\anti_chaeat\out\build\x64-debug\TargetGame.*" "%~dp0out\build\x64-debug\" >nul
    )
) else (
    echo [*] Ninja not in default path, TargetGame.exe pre-built binary will be used.
)

:: 3. Ensure assets are synced to out\build\x64-debug\assets
echo.
echo [3/3] Synchronizing assets to runtime directory...
if not exist "%~dp0out\build\x64-debug\assets" mkdir "%~dp0out\build\x64-debug\assets"
copy /y "%~dp0assets\*" "%~dp0out\build\x64-debug\assets\" >nul

:: Clean up intermediate files in root and tools
del "%~dp0*.obj" "%~dp0tools\*.obj" "%~dp0tools\*.exp" "%~dp0tools\*.lib" 2>nul

echo.
echo ========================================================
echo [SUCCESS] Build Complete! Ready for Demonstration:
echo   1. 1_Run_TargetGame.bat          -> TargetGame.exe
echo   2. 2_Attack_MemoryModifier.bat   -> MemoryModifier.exe
echo   3. 3_Attack_RemoteCaller.bat     -> remote_caller.exe + misc_dll.dll
echo   4. 4_Toggle_Wallhack.bat         -> Switch Texture Wallhack
echo ========================================================
pause
