cmake_minimum_required(VERSION 3.22)

# CMP0091 must be set BEFORE project() so CMAKE_MSVC_RUNTIME_LIBRARY works
cmake_policy(SET CMP0091 NEW)

project(MasterBlaster VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# ── Static CRT on MSVC (no vcruntime140.dll dependency for end users) ─────────
# Must be set before any FetchContent / add_subdirectory that brings in JUCE,
# so every target in the build uses the same runtime heap (required by JUCE).
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ── JUCE via FetchContent ─────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(MasterBlaster
    COMPANY_NAME                "CircuitBurn"
    COMPANY_WEBSITE             "https://circuitburn.audio"
    BUNDLE_ID                   "com.circuitburn.masterblaster"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE   # installer handles deployment
    PLUGIN_MANUFACTURER_CODE    Cbur
    PLUGIN_CODE                 Mblr
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "MasterBlaster"
    DESCRIPTION                 "Circuit-burn mastering chain: LP-EQ, MB-Comp, Saturation, M/S Widener, True Peak Limiter"
    VST3_CATEGORIES             "Fx" "Mastering"
)

juce_generate_juce_header(MasterBlaster)

target_sources(MasterBlaster
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/LinearPhaseEQ.cpp
        Source/DSP/MultibandCompressor.cpp
        Source/DSP/HarmonicSaturator.cpp
        Source/DSP/StereoWidener.cpp
        Source/DSP/TruePeakLimiter.cpp
        Source/DSP/Dithering.cpp
        Source/DSP/LUFSMeter.cpp
        Source/UI/CustomLookAndFeel.cpp
        Source/UI/PlatformSelector.cpp
        Source/UI/LUFSDisplay.cpp
        Source/UI/GutsPanel.cpp
)

target_include_directories(MasterBlaster PRIVATE Source)

target_compile_definitions(MasterBlaster
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_USE_MP3AUDIOFORMAT=1
)

# ── Platform compiler flags ───────────────────────────────────────────────────
if(MSVC)
    # No /arch:AVX2 — must run on all x64 CPUs (Haswell+ not guaranteed).
    # SSE2 is always available on x64, /fp:fast is safe for audio DSP.
    target_compile_options(MasterBlaster PRIVATE /W3 /O2 /fp:fast)
endif()

if(APPLE)
    target_compile_options(MasterBlaster PRIVATE -O2 -ffast-math -march=native)
    set_target_properties(MasterBlaster PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
    )
    if(TARGET MasterBlaster_AU)
        set_target_properties(MasterBlaster_AU PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
            XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO
        )
    endif()
endif()

if(UNIX AND NOT APPLE)
    target_compile_options(MasterBlaster PRIVATE -O2 -ffast-math)
endif()

# ── Link ──────────────────────────────────────────────────────────────────────
target_link_libraries(MasterBlaster
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
