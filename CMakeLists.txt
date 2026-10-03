# ÁCIDO — acid bass synth plugin
cmake_minimum_required(VERSION 3.22)

# Build for both Apple Silicon and Intel Macs, macOS 11 or newer.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac architectures")

project(acido VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download JUCE automatically the first time you configure.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(acido
    COMPANY_NAME              "ZOONIDO"   # folder shown in Ableton
    BUNDLE_ID                 com.zoonido.acido
    PRODUCT_NAME              "acido"     # file name: acido.vst3
    PLUGIN_NAME               "ACIDO"     # name shown in Ableton
    PLUGIN_MANUFACTURER_CODE  Zoon
    PLUGIN_CODE               Acd1
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE     # GitHub builds it; you copy the downloaded plugin yourself
    FORMATS                   VST3)

# Fonts from the mockup (SIL Open Font License, see assets/fonts), built into the plugin.
juce_add_binary_data(acido_fonts SOURCES
    assets/fonts/ChakraPetch-Bold.ttf
    assets/fonts/ChakraPetch-SemiBold.ttf
    assets/fonts/IBMPlexMono-Regular.ttf
    assets/fonts/IBMPlexMono-Medium.ttf)

target_sources(acido PRIVATE
    source/plugin_processor.cpp
    source/plugin_editor.cpp
    source/preset_manager.cpp)

target_include_directories(acido PRIVATE source)

target_compile_definitions(acido PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0   # stops Ableton showing ~2,000 hidden MIDI CC parameters
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(acido
    PRIVATE
        acido_fonts
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
