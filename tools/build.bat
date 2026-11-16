@echo off
echo ========================================================
echo Building External Tools (MSVC x64)
echo ========================================================

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Could not initialize MSVC environment.
    pause
    exit /b 1
)

pushd "%~dp0"

echo [*] Compiling misc_dll.dll (Injected Payload)...
cl.exe /LD /EHsc /O2 /Fe:"misc_dll.dll" "misc_dll.cpp"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile misc_dll.dll!
    popd
    pause
    exit /b 1
)

echo [*] Compiling remote_caller.exe (Function Invoker)...
cl.exe /EHsc /O2 /Fe:"remote_caller.exe" "remote_caller.cpp"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile remote_caller.exe!
    popd
    pause
    exit /b 1
)

echo [*] Compiling MemoryModifier.exe (External RPM/WPM)...
cl.exe /EHsc /O2 /Fe:"MemoryModifier.exe" "memory_modifcication.cpp"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to compile MemoryModifier.exe!
    popd
    pause
    exit /b 1
)

del *.obj *.exp *.lib 2>nul
popd
echo.
echo ========================================================
echo [SUCCESS] All tools compiled successfully in tools\
echo   - misc_dll.dll
echo   - remote_caller.exe
echo   - MemoryModifier.exe
echo ========================================================
