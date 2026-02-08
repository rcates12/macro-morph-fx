#pragma once

#include "PluginProcessor.h"
#include "PresetData.h"

/**
 * ============================================================================
 *  MacroMorphFX — Custom Performance UI
 * ============================================================================
 *
 *  Layout (top to bottom):
 *    Header:          Title + Preset selector + Bypass toggle
 *    Scene A row:     8 scene-select buttons
 *    Morph:           Large horizontal slider
 *    Scene B row:     8 scene-select buttons
 *    Macro row:       4 rotary knobs (Filter Sweep, Dirt, Space, Width)
 *    Bottom row:      Mix, In Gain, Out Gain knobs + Store buttons
 *    Toggle bar:      [MODULES] [MACRO CONFIG]
 *    Module panel:    Collapsible — read-only display of current DSP values
 *    Macro config:    Collapsible — edit macro target/amount mappings
 *
 *  Lane C — UI only (PluginEditor.*)
 * ============================================================================
 */
class MacroMorphFXEditor final : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit MacroMorphFXEditor (MacroMorphFXProcessor&);
    ~MacroMorphFXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // ── Timer ──────────────────────────────────────────────────────────
    void timerCallback() override;

    // ── Helpers ────────────────────────────────────────────────────────
    void sceneButtonClicked (bool isSceneA, int index);
    void updateSceneHighlights();
    void storeToScene (bool isSceneA);
    void toggleModulePanel();
    void refreshModuleSliders();
    void toggleEditTarget();
    void onModuleSliderChanged (int paramIdx);
    void toggleMacroConfig();
    void updateMacroConfigDisplay();
    void onMacroSlotChanged (int macroIdx);
    void onPresetChanged();
    void onSavePreset();
    void onLoadPreset();

    int computeTotalHeight() const;

    MacroMorphFXProcessor& processorRef;

    // ── LookAndFeel ────────────────────────────────────────────────────
    juce::LookAndFeel_V4 lnf;

    // ── Typedefs ───────────────────────────────────────────────────────
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // ── Header ─────────────────────────────────────────────────────────
    juce::Label titleLabel;
    juce::ToggleButton bypassButton { "BYPASS" };
    std::unique_ptr<ButtonAttach> bypassAttach;
    juce::ComboBox presetSelector_;
    juce::TextButton savePresetBtn_ { "Save" };
    juce::TextButton loadPresetBtn_ { "Load" };

    // ── Scene buttons ──────────────────────────────────────────────────
    juce::Label sceneALabel;
    juce::Label sceneBLabel;
    std::array<juce::TextButton, 8> sceneABtns;
    std::array<juce::TextButton, 8> sceneBBtns;

    // ── Morph slider ───────────────────────────────────────────────────
    juce::Slider morphSlider;
    juce::Label  morphLabel;
    std::unique_ptr<SliderAttach> morphAttach;

    // ── Macro knobs ────────────────────────────────────────────────────
    std::array<juce::Slider, 4> macroSliders;
    std::array<juce::Label, 4>  macroNumLabels;
    std::array<juce::Label, 4>  macroNameLabels;
    std::array<std::unique_ptr<SliderAttach>, 4> macroAttach;

    // ── Performance knobs ──────────────────────────────────────────────
    juce::Slider mixSlider;
    juce::Slider inGainSlider;
    juce::Slider outGainSlider;
    juce::Label  mixLabel;
    juce::Label  inGainLabel;
    juce::Label  outGainLabel;
    std::unique_ptr<SliderAttach> mixAttach;
    std::unique_ptr<SliderAttach> inGainAttach;
    std::unique_ptr<SliderAttach> outGainAttach;

    // ── Store buttons ──────────────────────────────────────────────────
    juce::TextButton storeABtn;
    juce::TextButton storeBBtn;
    int storeFlashA_ = 0;
    int storeFlashB_ = 0;

    // ── Module panel (collapsible + editable) ──────────────────────────
    bool modulePanelOpen_ = false;
    juce::TextButton modulePanelToggle;
    juce::Label moduleHeaders[4];
    std::array<juce::Slider, SceneParam::kCount> moduleSliders_;
    std::array<juce::Label, SceneParam::kCount>  moduleParamLabels_;
    bool editTargetIsA_ = true;
    juce::TextButton editTargetBtn_ { "EDIT: A" };

    // ── Macro config panel (collapsible) ───────────────────────────────
    bool macroConfigOpen_ = false;
    juce::TextButton macroConfigToggle_;
    juce::Label macroConfigHeaders_[4];

    static constexpr int kMaxMacroTargets = 4;
    struct MacroSlot
    {
        juce::ComboBox paramSelector;
        juce::Slider   amountSlider;
        juce::ComboBox curveSelector;
    };
    MacroSlot macroSlots_[4][kMaxMacroTargets];

    // ── Layout constants ───────────────────────────────────────────────
    static constexpr int kCollapsedHeight    = 500;
    static constexpr int kModulePanelHeight  = 140;
    static constexpr int kMacroConfigHeight  = 165;

    // ── Colours ────────────────────────────────────────────────────────
    static inline const juce::Colour colBg        { 0xff0d1117 };
    static inline const juce::Colour colPanel     { 0xff161b22 };
    static inline const juce::Colour colAccent    { 0xff2f81f7 };
    static inline const juce::Colour colAccentDim { 0xff1a4a8a };
    static inline const juce::Colour colText      { 0xffe6edf3 };
    static inline const juce::Colour colTextDim   { 0xff8b949e };
    static inline const juce::Colour colBtnNorm   { 0xff21262d };
    static inline const juce::Colour colBtnActive { 0xff2f81f7 };
    static inline const juce::Colour colDivider   { 0xff30363d };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroMorphFXEditor)
};
