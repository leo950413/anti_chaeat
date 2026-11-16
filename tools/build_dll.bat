@echo off
echo ==============================================
echo Building misc_dll.dll with MSVC x64...
echo ==============================================

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Could not initialize MSVC environment.
    pause
    exit /b 1
)

cl.exe /LD /EHsc /O2 /Fe:"%~dp0misc_dll.dll" "%~dp0misc_dll.cpp"
if %ERRORLEVEL% equ 0 (
    echo.
    echo [SUCCESS] misc_dll.dll created in tools directory!
    del "%~dp0misc_dll.obj" "%~dp0misc_dll.exp" "%~dp0misc_dll.lib" 2>nul
) else (
    echo.
    echo [ERROR] Build failed!
)
