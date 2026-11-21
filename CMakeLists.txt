cmake_minimum_required(VERSION 3.22)

project(EIDOLON VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(EIDOLON_BUILD_TESTS      "Build the JUCE-free core test suite"                 ON)
option(EIDOLON_BUILD_HOST_TEST  "Build the JUCE VST3 host validation tool"            OFF)
option(EIDOLON_BUILD_STANDALONE "Also build a Standalone app (needs audio devices)"   OFF)
if (WIN32)
    set(_eidolon_gl_default ON)
else()
    set(_eidolon_gl_default OFF)
endif()
option(EIDOLON_ENABLE_OPENGL "GPU (OpenGL) accelerated canvas, with CPU fallback" ${_eidolon_gl_default})
set(EIDOLON_JUCE_PATH "" CACHE PATH "Path to a local JUCE 8 checkout (empty = download JUCE 8.0.4)")

if (MSVC)
    # Static runtime: the .vst3 does not depend on a VC++ redistributable being installed.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------------------- JUCE
if (EIDOLON_JUCE_PATH)
    add_subdirectory(${EIDOLON_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------- musical core (no JUCE)
# MIDI engine, musical intelligence, mapping, capture/SMF export, Visual DNA + mutation.
add_library(eidolon_core STATIC
    Source/Core/Params.cpp
    Source/MIDI/NoteTracker.cpp
    Source/Intelligence/ChordDetector.cpp
    Source/Intelligence/ScaleDetector.cpp
    Source/Intelligence/VoiceTracker.cpp
    Source/Intelligence/RhythmAnalyzer.cpp
    Source/Intelligence/MusicalStatistics.cpp
    Source/Intelligence/MusicalAnalyzer.cpp
    Source/Mapping/MappingEngine.cpp
    Source/Capture/CaptureEngine.cpp
    Source/Capture/MidiFileWriter.cpp
    Source/DNA/MutationEngine.cpp
    Source/DNA/FactoryPresets.cpp)
target_include_directories(eidolon_core PUBLIC Source)
target_compile_features(eidolon_core PUBLIC cxx_std_20)
set_target_properties(eidolon_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if (MSVC)
    target_compile_options(eidolon_core PRIVATE /W4 /utf-8 /wd4244 /wd4267)
else()
    target_compile_options(eidolon_core PRIVATE -Wall -Wextra -O2)
endif()

# ---------------------------------------------------------------------------- fonts
juce_add_binary_data(EidolonBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Resources/Fonts/InterLight.otf
        Resources/Fonts/InterRegular.otf
        Resources/Fonts/InterMedium.otf
        Resources/Fonts/DejaVuSansMono.ttf)
set_target_properties(EidolonBinaryData PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------- plugin
set(EIDOLON_FORMATS VST3)
if (EIDOLON_BUILD_STANDALONE)
    list(APPEND EIDOLON_FORMATS Standalone)
endif()

# A visualizer must receive MIDI from the host. In FL Studio, generators (Channel Rack instruments)
# receive Piano Roll / controller notes directly, so EIDOLON is an instrument-type plugin with
# MIDI in + MIDI thru out and a silent stereo output. See Docs/FL_STUDIO_ROUTING.md.
juce_add_plugin(Eidolon
    COMPANY_NAME              "EIDOLON Lab"
    COMPANY_WEBSITE           ""
    BUNDLE_ID                 "com.eidolonlab.eidolon"
    PLUGIN_MANUFACTURER_CODE  Eidl
    PLUGIN_CODE               Eid1
    PRODUCT_NAME              "EIDOLON"
    VERSION                   ${PROJECT_VERSION}
    FORMATS                   ${EIDOLON_FORMATS}
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         TRUE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Instrument Tools)

target_sources(Eidolon PRIVATE
    Source/Plugin/PluginProcessor.cpp
    Source/Engine/EidolonCore.cpp
    Source/DNA/DNASerializer.cpp
    Source/DNA/PresetLibrary.cpp
    Source/Rendering/Camera.cpp
    Source/Rendering/Geometry.cpp
    Source/Rendering/RenderContext.cpp
    Source/Rendering/ParticleSystem.cpp
    Source/Rendering/Palette.cpp
    Source/Visualization/VisualizationEngine.cpp
    Source/Visualization/PianoUniverse.cpp
    Source/Visualization/Constellation.cpp
    Source/Visualization/Flow.cpp
    Source/Visualization/Pulse.cpp
    Source/Visualization/Spectrum.cpp
    Source/Visualization/Orbit.cpp
    Source/Scenes/SceneManager.cpp
    Source/Scenes/SceneRenderers.cpp
    Source/UI/Theme.cpp
    Source/UI/Controls.cpp
    Source/UI/ParamPanel.cpp
    Source/UI/MappingPanel.cpp
    Source/UI/DNAPanel.cpp
    Source/UI/CapturePanel.cpp
    Source/UI/AnalysisPanel.cpp
    Source/UI/Dashboard.cpp
    Source/UI/VisualizationCanvas.cpp
    Source/UI/PluginEditor.cpp)

target_compile_definitions(Eidolon PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0)

if (MSVC)
    target_compile_options(Eidolon PRIVATE /utf-8)
endif()

if (UNIX AND NOT APPLE)
    # Linux validation build: headless-friendly X11 subset.
    target_compile_definitions(Eidolon PUBLIC JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

target_link_libraries(Eidolon
    PRIVATE
        eidolon_core
        EidolonBinaryData
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (EIDOLON_ENABLE_OPENGL)
    target_link_libraries(Eidolon PRIVATE juce::juce_opengl)
    target_compile_definitions(Eidolon PRIVATE EIDOLON_OPENGL=1)
else()
    target_compile_definitions(Eidolon PRIVATE EIDOLON_OPENGL=0)
endif()

# ---------------------------------------------------------------------------- tests
if (EIDOLON_BUILD_TESTS)
    add_executable(eidolon_core_tests Tests/CoreTests.cpp)
    target_link_libraries(eidolon_core_tests PRIVATE eidolon_core)
    if (MSVC)
        target_compile_options(eidolon_core_tests PRIVATE /utf-8)
    endif()
    enable_testing()
    add_test(NAME eidolon_core_tests COMMAND eidolon_core_tests)
endif()

if (EIDOLON_BUILD_HOST_TEST)
    juce_add_console_app(eidolon_host_test PRODUCT_NAME "eidolon_host_test")
    target_sources(eidolon_host_test PRIVATE Tests/HostTest.cpp)
    target_compile_definitions(eidolon_host_test PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(eidolon_host_test PRIVATE
        juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags)
    if (UNIX AND NOT APPLE)
        target_link_libraries(eidolon_host_test PRIVATE X11)
    endif()
endif()
