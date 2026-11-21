@echo off
rem EIDOLON - copies the built VST3 bundle to the system VST3 folder. Run as administrator.
setlocal
set SRC=%~dp0build-win\Eidolon_artefacts\Release\VST3\EIDOLON.vst3
set DST=%CommonProgramFiles%\VST3\EIDOLON.vst3
if not exist "%SRC%" (echo [EIDOLON] Build first: build_windows.bat & exit /b 1)
net session >nul 2>&1 || (echo [EIDOLON] Please run this script as administrator. & exit /b 1)
if exist "%DST%" rmdir /s /q "%DST%"
xcopy "%SRC%" "%DST%\" /e /i /y >nul || (echo [EIDOLON] Copy failed. & exit /b 1)
echo [EIDOLON] Installed to %DST%
echo           In FL Studio: Options ^> Manage plugins ^> Find installed plugins.
endlocal
