@echo off
rem EIDOLON - one-step Windows x64 build (Visual Studio 2022 Build Tools + CMake + Git required)
rem 1. verifies the environment  2. configures  3. builds Release  4. produces the VST3
rem 5. reports errors clearly    6. verifies the output (and builds setup.exe if Inno Setup 6 is installed)
setlocal
set ROOT=%~dp0
set BUILD=%ROOT%build-win
set VST3=%BUILD%\Eidolon_artefacts\Release\VST3\EIDOLON.vst3

echo [EIDOLON] Checking build environment...
where cmake >nul 2>nul || (echo [EIDOLON] ERROR: CMake not found. Install CMake 3.22+ or use the "x64 Native Tools Command Prompt for VS 2022". & exit /b 1)
where git   >nul 2>nul || (echo [EIDOLON] ERROR: Git not found. It is needed to download JUCE 8.0.4 on the first build. & exit /b 1)

echo [EIDOLON] Configuring (first run downloads JUCE 8.0.4)...
cmake -S "%ROOT%." -B "%BUILD%" -G "Visual Studio 17 2022" -A x64 -DEIDOLON_BUILD_TESTS=ON -DEIDOLON_ENABLE_OPENGL=ON
if errorlevel 1 (echo [EIDOLON] ERROR: CMake configure failed. Is "Desktop development with C++" installed in Visual Studio 2022? & exit /b 1)

echo [EIDOLON] Building Release...
cmake --build "%BUILD%" --config Release --target Eidolon_VST3 eidolon_core_tests --parallel
if errorlevel 1 (echo [EIDOLON] ERROR: build failed - see the compiler messages above. & exit /b 1)

echo [EIDOLON] Running core tests...
"%BUILD%\Release\eidolon_core_tests.exe" "%BUILD%\core_test_results.md"
if errorlevel 1 (echo [EIDOLON] ERROR: core tests FAILED - see %BUILD%\core_test_results.md & exit /b 1)

if not exist "%VST3%\Contents\x86_64-win\EIDOLON.vst3" (echo [EIDOLON] ERROR: expected plugin binary not found in %VST3% & exit /b 1)
echo [EIDOLON] Plugin bundle OK: %VST3%

set ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe
if exist "%ISCC%" (
    echo [EIDOLON] Building installer...
    "%ISCC%" "%ROOT%installer\EIDOLON.iss" "/DPluginDir=%VST3%" || exit /b 1
    echo [EIDOLON] Installer: %ROOT%dist\EIDOLON-1.0.0-Setup.exe
) else (
    echo [EIDOLON] Inno Setup 6 not found - skipping setup.exe. Install it from jrsoftware.org to also get an installer.
)

echo.
echo [EIDOLON] Done. Run install_windows.bat as administrator to copy the plugin to the system VST3 folder,
echo           or run dist\EIDOLON-1.0.0-Setup.exe.
endlocal
