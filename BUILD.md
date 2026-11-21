# Building EIDOLON

## Windows x64 (target platform)

### Requirements
* **Visual Studio 2022** (Community or Build Tools) with the **Desktop development with C++** workload (MSVC v143,
  Windows 10/11 SDK)
* **CMake 3.22+** (the one bundled with Visual Studio works from the *x64 Native Tools Command Prompt*)
* **Git** (CMake downloads JUCE 8.0.4 on the first configure)
* *Optional:* **Inno Setup 6** (to produce `setup.exe`)

No other third-party libraries are needed. Internet access is only needed for the first configure (JUCE download).

### One step
```bat
build_windows.bat
```
The script checks the tools, configures, builds Release, runs the core tests, verifies the VST3 bundle and — when
Inno Setup 6 is installed — builds `dist\EIDOLON-1.0.0-Setup.exe`.

### Manual
```bat
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 -DEIDOLON_ENABLE_OPENGL=ON
cmake --build build-win --config Release --target Eidolon_VST3 eidolon_core_tests --parallel
build-win\Release\eidolon_core_tests.exe
"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" installer\EIDOLON.iss /DPluginDir=%CD%\build-win\Eidolon_artefacts\Release\VST3\EIDOLON.vst3
```
Output: `build-win\Eidolon_artefacts\Release\VST3\EIDOLON.vst3` and `dist\EIDOLON-1.0.0-Setup.exe`.
The plugin links the static MSVC runtime, so no VC++ redistributable is required.

### Install without the installer
Run `install_windows.bat` as administrator, or copy the `EIDOLON.vst3` folder to
`C:\Program Files\Common Files\VST3\`.

## CI (GitHub Actions)
`.github/workflows/build-windows.yml` builds on `windows-2022`: configure → build → core tests → pluginval
(strictness 5) → Inno Setup → uploads artifacts and force-pushes `EIDOLON-1.0.0-Setup.exe` to the `binaries` branch.

## Linux validation build (used for development testing)
```bash
sudo apt install build-essential cmake ninja-build libx11-dev libfreetype-dev libfontconfig1-dev xvfb
# headless-friendly X11 subset (CXXFLAGS also reaches JUCE's juceaide helper)
export CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DEIDOLON_BUILD_HOST_TEST=ON
ninja -C build Eidolon_VST3 eidolon_core_tests eidolon_host_test
./build/eidolon_core_tests
xvfb-run -a -s "-screen 0 1920x1200x24" ./build/eidolon_host_test_artefacts/Release/eidolon_host_test \
    build/Eidolon_artefacts/Release/VST3/EIDOLON.vst3 test-output
```
Set `-DEIDOLON_JUCE_PATH=/path/to/JUCE` to use a local JUCE 8 checkout instead of downloading it.

## CMake options
| Option | Default | Meaning |
|---|---|---|
| `EIDOLON_ENABLE_OPENGL` | ON on Windows, OFF elsewhere | GPU canvas through `juce::OpenGLContext` (CPU fallback built in) |
| `EIDOLON_BUILD_TESTS` | ON | JUCE-free core test suite |
| `EIDOLON_BUILD_HOST_TEST` | OFF | VST3 host validation tool (loads the built `.vst3`) |
| `EIDOLON_BUILD_STANDALONE` | OFF | also build a standalone app |
| `EIDOLON_JUCE_PATH` | empty | local JUCE checkout |

## Troubleshooting
* **"Failed to build juceaide"** — JUCE's helper tool did not compile: on Windows the C++ workload is missing; on
  Linux export the `CXXFLAGS` above or install the X11 extension headers.
* **Plugin not found in FL Studio** — re-scan (Options → Manage plugins → Find installed plugins) and make sure the
  bundle is in `C:\Program Files\Common Files\VST3\`.
* **Black canvas** — open the Render tab and turn *GPU acceleration* off (the watchdog normally does this by itself).
