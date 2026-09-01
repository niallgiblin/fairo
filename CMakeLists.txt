cmake_minimum_required(VERSION 3.22)
# Fairo — Pharaoh-style fuzz pedal emulation (VST3/AU/Standalone).
# Scaffold conventions (targets, options, JUCE version) inherited from the
# author's fuzzyband project, per PLAN.md Phase 0.
project(Fairo VERSION 0.4.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─────────────────────────────────────────────────────────────────
# Options
# ─────────────────────────────────────────────────────────────────

option(FA_BUILD_TESTS       "Build unit tests"                ON)
option(FA_BUILD_STANDALONE  "Build standalone app target"     ON)
# Parked Phase 3 inference: compiles NeuralTests only. The plugin is DSP-only
# regardless of this flag (models + ONNX Runtime never ship in the binary).
option(FA_ENABLE_ONNX       "Build parked neural inference tests (default OFF)" OFF)

# ─────────────────────────────────────────────────────────────────
# JUCE — fetch if not found locally (8.0.10, same as fuzzyband)
# Point FETCHCONTENT_SOURCE_DIR_JUCE at an existing checkout to skip the clone:
#   cmake -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/juce ..
# ─────────────────────────────────────────────────────────────────

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.10
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ─────────────────────────────────────────────────────────────────
# Catch2 (tests only)
# ─────────────────────────────────────────────────────────────────

if(FA_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# ─────────────────────────────────────────────────────────────────
# ONNX Runtime (parked Phase 3 inference tests only — not the plugin)
# ─────────────────────────────────────────────────────────────────

if(FA_ENABLE_ONNX)
    if(NOT DEFINED ONNXRUNTIME_ROOT)
        message(FATAL_ERROR "FA_ENABLE_ONNX is ON but ONNXRUNTIME_ROOT is not set.")
    endif()
    set(ONNX_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/include")
    if(APPLE)
        set(ONNX_LIB "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.dylib")
    elseif(UNIX)
        set(ONNX_LIB "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so")
    elseif(WIN32)
        set(ONNX_LIB "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib")
    endif()
    message(STATUS "ONNX Runtime enabled: ${ONNX_LIB}")
endif()

# ─────────────────────────────────────────────────────────────────
# Plugin target
# ─────────────────────────────────────────────────────────────────

set(PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()
if(FA_BUILD_STANDALONE)
    list(APPEND PLUGIN_FORMATS Standalone)
endif()

juce_add_plugin(Fairo
    VERSION                     ${PROJECT_VERSION}
    COMPANY_NAME                "Niall Giblin"
    BUNDLE_ID                   com.ng.Fairo
    PLUGIN_MANUFACTURER_CODE    "NGFR"
    PLUGIN_CODE                 "FaRo"
    FORMATS                     ${PLUGIN_FORMATS}
    PRODUCT_NAME                "Fairo"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Fx" "Distortion"
    AU_MAIN_TYPE                "kAudioUnitType_MusicEffect"
    DESCRIPTION                 "Pharaoh-style fuzz: switchable clipping, coupled Tone/High stack"
)

juce_generate_juce_header(Fairo)

# ─────────────────────────────────────────────────────────────────
# BinaryData — UI background + bundled OFL fonts (plugin).
# ONNX models stay test-only.
# ─────────────────────────────────────────────────────────────────

juce_add_binary_data(FairoAssets
    SOURCES
        "${CMAKE_SOURCE_DIR}/assets/coast.jpg"
        "${CMAKE_SOURCE_DIR}/assets/fonts/Rajdhani-Medium.ttf"
        "${CMAKE_SOURCE_DIR}/assets/fonts/Rajdhani-Bold.ttf"
        "${CMAKE_SOURCE_DIR}/assets/fonts/SpaceMono-Regular.ttf"
        "${CMAKE_SOURCE_DIR}/assets/fonts/SpaceMono-Bold.ttf"
)

# ─────────────────────────────────────────────────────────────────
# Source files
# ─────────────────────────────────────────────────────────────────

target_sources(Fairo PRIVATE
    # Plugin entry points
    src/FairoProcessor.cpp
    src/FairoEditor.cpp

    # DSP
    src/dsp/DiodeClipper.cpp
    src/dsp/RcNetwork.cpp
    src/dsp/ToneStack.cpp
    src/dsp/FuzzEngine.cpp
)

# ─────────────────────────────────────────────────────────────────
# Include paths
# ─────────────────────────────────────────────────────────────────

target_include_directories(Fairo PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}/Fairo_JuceHeader"
)

# ─────────────────────────────────────────────────────────────────
# Compile definitions
# ─────────────────────────────────────────────────────────────────

target_compile_definitions(Fairo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    FA_PLUGIN_VERSION="${PROJECT_VERSION}"
)

# ─────────────────────────────────────────────────────────────────
# Compiler flags (same policy as fuzzyband: -ffast-math is safe for
# this DSP and only affects the plugin + tests, not JUCE internals)
# ─────────────────────────────────────────────────────────────────

if(APPLE)
    target_compile_options(Fairo PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -ffast-math
        -O3
    )
elseif(UNIX)
    target_compile_options(Fairo PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -ffast-math
        -O3
    )
elseif(WIN32)
    target_compile_options(Fairo PRIVATE
        /W4 /O2 /fp:fast
    )
endif()

# ─────────────────────────────────────────────────────────────────
# Link libraries
# ─────────────────────────────────────────────────────────────────

set(JUCE_MODULES
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

target_link_libraries(Fairo PRIVATE
    ${JUCE_MODULES}
    FairoAssets
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ─────────────────────────────────────────────────────────────────
# Tests
# ─────────────────────────────────────────────────────────────────

if(FA_BUILD_TESTS)
    set(FAIRO_TEST_DSP_SOURCES
        tests/DspTests.cpp
        src/dsp/DiodeClipper.cpp
        src/dsp/RcNetwork.cpp
        src/dsp/ToneStack.cpp
        src/dsp/FuzzEngine.cpp
    )

    add_executable(FairoTests ${FAIRO_TEST_DSP_SOURCES})

    target_include_directories(FairoTests PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
    )

    target_compile_definitions(FairoTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        FA_PLUGIN_VERSION="${PROJECT_VERSION}"
        "FA_REPO_ROOT=\"${CMAKE_SOURCE_DIR}\""
    )

    if(FA_ENABLE_ONNX)
        juce_add_binary_data(FairoData SOURCES
            "${CMAKE_SOURCE_DIR}/assets/fuzz_silicon.onnx"
            "${CMAKE_SOURCE_DIR}/assets/fuzz_germanium.onnx"
            "${CMAKE_SOURCE_DIR}/assets/fuzz_bypass.onnx"
        )
        target_sources(FairoTests PRIVATE
            tests/NeuralTests.cpp
            src/inference/FuzzNeuralInference.cpp
        )
        target_compile_definitions(FairoTests PRIVATE FA_ENABLE_ONNX=1)
        target_include_directories(FairoTests PRIVATE
            "${ONNX_INCLUDE_DIR}" "${ONNX_INCLUDE_DIR}/onnxruntime"
            "${CMAKE_BINARY_DIR}/Fairo_JuceHeader")
        target_link_libraries(FairoTests PRIVATE "${ONNX_LIB}" FairoData)
        add_test(NAME fairo_neural_tests COMMAND FairoTests "[neural]")
        set_tests_properties(fairo_neural_tests PROPERTIES LABELS "unit;onnx")
        add_test(NAME fairo_neural_latency COMMAND FairoTests "[neural][latency][benchmark]")
        set_tests_properties(fairo_neural_latency PROPERTIES LABELS "onnx;benchmark")
    endif()

    if(APPLE)
        target_compile_options(FairoTests PRIVATE
            -Wall -Wextra -Wno-unused-parameter -ffast-math -O2)
    elseif(UNIX)
        target_compile_options(FairoTests PRIVATE
            -Wall -Wextra -Wno-unused-parameter -ffast-math -O2)
    elseif(WIN32)
        target_compile_options(FairoTests PRIVATE
            /W4 /O2 /fp:fast)
    endif()

    target_link_libraries(FairoTests PRIVATE
        Catch2::Catch2WithMain
        ${JUCE_MODULES}
    )

    include(CTest)
    add_test(NAME fairo_dsp_tests COMMAND FairoTests)
    set_tests_properties(fairo_dsp_tests PROPERTIES LABELS "unit")
endif()
