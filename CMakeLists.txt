cmake_minimum_required(VERSION 3.22)
project(raspa VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(raspa
    COMPANY_NAME "ZOONIDO"
    PLUGIN_MANUFACTURER_CODE Zoon
    PLUGIN_CODE Rasp
    FORMATS VST3
    PRODUCT_NAME "raspa"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum)

target_sources(raspa PRIVATE
    source/plugin_processor.cpp
    source/plugin_editor.cpp)

target_include_directories(raspa PRIVATE source)

target_compile_definitions(raspa PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(raspa
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

# Optional: a tool that draws the plugin window to a PNG (used for screenshots, not needed on the Mac)
option(RASPA_SNAPSHOT "Build the window snapshot tool" OFF)
if(RASPA_SNAPSHOT)
    juce_add_console_app(raspa_snapshot PRODUCT_NAME "raspa_snapshot")
    target_sources(raspa_snapshot PRIVATE tools/snapshot.cpp source/plugin_processor.cpp source/plugin_editor.cpp)
    target_include_directories(raspa_snapshot PRIVATE source)
    target_compile_definitions(raspa_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 RASPA_NO_PLUGIN_ENTRY=1)
    target_link_libraries(raspa_snapshot PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
endif()
