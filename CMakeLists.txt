cmake_minimum_required(VERSION 3.22)

project(Quirk VERSION 0.1.0)

add_subdirectory(JUCE)

# --- DSP Library (shared between plugin and tests) ---
add_library(QuirkDSP STATIC
    Source/DSP/Oscillator.cpp
    Source/DSP/BezierCurve.cpp
)

target_include_directories(QuirkDSP PUBLIC Source)

target_link_libraries(QuirkDSP
    PUBLIC
        juce::juce_dsp
)

target_compile_definitions(QuirkDSP
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

# --- Plugin ---
juce_add_plugin(Quirk
    COMPANY_NAME "conjius"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Cjus
    PLUGIN_CODE Qrks
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "quirk"
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(Quirk
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/KnobDesign.h
)

juce_add_binary_data(QuirkBinaryData
    SOURCES
        Assets/conjius-avatar-transparent-bg.png
        Assets/quirk-logo-transparent-bg.png
        Assets/Inconsolata-Bold.ttf
        Assets/Inconsolata-Regular.ttf)

target_compile_definitions(Quirk
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Quirk
    PRIVATE
        QuirkDSP
        QuirkBinaryData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_options(Quirk PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -ffunction-sections
        -fdata-sections>)

target_link_options(Quirk PRIVATE
    $<$<PLATFORM_ID:Darwin>:-Wl,-dead_strip -Wl,-x>
    $<$<PLATFORM_ID:Linux>:-Wl,--gc-sections -Wl,--strip-all>)

if(MSVC)
    target_link_options(Quirk PRIVATE /OPT:REF /OPT:ICF)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR NOT CMAKE_BUILD_TYPE)
    foreach(kind VST3 AU Standalone)
        if(TARGET Quirk_${kind})
            if(APPLE)
                add_custom_command(TARGET Quirk_${kind} POST_BUILD
                    COMMAND ${CMAKE_STRIP} -x -S
                        $<TARGET_FILE:Quirk_${kind}>
                    COMMAND codesign --force --sign - --timestamp=none
                        $<TARGET_BUNDLE_DIR:Quirk_${kind}>
                    COMMENT "Stripping + re-signing Quirk_${kind}")
            elseif(UNIX)
                add_custom_command(TARGET Quirk_${kind} POST_BUILD
                    COMMAND ${CMAKE_STRIP} --strip-all
                        $<TARGET_FILE:Quirk_${kind}>
                    COMMENT "Stripping Quirk_${kind}")
            endif()
        endif()
    endforeach()
endif()

# --- Tests ---
add_executable(QuirkTests
    Tests/TestBezierCurve.cpp
)

target_include_directories(QuirkTests PRIVATE Source)

target_link_libraries(QuirkTests
    PRIVATE
        QuirkDSP
        juce::juce_core
)

target_compile_definitions(QuirkTests
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
)
