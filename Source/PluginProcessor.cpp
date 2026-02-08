#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <juce_core/juce_core.h>

//==============================================================================
// Helper: smoothing time in seconds for each scene parameter (per SPEC).
static double getSceneParamSmoothTimeSec (int paramIndex)
{
    switch (paramIndex)
    {
        case SceneParam::filtMode:    return 0.0;    // discrete — instant
        case SceneParam::filtCutoff:  return 0.020;  // cutoff ~20 ms
        case SceneParam::filtReso:    return 0.030;  // tone ~30 ms
        case SceneParam::driveAmt:    return 0.030;
        case SceneParam::driveTone:   return 0.030;
        case SceneParam::delaySync:   return 0.0;    // discrete
        case SceneParam::delayFb:     return 0.050;  // feedback ~50 ms
        case SceneParam::delayTone:   return 0.030;
        case SceneParam::delayWidth:  return 0.030;
        case SceneParam::delayPingP:  return 0.0;    // discrete
        case SceneParam::revSize:     return 0.100;  // timeish ~100 ms
        case SceneParam::revDamp:     return 0.030;
        case SceneParam::revPreDelay: return 0.100;
        case SceneParam::revWidth:    return 0.030;
        default:                      return 0.0;
    }
}

//==============================================================================
// Helper: return choice labels for choice-type parameters.
static juce::StringArray getChoiceLabels (std::string_view paramId)
{
    using namespace Params::ID;

    if (paramId == filtMode)
        return { "LP", "BP", "HP" };

    if (paramId == sceneA || paramId == sceneB)
        return { "1", "2", "3", "4", "5", "6", "7", "8" };

    if (paramId == delaySync)
        return { "1/32", "1/16", "1/8", "1/4", "1/2", "1 Bar", "1/8 Dot", "1/4 Dot" };

    return { "Off", "On" };
}

//==============================================================================
// Helper: get raw param value by string_view ID
static inline float getRawParam (const juce::AudioProcessorValueTreeState& apvts,
                                 std::string_view id)
{
    return apvts.getRawParameterValue (juce::String (id.data(), id.size()))->load();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
MacroMorphFXProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& spec : Params::all)
    {
        juce::String id (spec.id.data(), spec.id.size());

        switch (spec.type)
        {
            case Params::ParamType::float01:
            {
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { id, 1 },
                    id,
                    juce::NormalisableRange<float> (spec.minValue, spec.maxValue, 0.0f),
                    spec.defaultValue));
                break;
            }

            case Params::ParamType::floatRange:
            {
                auto range = juce::NormalisableRange<float> (spec.minValue, spec.maxValue, 0.0f);

                if (spec.id == Params::ID::filtCutoff)
                    range.setSkewForCentre (1000.0f);

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { id, 1 },
                    id,
                    range,
                    spec.defaultValue));
                break;
            }

            case Params::ParamType::choice:
            {
                auto labels = getChoiceLabels (spec.id);
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { id, 1 },
                    id,
                    labels,
                    spec.defaultChoice));
                break;
            }

            case Params::ParamType::toggle:
            {
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { id, 1 },
                    id,
                    spec.defaultValue > 0.5f));
                break;
            }
        }
    }

    return layout;
}

//==============================================================================
MacroMorphFXProcessor::MacroMorphFXProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, juce::Identifier ("MacroMorphFXState"), createParameterLayout())
{
    loadFactoryPresetData (0);
}

MacroMorphFXProcessor::~MacroMorphFXProcessor()
{
}

//==============================================================================
const juce::String MacroMorphFXProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MacroMorphFXProcessor::acceptsMidi() const    { return false; }
bool MacroMorphFXProcessor::producesMidi() const   { return false; }
bool MacroMorphFXProcessor::isMidiEffect() const   { return false; }
double MacroMorphFXProcessor::getTailLengthSeconds() const { return 0.0; }

int MacroMorphFXProcessor::getNumPrograms()        { return kNumFactoryPresets; }
int MacroMorphFXProcessor::getCurrentProgram()     { return currentProgram_; }

void MacroMorphFXProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < kNumFactoryPresets)
    {
        currentProgram_ = index;
        loadFactoryPreset (index);
    }
}

const juce::String MacroMorphFXProcessor::getProgramName (int index)
{
    if (index >= 0 && index < kNumFactoryPresets)
        return kFactoryPresetNames[index];
    return {};
}

void MacroMorphFXProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

//==============================================================================
void MacroMorphFXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32> (getTotalNumOutputChannels());

    inputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.02);

    filterModule.prepare (spec);
    driveModule.prepare (spec);
    delayModule.prepare (spec);
    reverbModule.prepare (spec);

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);

    dryBuffer.setSize (static_cast<int> (spec.numChannels),
                       static_cast<int> (spec.maximumBlockSize));

    // Initialise parameter smoothers with per-SPEC smoothing times
    for (int i = 0; i < SceneParam::kCount; ++i)
    {
        smoothScene_[static_cast<size_t> (i)].reset (sampleRate,
                                                      getSceneParamSmoothTimeSec (i));
        smoothScene_[static_cast<size_t> (i)].setCurrentAndTargetValue (
            SceneParam::info[static_cast<size_t> (i)].defaultVal);
    }

    // Bypass crossfade: 10 ms per SPEC
    bypassSmooth_.reset (sampleRate, 0.01);
    bypassSmooth_.setCurrentAndTargetValue (0.0f);
}

void MacroMorphFXProcessor::releaseResources()
{
    filterModule.reset();
    driveModule.reset();
    delayModule.reset();
    reverbModule.reset();
    inputGain.reset();
    outputGain.reset();
}

bool MacroMorphFXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void MacroMorphFXProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // ── Read performance parameters (always from APVTS) ─────────────────
    using namespace Params::ID;

    const bool bypassed = getRawParam (apvts, bypass) > 0.5f;
    bypassSmooth_.setTargetValue (bypassed ? 1.0f : 0.0f);

    // If fully bypassed and settled, skip all processing (saves CPU)
    if (! bypassSmooth_.isSmoothing() && bypassSmooth_.getCurrentValue() > 0.999f)
    {
        bypassSmooth_.skip (buffer.getNumSamples());
        return;
    }

    const float inGainDb  = getRawParam (apvts, inputGainDb);
    const float outGainDb = getRawParam (apvts, outputGainDb);
    const float mixAmount = getRawParam (apvts, mix);

    // ── Scene / Morph / Macro pipeline ───────────────────────────────────
    const int sceneAIdx = std::clamp (static_cast<int> (getRawParam (apvts, sceneA)), 0, kNumScenes - 1);
    const int sceneBIdx = std::clamp (static_cast<int> (getRawParam (apvts, sceneB)), 0, kNumScenes - 1);
    const float morphVal = getRawParam (apvts, morph);

    // 1. Morph between scene A and scene B
    SceneParams morphed = SceneParams::morph (scenes_[static_cast<size_t> (sceneAIdx)],
                                              scenes_[static_cast<size_t> (sceneBIdx)],
                                              morphVal);

    // 2. Apply macro offsets
    const float macroValues[MacroEngine::kNumMacros] = {
        getRawParam (apvts, macro1),
        getRawParam (apvts, macro2),
        getRawParam (apvts, macro3),
        getRawParam (apvts, macro4)
    };
    macroEngine_.apply (morphed, macroValues);

    // 3. Smooth scene parameters to avoid clicks during morph transitions
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < SceneParam::kCount; ++i)
    {
        auto idx = static_cast<size_t> (i);

        if (SceneParam::info[idx].isDiscrete)
            smoothScene_[idx].setCurrentAndTargetValue (morphed.values[i]);
        else
            smoothScene_[idx].setTargetValue (morphed.values[i]);

        smoothScene_[idx].skip (numSamples);
    }

    // 4. Read smoothed values and store for UI
    SceneParams smoothed;
    for (int i = 0; i < SceneParam::kCount; ++i)
        smoothed.values[i] = smoothScene_[static_cast<size_t> (i)].getCurrentValue();

    lastComputedParams_ = smoothed;  // publish for UI (safe: single-writer)

    // 5. Extract final DSP values from the smoothed scene
    const int   filtModeVal    = static_cast<int> (smoothed.values[SceneParam::filtMode]);
    const float filtCutoffHz   = smoothed.values[SceneParam::filtCutoff];
    const float filtResoVal    = smoothed.values[SceneParam::filtReso];
    const float driveAmtVal    = smoothed.values[SceneParam::driveAmt];
    const float driveToneVal   = smoothed.values[SceneParam::driveTone];
    const int   delaySyncVal   = static_cast<int> (smoothed.values[SceneParam::delaySync]);
    const float delayFbVal     = smoothed.values[SceneParam::delayFb];
    const float delayToneVal   = smoothed.values[SceneParam::delayTone];
    const float delayWidthVal  = smoothed.values[SceneParam::delayWidth];
    const bool  delayPPVal     = smoothed.values[SceneParam::delayPingP] > 0.5f;
    const float revSizeVal     = smoothed.values[SceneParam::revSize];
    const float revDampVal     = smoothed.values[SceneParam::revDamp];
    const float revPreDelayVal = smoothed.values[SceneParam::revPreDelay];
    const float revWidthVal    = smoothed.values[SceneParam::revWidth];

    // ── Get BPM from host ────────────────────────────────────────────────
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        auto posInfo = ph->getPosition();
        if (posInfo.hasValue())
        {
            auto bpmOpt = posInfo->getBpm();
            if (bpmOpt.hasValue())
                bpm = *bpmOpt;
        }
    }

    // ── Save dry signal for mix ──────────────────────────────────────────
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    // ── Signal chain ─────────────────────────────────────────────────────
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // 1. Input Gain
    inputGain.setGainDecibels (inGainDb);
    inputGain.process (context);

    // 2. Filter
    filterModule.setParameters (filtModeVal, filtCutoffHz, filtResoVal);
    filterModule.process (context);

    // 3. Drive
    driveModule.setParameters (driveAmtVal, driveToneVal);
    driveModule.process (block);

    // 4. Delay
    delayModule.setParameters (delaySyncVal, delayFbVal, delayToneVal,
                               delayWidthVal, delayPPVal, bpm);
    delayModule.process (buffer);

    // 5. Reverb
    reverbModule.setParameters (revSizeVal, revDampVal, revPreDelayVal, revWidthVal);
    reverbModule.process (block);

    // 6. Mix (dry/wet blend)
    if (mixAmount < 1.0f)
    {
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);

            for (int s = 0; s < numSamples; ++s)
                wet[s] = dry[s] + mixAmount * (wet[s] - dry[s]);
        }
    }

    // 7. Output Gain
    outputGain.setGainDecibels (outGainDb);
    outputGain.process (context);

    // 8. Bypass crossfade (10 ms click-free, per SPEC)
    if (bypassSmooth_.isSmoothing() || bypassSmooth_.getCurrentValue() > 0.001f)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            const float bv = bypassSmooth_.getNextValue();
            for (int ch = 0; ch < totalNumInputChannels; ++ch)
            {
                float wet = buffer.getSample (ch, s);
                float dry = dryBuffer.getSample (ch, s);
                buffer.setSample (ch, s, wet + bv * (dry - wet));
            }
        }
    }

    // 9. Output safety clamp (SPEC: "MVP can clamp output to avoid runaway")
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int s = 0; s < numSamples; ++s)
            data[s] = std::clamp (data[s], -4.0f, 4.0f);
    }
}

//==============================================================================
bool MacroMorphFXProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* MacroMorphFXProcessor::createEditor()
{
    return new MacroMorphFXEditor (*this);
}

//==============================================================================
void MacroMorphFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto rootXml = std::make_unique<juce::XmlElement> ("MacroMorphFXPreset");

    // 1. APVTS parameters
    auto apvtsState = apvts.copyState();
    rootXml->addChildElement (apvtsState.createXml().release());

    // 2. Scene data
    auto* scenesXml = rootXml->createNewChildElement ("Scenes");
    for (int i = 0; i < kNumScenes; ++i)
    {
        auto* sceneXml = scenesXml->createNewChildElement ("Scene");
        sceneXml->setAttribute ("index", i);

        for (int p = 0; p < SceneParam::kCount; ++p)
        {
            const auto& inf = SceneParam::info[static_cast<size_t> (p)];
            sceneXml->setAttribute (juce::String (inf.id.data(), inf.id.size()),
                                    static_cast<double> (scenes_[static_cast<size_t> (i)].values[p]));
        }
    }

    // 3. Macro mappings
    auto* macrosXml = rootXml->createNewChildElement ("MacroMappings");
    for (int m = 0; m < MacroEngine::kNumMacros; ++m)
    {
        auto* macroXml = macrosXml->createNewChildElement ("Macro");
        macroXml->setAttribute ("index", m);

        for (const auto& target : macroEngine_.getMappings (m))
        {
            auto* targetXml = macroXml->createNewChildElement ("Target");
            const auto& inf = SceneParam::info[static_cast<size_t> (target.sceneParamIndex)];
            targetXml->setAttribute ("param", juce::String (inf.id.data(), inf.id.size()));
            targetXml->setAttribute ("amount", static_cast<double> (target.amount));
            targetXml->setAttribute ("curve", static_cast<int> (target.curve));
        }
    }

    copyXmlToBinary (*rootXml, destData);
}

void MacroMorphFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml == nullptr)
        return;

    // New format: wrapped in MacroMorphFXPreset
    if (xml->hasTagName ("MacroMorphFXPreset"))
    {
        // Restore APVTS
        auto* apvtsXml = xml->getChildByName (apvts.state.getType());
        if (apvtsXml != nullptr)
            apvts.replaceState (juce::ValueTree::fromXml (*apvtsXml));

        // Restore scenes
        auto* scenesXml = xml->getChildByName ("Scenes");
        if (scenesXml != nullptr)
        {
            for (auto* sceneXml : scenesXml->getChildIterator())
            {
                int idx = sceneXml->getIntAttribute ("index", -1);
                if (idx < 0 || idx >= kNumScenes)
                    continue;

                for (int p = 0; p < SceneParam::kCount; ++p)
                {
                    const auto& inf = SceneParam::info[static_cast<size_t> (p)];
                    juce::String attrName (inf.id.data(), inf.id.size());

                    if (sceneXml->hasAttribute (attrName))
                        scenes_[static_cast<size_t> (idx)].values[p] =
                            static_cast<float> (sceneXml->getDoubleAttribute (attrName));
                }
            }
        }

        // Restore macro mappings
        auto* macrosXml = xml->getChildByName ("MacroMappings");
        if (macrosXml != nullptr)
        {
            macroEngine_.clearAllMappings();

            for (auto* macroXml : macrosXml->getChildIterator())
            {
                int mIdx = macroXml->getIntAttribute ("index", -1);
                if (mIdx < 0 || mIdx >= MacroEngine::kNumMacros)
                    continue;

                std::vector<MacroTarget> targets;

                for (auto* targetXml : macroXml->getChildIterator())
                {
                    juce::String paramName = targetXml->getStringAttribute ("param");
                    float amount = static_cast<float> (targetXml->getDoubleAttribute ("amount"));

                    int curveInt = targetXml->getIntAttribute ("curve", 0);
                    auto curve = static_cast<MacroCurve> (
                        std::clamp (curveInt, 0, static_cast<int> (MacroCurve::kCount) - 1));

                    // Find the scene param index by ID
                    for (int p = 0; p < SceneParam::kCount; ++p)
                    {
                        const auto& inf = SceneParam::info[static_cast<size_t> (p)];
                        if (juce::String (inf.id.data(), inf.id.size()) == paramName)
                        {
                            targets.push_back ({ p, amount, curve });
                            break;
                        }
                    }
                }

                macroEngine_.setMappings (mIdx, std::move (targets));
            }
        }
    }
    else if (xml->hasTagName (apvts.state.getType()))
    {
        // Legacy format: just APVTS state (backward compat with Milestone 0–2 saves)
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

//==============================================================================
void MacroMorphFXProcessor::loadFactoryPresetData (int index)
{
    if (index < 0 || index >= kNumFactoryPresets)
        return;

    auto presets = createFactoryPresets();
    const auto& preset = presets[static_cast<size_t> (index)];

    // Load scenes
    scenes_ = preset.scenes;

    // Load macro mappings
    macroEngine_.clearAllMappings();
    for (int m = 0; m < MacroEngine::kNumMacros; ++m)
    {
        std::vector<MacroTarget> targets;
        const auto& mc = preset.macros[static_cast<size_t> (m)];

        for (int t = 0; t < mc.numTargets; ++t)
            targets.push_back (mc.targets[t]);

        macroEngine_.setMappings (m, std::move (targets));
    }
}

void MacroMorphFXProcessor::loadFactoryPreset (int index)
{
    loadFactoryPresetData (index);

    // Reset performance parameters to defaults
    auto setParam = [this] (std::string_view id, float rawValue)
    {
        if (auto* p = apvts.getParameter (juce::String (id.data(), id.size())))
            p->setValueNotifyingHost (p->convertTo0to1 (rawValue));
    };

    using namespace Params::ID;
    setParam (morph,       0.f);
    setParam (macro1,      0.f);
    setParam (macro2,      0.f);
    setParam (macro3,      0.f);
    setParam (macro4,      0.f);
    setParam (sceneA,      0.f);
    setParam (sceneB,      1.f);
    setParam (mix,         1.f);
    setParam (inputGainDb, 0.f);
    setParam (outputGainDb,0.f);
    setParam (bypass,      0.f);
}

void MacroMorphFXProcessor::setSceneParam (int sceneIndex, int paramIndex, float value)
{
    if (sceneIndex >= 0 && sceneIndex < kNumScenes
        && paramIndex >= 0 && paramIndex < SceneParam::kCount)
    {
        scenes_[static_cast<size_t> (sceneIndex)].values[paramIndex] = value;
    }
}

void MacroMorphFXProcessor::storeScene (int sceneIndex)
{
    if (sceneIndex < 0 || sceneIndex >= kNumScenes)
        return;

    auto& scene = scenes_[static_cast<size_t> (sceneIndex)];

    for (int p = 0; p < SceneParam::kCount; ++p)
    {
        const auto& inf = SceneParam::info[static_cast<size_t> (p)];
        scene.values[p] = getRawParam (apvts, inf.id);
    }
}

void MacroMorphFXProcessor::storeCurrentToScene (int sceneIndex)
{
    if (sceneIndex < 0 || sceneIndex >= kNumScenes)
        return;

    // Recompute the current morph + macro values (same logic as processBlock)
    const int sceneAIdx = std::clamp (static_cast<int> (getRawParam (apvts, Params::ID::sceneA)), 0, kNumScenes - 1);
    const int sceneBIdx = std::clamp (static_cast<int> (getRawParam (apvts, Params::ID::sceneB)), 0, kNumScenes - 1);
    const float morphVal = getRawParam (apvts, Params::ID::morph);

    SceneParams morphed = SceneParams::morph (scenes_[static_cast<size_t> (sceneAIdx)],
                                              scenes_[static_cast<size_t> (sceneBIdx)],
                                              morphVal);

    const float macroVals[MacroEngine::kNumMacros] = {
        getRawParam (apvts, Params::ID::macro1),
        getRawParam (apvts, Params::ID::macro2),
        getRawParam (apvts, Params::ID::macro3),
        getRawParam (apvts, Params::ID::macro4)
    };
    macroEngine_.apply (morphed, macroVals);

    scenes_[static_cast<size_t> (sceneIndex)] = morphed;
}

//==============================================================================
bool MacroMorphFXProcessor::saveUserPreset (const juce::File& file) const
{
    // Reuse the same XML format as getStateInformation
    juce::MemoryBlock data;

    // We need to call getStateInformation, but it's const-compatible since
    // we only read state. Cast away const for the JUCE API.
    const_cast<MacroMorphFXProcessor*> (this)->getStateInformation (data);

    return file.replaceWithData (data.getData(), data.getSize());
}

bool MacroMorphFXProcessor::loadUserPreset (const juce::File& file)
{
    juce::MemoryBlock data;

    if (! file.loadFileAsData (data))
        return false;

    setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    return true;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MacroMorphFXProcessor();
}
