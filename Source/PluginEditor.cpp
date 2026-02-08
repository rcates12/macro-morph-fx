#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Unicode icon constants (platform-safe — avoids raw hex escapes that break on MSVC)
static const juce::String kIconCollapsed = juce::String::charToString (0x25B8);  // ▸
static const juce::String kIconExpanded  = juce::String::charToString (0x25BE);  // ▾
static const juce::String kIconArrow     = juce::String::charToString (0x2192);  // →

//==============================================================================
// Helpers

static juce::String svToString (std::string_view sv)
{
    return juce::String (sv.data(), sv.size());
}

static void setupRotaryKnob (juce::Slider& s, juce::Colour textCol)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 16);
    s.setColour (juce::Slider::textBoxTextColourId,       textCol);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
}

static void setupLabel (juce::Label& l, float fontSize,
                        juce::Justification just, juce::Colour colour)
{
    l.setFont (juce::FontOptions (fontSize));
    l.setJustificationType (just);
    l.setColour (juce::Label::textColourId, colour);
}

// Display names for scene parameters (indexed by SceneParam::Index)
static const char* const kParamDisplayNames[SceneParam::kCount] = {
    "Mode", "Cutoff", "Reso",               // Filter (3)
    "Amount", "Tone",                        // Drive (2)
    "Sync", "FB", "Tone", "Width", "PP",     // Delay (5)
    "Size", "Damp", "PDly", "Width"          // Reverb (4)
};

static juce::String formatSceneValue (int paramIndex, float value)
{
    switch (paramIndex)
    {
        case SceneParam::filtMode:
        {
            static const char* names[] = { "LP", "BP", "HP" };
            return names[std::clamp (static_cast<int> (value), 0, 2)];
        }
        case SceneParam::filtCutoff:
        {
            if (value >= 1000.0f)
                return juce::String (value / 1000.0f, 1) + " kHz";
            return juce::String (static_cast<int> (value)) + " Hz";
        }
        case SceneParam::delaySync:
        {
            static const char* names[] = { "1/32", "1/16", "1/8", "1/4", "1/2", "1 Bar", "1/8D", "1/4D" };
            return names[std::clamp (static_cast<int> (value), 0, 7)];
        }
        case SceneParam::delayPingP:
            return value > 0.5f ? "On" : "Off";
        case SceneParam::revPreDelay:
            return juce::String (value, 1) + " ms";
        default:
            return juce::String (value, 2);
    }
}

//==============================================================================
// Macro target options: continuous scene params only (discrete are skipped)

struct MacroTargetOption
{
    int sceneIdx;
    const char* name;
};

static const MacroTargetOption kMacroTargetOptions[] = {
    { SceneParam::filtCutoff,  "Cutoff"     },
    { SceneParam::filtReso,    "Reso"       },
    { SceneParam::driveAmt,    "Drive Amt"  },
    { SceneParam::driveTone,   "Drive Tone" },
    { SceneParam::delayFb,     "Delay FB"   },
    { SceneParam::delayTone,   "Delay Tone" },
    { SceneParam::delayWidth,  "Delay Width"},
    { SceneParam::revSize,     "Rev Size"   },
    { SceneParam::revDamp,     "Rev Damp"   },
    { SceneParam::revPreDelay, "Rev PDly"   },
    { SceneParam::revWidth,    "Rev Width"  },
};
static constexpr int kNumMacroTargetOptions = 11;

// Convert a SceneParam index to a ComboBox item ID (2..12), or 1 for "None"
static int sceneParamToComboId (int sceneParamIndex)
{
    for (int i = 0; i < kNumMacroTargetOptions; ++i)
        if (kMacroTargetOptions[i].sceneIdx == sceneParamIndex)
            return i + 2;
    return 1; // None
}

//==============================================================================
MacroMorphFXEditor::MacroMorphFXEditor (MacroMorphFXProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // ── Dark LookAndFeel ────────────────────────────────────────────────
    auto scheme = juce::LookAndFeel_V4::getDarkColourScheme();
    lnf.setColourScheme (scheme);
    setLookAndFeel (&lnf);

    // ── Title ───────────────────────────────────────────────────────────
    titleLabel.setText ("MACRO MORPH FX", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setColour (juce::Label::textColourId, colText);
    addAndMakeVisible (titleLabel);

    // ── Preset selector ─────────────────────────────────────────────────
    for (int i = 0; i < kNumFactoryPresets; ++i)
        presetSelector_.addItem (kFactoryPresetNames[i], i + 1);
    presetSelector_.setSelectedId (processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    presetSelector_.onChange = [this] { onPresetChanged(); };
    presetSelector_.setColour (juce::ComboBox::backgroundColourId, colPanel);
    presetSelector_.setColour (juce::ComboBox::textColourId,       colText);
    presetSelector_.setColour (juce::ComboBox::outlineColourId,    colDivider);
    presetSelector_.setTooltip ("Select a factory preset");
    addAndMakeVisible (presetSelector_);

    // ── Save / Load preset buttons ──────────────────────────────────────
    savePresetBtn_.setColour (juce::TextButton::buttonColourId,  colBtnNorm);
    savePresetBtn_.setColour (juce::TextButton::textColourOffId, colText);
    savePresetBtn_.setTooltip ("Save current state as a user preset file (.mmfx)");
    savePresetBtn_.onClick = [this] { onSavePreset(); };
    addAndMakeVisible (savePresetBtn_);

    loadPresetBtn_.setColour (juce::TextButton::buttonColourId,  colBtnNorm);
    loadPresetBtn_.setColour (juce::TextButton::textColourOffId, colText);
    loadPresetBtn_.setTooltip ("Load a user preset file (.mmfx)");
    loadPresetBtn_.onClick = [this] { onLoadPreset(); };
    addAndMakeVisible (loadPresetBtn_);

    // ── Bypass ──────────────────────────────────────────────────────────
    bypassButton.setColour (juce::ToggleButton::textColourId, colText);
    bypassButton.setColour (juce::ToggleButton::tickColourId, colAccent);
    bypassButton.setTooltip ("Bypass all processing (click-free 10ms crossfade)");
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<ButtonAttach> (
        processorRef.apvts, svToString (Params::ID::bypass), bypassButton);

    // ── Scene A label + buttons ─────────────────────────────────────────
    sceneALabel.setText ("SCENE A", juce::dontSendNotification);
    setupLabel (sceneALabel, 12.0f, juce::Justification::centredLeft, colTextDim);
    addAndMakeVisible (sceneALabel);

    sceneBLabel.setText ("SCENE B", juce::dontSendNotification);
    setupLabel (sceneBLabel, 12.0f, juce::Justification::centredLeft, colTextDim);
    addAndMakeVisible (sceneBLabel);

    static const char* sceneTooltips[8] = {
        "Clean", "Dark Drive", "Bright Echo", "Wide Space",
        "Crushed", "Dub", "Shimmer", "Telephone"
    };

    for (int i = 0; i < 8; ++i)
    {
        auto idx = static_cast<size_t> (i);

        sceneABtns[idx].setButtonText (juce::String (i + 1));
        sceneABtns[idx].setTooltip (sceneTooltips[i]);
        sceneABtns[idx].setColour (juce::TextButton::buttonColourId, colBtnNorm);
        sceneABtns[idx].setColour (juce::TextButton::textColourOffId, colText);
        sceneABtns[idx].onClick = [this, i] { sceneButtonClicked (true, i); };
        addAndMakeVisible (sceneABtns[idx]);

        sceneBBtns[idx].setButtonText (juce::String (i + 1));
        sceneBBtns[idx].setTooltip (sceneTooltips[i]);
        sceneBBtns[idx].setColour (juce::TextButton::buttonColourId, colBtnNorm);
        sceneBBtns[idx].setColour (juce::TextButton::textColourOffId, colText);
        sceneBBtns[idx].onClick = [this, i] { sceneButtonClicked (false, i); };
        addAndMakeVisible (sceneBBtns[idx]);
    }

    // ── Morph slider ────────────────────────────────────────────────────
    morphSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    morphSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
    morphSlider.setColour (juce::Slider::trackColourId,             colAccent);
    morphSlider.setColour (juce::Slider::thumbColourId,             colAccent);
    morphSlider.setColour (juce::Slider::backgroundColourId,        colBtnNorm);
    morphSlider.setColour (juce::Slider::textBoxTextColourId,       colText);
    morphSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    morphSlider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    morphSlider.setTooltip ("Blend between Scene A (left) and Scene B (right)");
    addAndMakeVisible (morphSlider);
    morphAttach = std::make_unique<SliderAttach> (
        processorRef.apvts, svToString (Params::ID::morph), morphSlider);

    morphLabel.setText ("MORPH", juce::dontSendNotification);
    setupLabel (morphLabel, 12.0f, juce::Justification::centredLeft, colAccent);
    addAndMakeVisible (morphLabel);

    // ── Macro knobs ─────────────────────────────────────────────────────
    static const std::string_view macroIDs[4] = {
        Params::ID::macro1, Params::ID::macro2,
        Params::ID::macro3, Params::ID::macro4
    };
    static const char* macroNames[4] = {
        "FILTER SWEEP", "DIRT", "SPACE", "WIDTH"
    };
    static const char* macroNums[4] = {
        "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4"
    };
    static const char* macroTips[4] = {
        "Macro 1: Filter Sweep (offsets cutoff + resonance)",
        "Macro 2: Dirt (offsets drive amount + tone)",
        "Macro 3: Space (offsets delay feedback + reverb size)",
        "Macro 4: Width (offsets delay + reverb width)"
    };

    for (int i = 0; i < 4; ++i)
    {
        auto idx = static_cast<size_t> (i);

        setupRotaryKnob (macroSliders[idx], colText);
        macroSliders[idx].setColour (juce::Slider::rotarySliderFillColourId, colAccent);
        macroSliders[idx].setTooltip (macroTips[i]);
        addAndMakeVisible (macroSliders[idx]);
        macroAttach[idx] = std::make_unique<SliderAttach> (
            processorRef.apvts, svToString (macroIDs[i]), macroSliders[idx]);

        macroNumLabels[idx].setText (macroNums[i], juce::dontSendNotification);
        setupLabel (macroNumLabels[idx], 11.0f, juce::Justification::centred, colTextDim);
        addAndMakeVisible (macroNumLabels[idx]);

        macroNameLabels[idx].setText (macroNames[i], juce::dontSendNotification);
        setupLabel (macroNameLabels[idx], 9.5f, juce::Justification::centred, colAccent);
        addAndMakeVisible (macroNameLabels[idx]);
    }

    // ── Performance knobs ───────────────────────────────────────────────
    setupRotaryKnob (mixSlider,     colText);
    setupRotaryKnob (inGainSlider,  colText);
    setupRotaryKnob (outGainSlider, colText);

    mixSlider.setTooltip     ("Dry/Wet mix (0 = dry, 1 = wet)");
    inGainSlider.setTooltip  ("Input gain in dB (-24 to +24)");
    outGainSlider.setTooltip ("Output gain in dB (-24 to +24)");

    addAndMakeVisible (mixSlider);
    addAndMakeVisible (inGainSlider);
    addAndMakeVisible (outGainSlider);

    mixAttach     = std::make_unique<SliderAttach> (processorRef.apvts, svToString (Params::ID::mix),          mixSlider);
    inGainAttach  = std::make_unique<SliderAttach> (processorRef.apvts, svToString (Params::ID::inputGainDb),  inGainSlider);
    outGainAttach = std::make_unique<SliderAttach> (processorRef.apvts, svToString (Params::ID::outputGainDb), outGainSlider);

    mixLabel.setText     ("MIX",      juce::dontSendNotification);
    inGainLabel.setText  ("IN GAIN",  juce::dontSendNotification);
    outGainLabel.setText ("OUT GAIN", juce::dontSendNotification);

    setupLabel (mixLabel,     11.0f, juce::Justification::centred, colTextDim);
    setupLabel (inGainLabel,  11.0f, juce::Justification::centred, colTextDim);
    setupLabel (outGainLabel, 11.0f, juce::Justification::centred, colTextDim);

    addAndMakeVisible (mixLabel);
    addAndMakeVisible (inGainLabel);
    addAndMakeVisible (outGainLabel);

    // ── Store buttons ───────────────────────────────────────────────────
    storeABtn.setButtonText ("STORE " + kIconArrow + " A");
    storeABtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff2d333b));
    storeABtn.setColour (juce::TextButton::textColourOffId, colText);
    storeABtn.setTooltip ("Flatten current morph+macro sound into Scene A slot");
    storeABtn.onClick = [this] { storeToScene (true); };
    addAndMakeVisible (storeABtn);

    storeBBtn.setButtonText ("STORE " + kIconArrow + " B");
    storeBBtn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff2d333b));
    storeBBtn.setColour (juce::TextButton::textColourOffId, colText);
    storeBBtn.setTooltip ("Flatten current morph+macro sound into Scene B slot");
    storeBBtn.onClick = [this] { storeToScene (false); };
    addAndMakeVisible (storeBBtn);

    // ── Module panel toggle ─────────────────────────────────────────────
    modulePanelToggle.setButtonText (kIconCollapsed + " MODULES");
    modulePanelToggle.setColour (juce::TextButton::buttonColourId,  colPanel);
    modulePanelToggle.setColour (juce::TextButton::textColourOffId, colTextDim);
    modulePanelToggle.onClick = [this] { toggleModulePanel(); };
    addAndMakeVisible (modulePanelToggle);

    // ── Module panel headers ────────────────────────────────────────────
    static const char* headerNames[4] = { "FILTER", "DRIVE", "DELAY", "REVERB" };
    for (int i = 0; i < 4; ++i)
    {
        moduleHeaders[i].setText (headerNames[i], juce::dontSendNotification);
        setupLabel (moduleHeaders[i], 11.0f, juce::Justification::centredLeft, colAccent);
        addChildComponent (moduleHeaders[i]);
    }

    // ── Module panel editable sliders ───────────────────────────────────
    for (int i = 0; i < SceneParam::kCount; ++i)
    {
        auto idx = static_cast<size_t> (i);
        const auto& inf = SceneParam::info[idx];

        // Name label
        moduleParamLabels_[idx].setText (kParamDisplayNames[i], juce::dontSendNotification);
        setupLabel (moduleParamLabels_[idx], 10.0f, juce::Justification::centredRight, colTextDim);
        addChildComponent (moduleParamLabels_[idx]);

        // Editable slider
        auto& slider = moduleSliders_[idx];
        slider.setSliderStyle (juce::Slider::LinearHorizontal);

        if (inf.isDiscrete)
            slider.setRange (static_cast<double> (inf.minVal),
                             static_cast<double> (inf.maxVal), 1.0);
        else
        {
            double step = (inf.maxVal - inf.minVal) > 100.0 ? 1.0 : 0.01;
            slider.setRange (static_cast<double> (inf.minVal),
                             static_cast<double> (inf.maxVal), step);
        }

        slider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 50, 18);
        slider.setTextBoxIsEditable (false);

        slider.textFromValueFunction = [i] (double val)
        {
            return formatSceneValue (i, static_cast<float> (val));
        };

        if (i == SceneParam::filtCutoff)
            slider.setSkewFactorFromMidPoint (1000.0);

        slider.setColour (juce::Slider::trackColourId,             colAccentDim);
        slider.setColour (juce::Slider::thumbColourId,             colAccent);
        slider.setColour (juce::Slider::backgroundColourId,        colBtnNorm);
        slider.setColour (juce::Slider::textBoxTextColourId,       colText);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);

        slider.onValueChange = [this, i] { onModuleSliderChanged (i); };
        addChildComponent (slider);
    }

    // ── Edit target button ──────────────────────────────────────────────
    editTargetBtn_.setColour (juce::TextButton::buttonColourId,  colBtnNorm);
    editTargetBtn_.setColour (juce::TextButton::textColourOffId, colAccent);
    editTargetBtn_.onClick = [this] { toggleEditTarget(); };
    addChildComponent (editTargetBtn_);

    // ── Macro config toggle ─────────────────────────────────────────────
    macroConfigToggle_.setButtonText (kIconCollapsed + " MACRO CONFIG");
    macroConfigToggle_.setColour (juce::TextButton::buttonColourId,  colPanel);
    macroConfigToggle_.setColour (juce::TextButton::textColourOffId, colTextDim);
    macroConfigToggle_.onClick = [this] { toggleMacroConfig(); };
    addAndMakeVisible (macroConfigToggle_);

    // ── Macro config headers + slots ────────────────────────────────────
    static const char* macroConfigHdrs[4] = { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4" };

    for (int m = 0; m < 4; ++m)
    {
        macroConfigHeaders_[m].setText (macroConfigHdrs[m], juce::dontSendNotification);
        setupLabel (macroConfigHeaders_[m], 11.0f, juce::Justification::centredLeft, colAccent);
        addChildComponent (macroConfigHeaders_[m]);

        for (int s = 0; s < kMaxMacroTargets; ++s)
        {
            auto& slot = macroSlots_[m][s];

            // ComboBox: param selector
            slot.paramSelector.addItem ("None", 1);
            for (int t = 0; t < kNumMacroTargetOptions; ++t)
                slot.paramSelector.addItem (kMacroTargetOptions[t].name, t + 2);
            slot.paramSelector.setSelectedId (1, juce::dontSendNotification);
            slot.paramSelector.onChange = [this, m] { onMacroSlotChanged (m); };
            slot.paramSelector.setColour (juce::ComboBox::backgroundColourId, colBtnNorm);
            slot.paramSelector.setColour (juce::ComboBox::textColourId,       colText);
            slot.paramSelector.setColour (juce::ComboBox::outlineColourId,    colDivider);
            addChildComponent (slot.paramSelector);

            // Slider: amount (-1..+1)
            slot.amountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
            slot.amountSlider.setRange (-1.0, 1.0, 0.01);
            slot.amountSlider.setValue (0.0, juce::dontSendNotification);
            slot.amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 38, 18);
            slot.amountSlider.setColour (juce::Slider::trackColourId,             colAccent);
            slot.amountSlider.setColour (juce::Slider::thumbColourId,             colAccent);
            slot.amountSlider.setColour (juce::Slider::backgroundColourId,        colBtnNorm);
            slot.amountSlider.setColour (juce::Slider::textBoxTextColourId,       colText);
            slot.amountSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            slot.amountSlider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
            slot.amountSlider.onValueChange = [this, m] { onMacroSlotChanged (m); };
            addChildComponent (slot.amountSlider);

            // ComboBox: curve selector
            for (int c = 0; c < static_cast<int> (MacroCurve::kCount); ++c)
                slot.curveSelector.addItem (macroCurveNames[c], c + 1);
            slot.curveSelector.setSelectedId (1, juce::dontSendNotification); // Linear
            slot.curveSelector.setTooltip ("Response curve for this macro target");
            slot.curveSelector.onChange = [this, m] { onMacroSlotChanged (m); };
            slot.curveSelector.setColour (juce::ComboBox::backgroundColourId, colBtnNorm);
            slot.curveSelector.setColour (juce::ComboBox::textColourId,       colTextDim);
            slot.curveSelector.setColour (juce::ComboBox::outlineColourId,    colDivider);
            addChildComponent (slot.curveSelector);
        }
    }

    // ── Size + Timer ────────────────────────────────────────────────────
    setSize (720, kCollapsedHeight);
    startTimerHz (15);
    updateSceneHighlights();
}

MacroMorphFXEditor::~MacroMorphFXEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void MacroMorphFXEditor::paint (juce::Graphics& g)
{
    const int w = getWidth();

    // Background
    g.fillAll (colBg);

    // Header bar
    g.setColour (colPanel);
    g.fillRect (0, 0, w, 46);

    // Dividers
    g.setColour (colDivider);
    g.drawHorizontalLine (46,  0.0f, static_cast<float> (w));
    g.drawHorizontalLine (170, 0.0f, static_cast<float> (w));
    g.drawHorizontalLine (330, 0.0f, static_cast<float> (w));

    // Section background for macros
    g.setColour (colPanel.withAlpha (0.3f));
    g.fillRect (0, 171, w, 159);

    // Module panel background
    if (modulePanelOpen_)
    {
        int moduleY = kCollapsedHeight;
        g.setColour (colDivider);
        g.drawHorizontalLine (moduleY, 0.0f, static_cast<float> (w));
        g.setColour (colPanel.withAlpha (0.5f));
        g.fillRect (0, moduleY, w, kModulePanelHeight);
    }

    // Macro config panel background
    if (macroConfigOpen_)
    {
        int macroConfigY = kCollapsedHeight + (modulePanelOpen_ ? kModulePanelHeight : 0);
        g.setColour (colDivider);
        g.drawHorizontalLine (macroConfigY, 0.0f, static_cast<float> (w));
        g.setColour (colPanel.withAlpha (0.4f));
        g.fillRect (0, macroConfigY, w, kMacroConfigHeight);
    }
}

void MacroMorphFXEditor::resized()
{
    const int w = getWidth();
    const int mx = 15;

    // ── Header (y: 0–46) ───────────────────────────────────────────────
    titleLabel.setBounds (mx, 8, 180, 30);
    presetSelector_.setBounds (195, 10, 170, 26);
    savePresetBtn_.setBounds (375, 10, 50, 26);
    loadPresetBtn_.setBounds (430, 10, 50, 26);
    bypassButton.setBounds (w - 120, 8, 105, 30);

    // ── Scene A row (y: 52–80) ─────────────────────────────────────────
    sceneALabel.setBounds (mx, 55, 62, 22);
    {
        const int btnX0 = 82;
        const int btnW  = 70;
        const int gap   = 5;
        for (int i = 0; i < 8; ++i)
            sceneABtns[static_cast<size_t> (i)].setBounds (
                btnX0 + i * (btnW + gap), 53, btnW, 26);
    }

    // ── Morph slider (y: 88–126) ───────────────────────────────────────
    morphLabel.setBounds (mx, 96, 55, 20);
    morphSlider.setBounds (75, 90, w - 75 - mx, 36);

    // ── Scene B row (y: 132–162) ───────────────────────────────────────
    sceneBLabel.setBounds (mx, 139, 62, 22);
    {
        const int btnX0 = 82;
        const int btnW  = 70;
        const int gap   = 5;
        for (int i = 0; i < 8; ++i)
            sceneBBtns[static_cast<size_t> (i)].setBounds (
                btnX0 + i * (btnW + gap), 137, btnW, 26);
    }

    // ── Macro knobs (y: 175–325) ───────────────────────────────────────
    {
        const int knobSize = 90;
        const int colW     = w / 4;
        const int knobY    = 205;

        for (int i = 0; i < 4; ++i)
        {
            auto idx = static_cast<size_t> (i);
            int cx = colW / 2 + i * colW;

            macroNumLabels[idx].setBounds  (cx - 55, 178, 110, 16);
            macroSliders[idx].setBounds    (cx - knobSize / 2, knobY, knobSize, knobSize);
            macroNameLabels[idx].setBounds  (cx - 55, knobY + knobSize + 2, 110, 14);
        }
    }

    // ── Performance row (y: 340–470) ───────────────────────────────────
    {
        const int knobSize = 80;
        const int knobY    = 370;
        const int colW     = w / 5;

        int cx0 = colW / 2;
        mixLabel.setBounds    (cx0 - 40, 345, 80, 18);
        mixSlider.setBounds   (cx0 - knobSize / 2, knobY, knobSize, knobSize);

        int cx1 = colW + colW / 2;
        inGainLabel.setBounds  (cx1 - 40, 345, 80, 18);
        inGainSlider.setBounds (cx1 - knobSize / 2, knobY, knobSize, knobSize);

        int cx2 = 2 * colW + colW / 2;
        outGainLabel.setBounds  (cx2 - 40, 345, 80, 18);
        outGainSlider.setBounds (cx2 - knobSize / 2, knobY, knobSize, knobSize);

        int btnW = 105;
        int btnH = 34;
        storeABtn.setBounds (w - 2 * btnW - mx - 8, 368, btnW, btnH);
        storeBBtn.setBounds (w - btnW - mx,          368, btnW, btnH);
    }

    // ── Toggle buttons (bottom of collapsed area) ───────────────────────
    modulePanelToggle.setBounds (mx, kCollapsedHeight - 28, 130, 24);
    macroConfigToggle_.setBounds (mx + 140, kCollapsedHeight - 28, 155, 24);

    // ── Module panel content (editable sliders) ────────────────────────
    if (modulePanelOpen_)
    {
        const int panelY  = kCollapsedHeight + 4;
        const int colW    = (w - 2 * mx) / 4;
        const int rowH    = 22;
        const int nameW   = 40;
        const int sliderW = colW - nameW - 8;

        // Edit target button (top-right of module panel area)
        editTargetBtn_.setBounds (w - mx - 75, panelY, 75, 18);

        static const int filterParams[] = { SceneParam::filtMode, SceneParam::filtCutoff, SceneParam::filtReso };
        static const int driveParams[]  = { SceneParam::driveAmt, SceneParam::driveTone };
        static const int delayParams[]  = { SceneParam::delaySync, SceneParam::delayFb, SceneParam::delayTone,
                                            SceneParam::delayWidth, SceneParam::delayPingP };
        static const int reverbParams[] = { SceneParam::revSize, SceneParam::revDamp,
                                            SceneParam::revPreDelay, SceneParam::revWidth };

        struct ColInfo { const int* params; int count; };
        ColInfo cols[4] = {
            { filterParams, 3 },
            { driveParams,  2 },
            { delayParams,  5 },
            { reverbParams, 4 }
        };

        for (int col = 0; col < 4; ++col)
        {
            int x = mx + col * colW;
            moduleHeaders[col].setBounds (x, panelY, colW - 5, 18);

            for (int row = 0; row < cols[col].count; ++row)
            {
                int paramIdx = cols[col].params[row];
                auto pidx = static_cast<size_t> (paramIdx);
                int y = panelY + 22 + row * rowH;

                moduleParamLabels_[pidx].setBounds (x, y, nameW, rowH);
                moduleSliders_[pidx].setBounds (x + nameW, y, sliderW, rowH);
            }
        }
    }

    // ── Macro config panel content ──────────────────────────────────────
    if (macroConfigOpen_)
    {
        const int panelY = kCollapsedHeight
                         + (modulePanelOpen_ ? kModulePanelHeight : 0)
                         + 4;
        const int colW   = (w - 2 * mx) / 4;
        const int rowH   = 28;

        for (int m = 0; m < 4; ++m)
        {
            int x = mx + m * colW;
            macroConfigHeaders_[m].setBounds (x, panelY, colW - 5, 20);

            for (int s = 0; s < kMaxMacroTargets; ++s)
            {
                int y = panelY + 24 + s * rowH;
                int paramW  = (colW * 38) / 100;  // ~38% for param selector
                int sliderW = (colW * 32) / 100;  // ~32% for amount slider
                int curveW  = colW - paramW - sliderW - 6; // remaining for curve

                macroSlots_[m][s].paramSelector.setBounds  (x, y, paramW, 22);
                macroSlots_[m][s].amountSlider.setBounds   (x + paramW + 2, y, sliderW, 22);
                macroSlots_[m][s].curveSelector.setBounds   (x + paramW + sliderW + 4, y, curveW, 22);
            }
        }
    }
}

//==============================================================================
int MacroMorphFXEditor::computeTotalHeight() const
{
    return kCollapsedHeight
         + (modulePanelOpen_  ? kModulePanelHeight  : 0)
         + (macroConfigOpen_  ? kMacroConfigHeight   : 0);
}

//==============================================================================
void MacroMorphFXEditor::timerCallback()
{
    updateSceneHighlights();

    if (modulePanelOpen_)
        refreshModuleSliders();

    // ── Store flash countdown ───────────────────────────────────────────
    if (storeFlashA_ > 0 && --storeFlashA_ == 0)
    {
        storeABtn.setButtonText ("STORE " + kIconArrow + " A");
        storeABtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d333b));
    }
    if (storeFlashB_ > 0 && --storeFlashB_ == 0)
    {
        storeBBtn.setButtonText ("STORE " + kIconArrow + " B");
        storeBBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d333b));
    }
}

void MacroMorphFXEditor::sceneButtonClicked (bool isSceneA, int index)
{
    auto paramId = isSceneA ? Params::ID::sceneA : Params::ID::sceneB;
    auto* param = processorRef.apvts.getParameter (svToString (paramId));

    if (param != nullptr)
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (
            param->convertTo0to1 (static_cast<float> (index)));
        param->endChangeGesture();
    }

    updateSceneHighlights();
}

void MacroMorphFXEditor::updateSceneHighlights()
{
    int currentA = static_cast<int> (
        processorRef.apvts.getRawParameterValue (svToString (Params::ID::sceneA))->load());
    int currentB = static_cast<int> (
        processorRef.apvts.getRawParameterValue (svToString (Params::ID::sceneB))->load());

    for (int i = 0; i < 8; ++i)
    {
        auto idx = static_cast<size_t> (i);
        bool activeA = (i == currentA);
        bool activeB = (i == currentB);

        sceneABtns[idx].setColour (juce::TextButton::buttonColourId,
                                   activeA ? colBtnActive : colBtnNorm);
        sceneABtns[idx].setColour (juce::TextButton::textColourOffId,
                                   activeA ? juce::Colours::white : colText);

        sceneBBtns[idx].setColour (juce::TextButton::buttonColourId,
                                   activeB ? colBtnActive : colBtnNorm);
        sceneBBtns[idx].setColour (juce::TextButton::textColourOffId,
                                   activeB ? juce::Colours::white : colText);
    }
}

void MacroMorphFXEditor::storeToScene (bool isSceneA)
{
    auto paramId = isSceneA ? Params::ID::sceneA : Params::ID::sceneB;
    int sceneIdx = static_cast<int> (
        processorRef.apvts.getRawParameterValue (svToString (paramId))->load());

    processorRef.storeCurrentToScene (sceneIdx);

    // Flash feedback
    if (isSceneA)
    {
        storeFlashA_ = 15; // ~1 second at 15 Hz
        storeABtn.setButtonText ("STORED!");
        storeABtn.setColour (juce::TextButton::buttonColourId, colAccent);
    }
    else
    {
        storeFlashB_ = 15;
        storeBBtn.setButtonText ("STORED!");
        storeBBtn.setColour (juce::TextButton::buttonColourId, colAccent);
    }
}

//==============================================================================
void MacroMorphFXEditor::toggleModulePanel()
{
    modulePanelOpen_ = ! modulePanelOpen_;

    modulePanelToggle.setButtonText (modulePanelOpen_
        ? kIconExpanded + " MODULES"
        : kIconCollapsed + " MODULES");

    for (int i = 0; i < 4; ++i)
        moduleHeaders[i].setVisible (modulePanelOpen_);

    for (int i = 0; i < SceneParam::kCount; ++i)
    {
        moduleSliders_[static_cast<size_t> (i)].setVisible (modulePanelOpen_);
        moduleParamLabels_[static_cast<size_t> (i)].setVisible (modulePanelOpen_);
    }

    editTargetBtn_.setVisible (modulePanelOpen_);

    setSize (720, computeTotalHeight());

    if (modulePanelOpen_)
        refreshModuleSliders();
}

void MacroMorphFXEditor::refreshModuleSliders()
{
    // Determine which scene to display for editing
    auto paramId = editTargetIsA_ ? Params::ID::sceneA : Params::ID::sceneB;
    int sceneIdx = static_cast<int> (
        processorRef.apvts.getRawParameterValue (svToString (paramId))->load());
    sceneIdx = std::clamp (sceneIdx, 0, kNumScenes - 1);

    const auto& scene = processorRef.getScene (sceneIdx);

    for (int i = 0; i < SceneParam::kCount; ++i)
    {
        auto idx = static_cast<size_t> (i);

        // Only update if the user is not currently dragging this slider
        if (! moduleSliders_[idx].isMouseButtonDown())
        {
            moduleSliders_[idx].setValue (
                static_cast<double> (scene.values[i]),
                juce::dontSendNotification);
        }
    }
}

void MacroMorphFXEditor::onModuleSliderChanged (int paramIdx)
{
    auto paramId = editTargetIsA_ ? Params::ID::sceneA : Params::ID::sceneB;
    int sceneIdx = static_cast<int> (
        processorRef.apvts.getRawParameterValue (svToString (paramId))->load());
    sceneIdx = std::clamp (sceneIdx, 0, kNumScenes - 1);

    float newVal = static_cast<float> (moduleSliders_[static_cast<size_t> (paramIdx)].getValue());
    processorRef.setSceneParam (sceneIdx, paramIdx, newVal);
}

void MacroMorphFXEditor::toggleEditTarget()
{
    editTargetIsA_ = ! editTargetIsA_;
    editTargetBtn_.setButtonText (editTargetIsA_ ? "EDIT: A" : "EDIT: B");
    refreshModuleSliders();
}

//==============================================================================
void MacroMorphFXEditor::toggleMacroConfig()
{
    macroConfigOpen_ = ! macroConfigOpen_;

    macroConfigToggle_.setButtonText (macroConfigOpen_
        ? kIconExpanded + " MACRO CONFIG"
        : kIconCollapsed + " MACRO CONFIG");

    for (int m = 0; m < 4; ++m)
    {
        macroConfigHeaders_[m].setVisible (macroConfigOpen_);

        for (int s = 0; s < kMaxMacroTargets; ++s)
        {
            macroSlots_[m][s].paramSelector.setVisible (macroConfigOpen_);
            macroSlots_[m][s].amountSlider.setVisible  (macroConfigOpen_);
            macroSlots_[m][s].curveSelector.setVisible  (macroConfigOpen_);
        }
    }

    setSize (720, computeTotalHeight());

    if (macroConfigOpen_)
        updateMacroConfigDisplay();
}

void MacroMorphFXEditor::updateMacroConfigDisplay()
{
    for (int m = 0; m < 4; ++m)
    {
        const auto& mappings = processorRef.getMacroEngine().getMappings (m);

        for (int s = 0; s < kMaxMacroTargets; ++s)
        {
            auto& slot = macroSlots_[m][s];

            if (s < static_cast<int> (mappings.size()))
            {
                const auto& target = mappings[static_cast<size_t> (s)];
                int comboId = sceneParamToComboId (target.sceneParamIndex);
                slot.paramSelector.setSelectedId (comboId, juce::dontSendNotification);
                slot.amountSlider.setValue (
                    static_cast<double> (target.amount),
                    juce::dontSendNotification);
                slot.curveSelector.setSelectedId (
                    static_cast<int> (target.curve) + 1, juce::dontSendNotification);
            }
            else
            {
                slot.paramSelector.setSelectedId (1, juce::dontSendNotification); // None
                slot.amountSlider.setValue (0.0, juce::dontSendNotification);
                slot.curveSelector.setSelectedId (1, juce::dontSendNotification); // Linear
            }
        }
    }
}

void MacroMorphFXEditor::onMacroSlotChanged (int macroIdx)
{
    std::vector<MacroTarget> targets;

    for (int s = 0; s < kMaxMacroTargets; ++s)
    {
        auto& slot = macroSlots_[macroIdx][s];
        int selectedId = slot.paramSelector.getSelectedId();

        if (selectedId >= 2)
        {
            int optionIdx = selectedId - 2;
            if (optionIdx >= 0 && optionIdx < kNumMacroTargetOptions)
            {
                float amount = static_cast<float> (slot.amountSlider.getValue());
                int curveId = slot.curveSelector.getSelectedId();
                auto curve = static_cast<MacroCurve> (
                    std::clamp (curveId - 1, 0, static_cast<int> (MacroCurve::kCount) - 1));
                targets.push_back ({ kMacroTargetOptions[optionIdx].sceneIdx, amount, curve });
            }
        }
    }

    processorRef.getMacroEngine().setMappings (macroIdx, std::move (targets));
}

//==============================================================================
void MacroMorphFXEditor::onSavePreset()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Save Preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.mmfx");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                        | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return;

            // Ensure .mmfx extension
            if (! file.hasFileExtension ("mmfx"))
                file = file.withFileExtension ("mmfx");

            processorRef.saveUserPreset (file);
        });
}

void MacroMorphFXEditor::onLoadPreset()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load Preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.mmfx");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File() || ! file.existsAsFile())
                return;

            if (processorRef.loadUserPreset (file))
            {
                // Refresh macro config if open
                if (macroConfigOpen_)
                    updateMacroConfigDisplay();

                // Refresh module sliders if open
                if (modulePanelOpen_)
                    refreshModuleSliders();
            }
        });
}

//==============================================================================
void MacroMorphFXEditor::onPresetChanged()
{
    int presetIdx = presetSelector_.getSelectedId() - 1;

    if (presetIdx >= 0 && presetIdx < kNumFactoryPresets)
    {
        processorRef.setCurrentProgram (presetIdx);

        // Update macro config display if visible
        if (macroConfigOpen_)
            updateMacroConfigDisplay();
    }
}
