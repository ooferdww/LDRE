@echo off
setlocal enabledelayedexpansion
color B

echo ======================================
echo == dsda-doom + LDRE: clang-cl build ==
echo ======================================
echo Select CPU target profile:
echo   [1] Default   (most compatible, should work on most 64-bit processors)
echo   [2] Optimized (znver2 tuning, should optimally run on most processors with AVX2)
echo   [3] Native    (native, usually most optimal for your system but don't expect it to run on others' systems)
echo.
choice /c 123 /n /m "Choose profile [1-3]: "

if errorlevel 3 (
    set "ARCH_PROFILE=native"
    set "PROFILE_LABEL=Native (native)"
) else if errorlevel 2 (
    set "ARCH_PROFILE=modern"
    set "PROFILE_LABEL=Optimized (modern / znver2)"
) else (
    set "ARCH_PROFILE=compat"
    set "PROFILE_LABEL=Default (compat)"
)

echo.&echo Enable Link-Time Optimization (LTO)? (Slower link, slightly faster binary)
echo.
choice /c YN /n /m "Enable LTO [Y/N]: "

if errorlevel 2 (
    set "LTO_ENABLED=OFF"
    set "LTO_LABEL=LTO: OFF"
) else (
    set "LTO_ENABLED=ON"
    set "LTO_LABEL=LTO: ON"
)

echo.&echo [INFO] Selected configuration: %PROFILE_LABEL% ^| %LTO_LABEL%

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
if exist "build" rd "build" /s /q >NUL

:: configure with cmake
echo.&echo [INFO] Configuring using preset: %selpreset% [LDRE_ARCH_PROFILE=%ARCH_PROFILE%, LDRE_ENABLE_LTO=%LTO_ENABLED%]
cmake --preset %selpreset% -DLDRE_ARCH_PROFILE=%ARCH_PROFILE% -DLDRE_ENABLE_LTO=%LTO_ENABLED%

if errorlevel 1 (
    echo [ERROR] CMake preset configuration failed.
    exit /b 1
)

:: build targets
echo.&echo [INFO] Building preset: %selpreset% (%PROFILE_LABEL% ^| %LTO_LABEL%)
cmake --build --preset %selpreset% --config Release --parallel %NUMBER_OF_PROCESSORS% -- -m

if errorlevel 1 (
    echo.&echo [ERROR] Build failed.
    pause
    exit /b 1
)

:: summary
color A
echo.&echo Generated output (%PROFILE_LABEL% ^| %LTO_LABEL%):
dir /b /s "build\windows-clang\*.exe" 2>nul
dir /b /s "build\windows-clang\*.wad" 2>nul
echo.&echo ==========================
echo [SUCCESS] Build completed.
echo ==========================

pause

endlocal
exit /b 0