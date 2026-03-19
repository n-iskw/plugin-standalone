cmake_minimum_required(VERSION 3.22)
project(PluginStandalone VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(PluginStandalone
    PRODUCT_NAME "Plugin Standalone"
    BUNDLE_ID    "com.example.pluginstandalone"
    NEEDS_MIDI_INPUT  TRUE
    NEEDS_MIDI_OUTPUT TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Audio input is needed to process guitar signal"
)

target_sources(PluginStandalone PRIVATE src/Main.cpp)

target_compile_definitions(PluginStandalone PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
)

target_link_libraries(PluginStandalone PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
)
