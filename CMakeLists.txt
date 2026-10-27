cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime so the .vst3 works on machines without the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(HeavyChop VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout in deps/JUCE if present, otherwise download it
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/JUCE/CMakeLists.txt")
    add_subdirectory(deps/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(HeavyChop
    COMPANY_NAME "HeavyChop"
    PLUGIN_MANUFACTURER_CODE Hvch
    PLUGIN_CODE Hch1
    FORMATS VST3 Standalone
    PRODUCT_NAME "HeavyChop"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE          # MIDI notes can retrigger the slow->fast ramp
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE  # set TRUE to auto-install to C:/Program Files/Common Files/VST3 (needs admin)
    VST3_CATEGORIES Fx Dynamics Distortion)

target_sources(HeavyChop PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(HeavyChop PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(HeavyChop
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(HEAVYCHOP_TESTS "Build offline render test" OFF)
if(HEAVYCHOP_TESTS)
    add_executable(RenderTest Tests/RenderTest.cpp)
    target_link_libraries(RenderTest PRIVATE HeavyChop)
    target_include_directories(RenderTest PRIVATE $<TARGET_PROPERTY:HeavyChop,INCLUDE_DIRECTORIES>)
    target_compile_definitions(RenderTest PRIVATE $<TARGET_PROPERTY:HeavyChop,COMPILE_DEFINITIONS>)
endif()
