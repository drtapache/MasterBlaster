@echo off
setlocal EnableDelayedExpansion
:: ============================================================
::  dev_build.bat  —  DEVELOPER TOOL ONLY
::
::  Builds the complete MasterBlaster pipeline locally.
::  END USERS DO NOT NEED THIS FILE.
::  End users receive only: MasterBlaster-1.1.0-Setup.exe
::
::  Produces: installer\dist\MasterBlaster-1.1.0-Setup.exe
::  Distribute ONLY that .exe — nothing else.
::
::  Requirements (developer machine only):
::    • Git           — git-scm.com
::    • Visual Studio 2022 with "Desktop development with C++"
::      (or "Build Tools for Visual Studio 2022")
::      Download: https://visualstudio.microsoft.com/downloads/
::    • Internet on first build (CMake downloads JUCE automatically)
::
::  PREFERRED: Push to GitHub — CI builds automatically with zero
::             local tools. See .github/workflows/build.yml
:: ============================================================
title MasterBlaster — Build & Package

:: ── Detect CPU count ─────────────────────────────────────────────────────────
for /f "tokens=*" %%C in (
    'powershell -NoProfile -Command "[Environment]::ProcessorCount"'
) do set "NCPU=%%C"
if "%NCPU%"=="" set "NCPU=4"

echo.
echo ============================================================
echo   MasterBlaster  ^|  Circuit Burn Audio  ^|  v1.1
echo ============================================================
echo.

:: ── Prerequisite checks ──────────────────────────────────────────────────────
set "FAIL=0"

where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] cmake not found.
    echo         Install Visual Studio 2022 with C++ workload — it includes CMake.
    echo         Or: https://cmake.org/download/
    set "FAIL=1"
)

where git >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] git not found. Install from https://git-scm.com/
    set "FAIL=1"
)

if "%FAIL%"=="1" (
    echo.
    echo  TIP: Push to GitHub instead — the CI builds automatically.
    echo       See .github\workflows\build.yml for details.
    echo.
    pause & exit /b 1
)

:: ── Step 1: Build plugin ─────────────────────────────────────────────────────
echo [1/4] Configuring plugin (CMake + MSVC)...
echo.

if not exist build mkdir build

cmake -B build ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DJUCE_BUILD_EXAMPLES=OFF ^
    -DJUCE_BUILD_EXTRAS=OFF

if errorlevel 1 (
    echo.
    echo [ERROR] CMake configure failed.
    echo         Make sure Visual Studio 2022 with C++ workload is installed.
    pause & exit /b 1
)

echo.
echo [2/4] Compiling plugin (Release, %NCPU% threads)...
echo       First build downloads JUCE — this takes a few minutes.
echo.

cmake --build build --config Release --parallel %NCPU%

if errorlevel 1 (
    echo.
    echo [ERROR] Plugin compilation failed. Check the output above.
    pause & exit /b 1
)

:: Verify output
set "VST3_BUNDLE=build\MasterBlaster_artefacts\Release\VST3\MasterBlaster.vst3"
set "VST3_DLL=%VST3_BUNDLE%\Contents\x86_64-win\MasterBlaster.vst3"

if not exist "%VST3_DLL%" (
    echo [ERROR] VST3 DLL not found. Expected:
    echo         %VST3_DLL%
    echo.
    echo         Check the build output above for errors.
    pause & exit /b 1
)
echo [OK] Plugin built successfully.

:: ── Step 2: Stage payload ─────────────────────────────────────────────────────
echo.
echo [3/4] Staging plugin payload for installer...

:: Clean and recreate payload directory
if exist installer\payload rmdir /s /q installer\payload
xcopy /E /I /Q "%VST3_BUNDLE%" "installer\payload\MasterBlaster.vst3\"

if errorlevel 1 (
    echo [ERROR] Failed to stage payload.
    pause & exit /b 1
)
echo [OK] Payload staged.

:: ── Step 3: Build installer ───────────────────────────────────────────────────
echo.
echo [4/4] Building self-contained installer...
echo.

if not exist installer\dist mkdir installer\dist
if not exist build-installer mkdir build-installer

cmake -B build-installer -S installer -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo [ERROR] Installer CMake configure failed.
    pause & exit /b 1
)

cmake --build build-installer --config Release --parallel %NCPU%

if errorlevel 1 (
    echo [ERROR] Installer compilation failed.
    pause & exit /b 1
)

:: ── Done ─────────────────────────────────────────────────────────────────────
set "SETUP=installer\dist\MasterBlaster-1.1.0-Setup.exe"

if not exist "%SETUP%" (
    echo [WARN] Installer not found at expected path.
    pause & exit /b 1
)

for %%A in ("%SETUP%") do set "SIZE=%%~zA"
set /a "SIZE_MB=!SIZE! / 1048576"

echo.
echo ============================================================
echo   BUILD COMPLETE
echo.
echo   Plugin:    %VST3_DLL%
echo   Installer: %SETUP%  (~!SIZE_MB! MB)
echo.
echo   Distribute %SETUP% to users.
echo   They double-click it on Windows 10/11 — no prerequisites.
echo ============================================================
echo.
pause
endlocal
