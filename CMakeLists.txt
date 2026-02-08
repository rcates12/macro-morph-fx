# MacroMorphFX — Top-level CMake configuration
# =============================================
# Build:
#   cmake -B build -G "Visual Studio 17 2022"
#   cmake --build build --config Release

cmake_minimum_required(VERSION 3.22)

project(MacroMorphFX VERSION 0.1.0)

# Add JUCE (cloned as a git submodule in /JUCE)
add_subdirectory(JUCE)

# --- JUCE AudioPluginHost (for testing) -----------------------------------
add_subdirectory(JUCE/extras/AudioPluginHost)

# --- Plugin target --------------------------------------------------------

juce_add_plugin(MacroMorphFX
    COMPANY_NAME            "MacroMorph"
    PLUGIN_MANUFACTURER_CODE Mmfx          # 4-char manufacturer code (at least one uppercase)
    PLUGIN_CODE              Mm01          # 4-char unique plugin code (exactly one uppercase)
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE         # Set TRUE to auto-install (needs admin PowerShell)
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "MacroMorphFX"
)

# --- Source files ---------------------------------------------------------

target_sources(MacroMorphFX
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# --- Preprocessor definitions ---------------------------------------------

target_compile_definitions(MacroMorphFX
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# --- Link JUCE modules ---------------------------------------------------

target_link_libraries(MacroMorphFX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

