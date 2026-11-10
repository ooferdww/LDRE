@echo off
setlocal enabledelayedexpansion
color B

echo ======================================
echo == dsda-doom + LDRE: clang-cl build ==
echo ======================================

set selpreset=windows-clang
cd /d "%~dp0.."

:: init vs developer prompt
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
    color 4
    echo.&echo [ERROR] vswhere.exe not found at "%VSWHERE%".
    echo Please make sure Visual Studio or Visual Studio Build Tools is installed.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_DIR=%%i"
)

if not defined VS_DIR (
    color 4
    echo.&echo [ERROR] No suitable Visual Studio toolchain found.
    exit /b 1
)

set "VCVARS=%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    color 4
    echo.&echo [ERROR] vcvars64.bat not found at "%VCVARS%".
    exit /b 1
)

echo.&echo [INFO] Initializing developer environment from:
echo        "%VS_DIR%"
call "%VCVARS%" >NUL
if errorlevel 1 (
    color 4
    echo [ERROR] Failed to initialize developer environment.
    exit /b 1
)

:: vcpkg update
if exist "%VS_DIR%\VC\vcpkg\vcpkg.exe" (
    echo.&echo [INFO] Syncing baseline with vcpkg
    start /wait cmd /c "vcpkg x-update-baseline --add-initial-baseline"
) else (
    echo.&echo [WARNING] vcpkg executable not found, skipping baseline sync.
	echo [WARNING] Build might fail.
)

:: refresh build dir
rd "build" /s /q >NUL

:: configure with cmake
echo.&echo [INFO] Configuring using preset: %selpreset%
cmake --preset %selpreset%

if errorlevel 1 (
    echo [ERROR] CMake preset configuration failed.
    exit /b 1
)

:: build targets
echo.&echo [INFO] Building preset: %selpreset%
cmake --build --preset %selpreset% --config Release

if errorlevel 1 (
    echo.&echo [ERROR] Build failed.
    pause
    exit /b 1
)

:: summary
color A
echo.&echo Generated output:
dir /b /s "build\windows-clang\*.exe" 2>nul
dir /b /s "build\windows-clang\*.wad" 2>nul
echo.&echo ==========================
echo [SUCCESS] Build completed.
echo ==========================

pause

endlocal
exit /b 0