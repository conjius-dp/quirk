#include "PluginProcessor.h"
#include "PluginEditor.h"

static const char* kAnimatedParamIds[AnimationEngine::kNumParams] = {
    "volume", "attack", "decay", "sustain", "release", "velocity",
    "rc_sh_dx", "rc_sh_dy", "rc_eh_dx", "rc_eh_dy",
    "rc_p0_on", "rc_p0_x", "rc_p0_y", "rc_p0_idx", "rc_p0_idy", "rc_p0_odx", "rc_p0_ody",
    "rc_p1_on", "rc_p1_x", "rc_p1_y", "rc_p1_idx", "rc_p1_idy", "rc_p1_odx", "rc_p1_ody",
    "rc_p2_on", "rc_p2_x", "rc_p2_y", "rc_p2_idx", "rc_p2_idy", "rc_p2_odx", "rc_p2_ody",
    "lc_sh_dx", "lc_sh_dy", "lc_eh_dx", "lc_eh_dy",
    "lc_p0_on", "lc_p0_x", "lc_p0_y", "lc_p0_idx", "lc_p0_idy", "lc_p0_odx", "lc_p0_ody",
    "lc_p1_on", "lc_p1_x", "lc_p1_y", "lc_p1_idx", "lc_p1_idy", "lc_p1_odx", "lc_p1_ody",
    "lc_p2_on", "lc_p2_x", "lc_p2_y", "lc_p2_idx", "lc_p2_idy", "lc_p2_odx", "lc_p2_ody",
};

const char* QuirkAudioProcessor::getAnimatedParamId(int idx)
{
    if (idx < 0 || idx >= AnimationEngine::kNumParams) return nullptr;
    return kAnimatedParamIds[idx];
}

int QuirkAudioProcessor::paramIdToIndex(const juce::String& paramId)
{
    for (int i = 0; i < AnimationEngine::kNumParams; ++i)
        if (paramId == kAnimatedParamIds[i])
            return i;
    return -1;
}

void QuirkAudioProcessor::setKeyframeParamForAllFrames(const juce::String& paramId, float value)
{
    const int idx = paramIdToIndex(paramId);
    if (idx < 0) return;
    for (int f = 0; f < AnimationEngine::kNumKeyframes; ++f)
        animationEngine_.setKeyframeValue(f, idx, value);
}

static void registerCurveListeners(juce::AudioProcessorValueTreeState& apvts,
                                    juce::AudioProcessorValueTreeState::Listener* listener,
                                    const juce::String& prefix)
{
    apvts.addParameterListener(prefix + "sh_dx", listener);
    apvts.addParameterListener(prefix + "sh_dy", listener);
    apvts.addParameterListener(prefix + "eh_dx", listener);
    apvts.addParameterListener(prefix + "eh_dy", listener);
    for (int i = 0; i < BezierCurve::SlotValues::kMaxSlots; ++i)
    {
        auto s = juce::String(i);
        apvts.addParameterListener(prefix + "p" + s + "_on", listener);
        apvts.addParameterListener(prefix + "p" + s + "_x", listener);
        apvts.addParameterListener(prefix + "p" + s + "_y", listener);
        apvts.addParameterListener(prefix + "p" + s + "_idx", listener);
        apvts.addParameterListener(prefix + "p" + s + "_idy", listener);
        apvts.addParameterListener(prefix + "p" + s + "_odx", listener);
        apvts.addParameterListener(prefix + "p" + s + "_ody", listener);
    }
}

static void removeCurveListeners(juce::AudioProcessorValueTreeState& apvts,
                                  juce::AudioProcessorValueTreeState::Listener* listener,
                                  const juce::String& prefix)
{
    apvts.removeParameterListener(prefix + "sh_dx", listener);
    apvts.removeParameterListener(prefix + "sh_dy", listener);
    apvts.removeParameterListener(prefix + "eh_dx", listener);
    apvts.removeParameterListener(prefix + "eh_dy", listener);
    for (int i = 0; i < BezierCurve::SlotValues::kMaxSlots; ++i)
    {
        auto s = juce::String(i);
        apvts.removeParameterListener(prefix + "p" + s + "_on", listener);
        apvts.removeParameterListener(prefix + "p" + s + "_x", listener);
        apvts.removeParameterListener(prefix + "p" + s + "_y", listener);
        apvts.removeParameterListener(prefix + "p" + s + "_idx", listener);
        apvts.removeParameterListener(prefix + "p" + s + "_idy", listener);
        apvts.removeParameterListener(prefix + "p" + s + "_odx", listener);
        apvts.removeParameterListener(prefix + "p" + s + "_ody", listener);
    }
}

QuirkAudioProcessor::QuirkAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    registerCurveListeners(apvts, &curveParamListener_, "rc_");
    registerCurveListeners(apvts, &curveParamListener_, "lc_");
    updateDisplayCurves();
    rebuildLUT();
    initializeKeyframesFromCurrentParams();
}

QuirkAudioProcessor::~QuirkAudioProcessor()
{
    removeCurveListeners(apvts, &curveParamListener_, "rc_");
    removeCurveListeners(apvts, &curveParamListener_, "lc_");
}

static void addCurveParams(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                           const juce::String& prefix, bool automatable)
{
    auto fid = [&](const juce::String& suffix) {
        return juce::ParameterID(prefix + suffix, 1);
    };
    auto fa = juce::AudioParameterFloatAttributes().withAutomatable(automatable);
    auto ba = juce::AudioParameterBoolAttributes().withAutomatable(automatable);

    params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("sh_dx"), prefix + "StartOut DX",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f / 3.0f, fa));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("sh_dy"), prefix + "StartOut DY",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), -1.0f, fa));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("eh_dx"), prefix + "EndIn DX",
        juce::NormalisableRange<float>(-1.0f, 0.0f, 0.001f), -1.0f / 3.0f, fa));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("eh_dy"), prefix + "EndIn DY",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), -1.0f, fa));

    for (int i = 0; i < BezierCurve::SlotValues::kMaxSlots; ++i)
    {
        auto s = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterBool>(fid("p" + s + "_on"), prefix + "P" + s + " On", false, ba));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("p" + s + "_x"), prefix + "P" + s + " X",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f, fa));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("p" + s + "_y"), prefix + "P" + s + " Y",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.5f, fa));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("p" + s + "_idx"), prefix + "P" + s + " InDX",
            juce::NormalisableRange<float>(-1.0f, 0.0f, 0.001f), 0.0f, fa));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("p" + s + "_idy"), prefix + "P" + s + " InDY",
            juce::NormalisableRange<float>(-2.0f, 2.0f, 0.001f), 0.0f, fa));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("p" + s + "_odx"), prefix + "P" + s + " OutDX",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f, fa));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(fid("p" + s + "_ody"), prefix + "P" + s + " OutDY",
            juce::NormalisableRange<float>(-2.0f, 2.0f, 0.001f), 0.0f, fa));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout QuirkAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("volume", 1), "Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("bypass", 1), "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1), "Attack",
        juce::NormalisableRange<float>(0.0f, 2000.0f, 1.0f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay",
        juce::NormalisableRange<float>(0.0f, 2000.0f, 1.0f), 200.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sustain", 1), "Sustain",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 70.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1), "Release",
        juce::NormalisableRange<float>(0.0f, 2000.0f, 1.0f), 300.0f));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("voices", 1), "Voices", 1, 10, 4));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("velocity", 1), "Velocity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    addCurveParams(params, "rc_", true);
    addCurveParams(params, "lc_", true);

    return { params.begin(), params.end() };
}

const juce::String QuirkAudioProcessor::getName() const { return JucePlugin_Name; }
bool QuirkAudioProcessor::acceptsMidi() const { return true; }
bool QuirkAudioProcessor::producesMidi() const { return false; }
bool QuirkAudioProcessor::isMidiEffect() const { return false; }
double QuirkAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int QuirkAudioProcessor::getNumPrograms() { return 1; }
int QuirkAudioProcessor::getCurrentProgram() { return 0; }
void QuirkAudioProcessor::setCurrentProgram(int) {}
const juce::String QuirkAudioProcessor::getProgramName(int) { return {}; }
void QuirkAudioProcessor::changeProgramName(int, const juce::String&) {}

void QuirkAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < VoiceAlloc::kMaxVoices; ++i)
        voices_[i].prepare(sampleRate);
    voiceBuffer_.resize(static_cast<size_t>(samplesPerBlock));
}

void QuirkAudioProcessor::releaseResources() {}

bool QuirkAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void QuirkAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    for (int ch = 0; ch < numCh; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (apvts.getRawParameterValue("bypass")->load() >= 0.5f)
    {
        midiMessages.clear();
        return;
    }

    int voiceCount = static_cast<int>(apvts.getRawParameterValue("voices")->load());

    if (voiceCount < lastVoiceCount_)
    {
        for (int i = voiceCount; i < lastVoiceCount_; ++i)
            voices_[i].noteOff(-1);
    }
    lastVoiceCount_ = voiceCount;

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            int v = VoiceAlloc::findVoice(voices_, voiceAge_, voiceCount,
                                          msg.getNoteNumber());
            voices_[v].noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
            voiceAge_[v] = nextAge_++;
            animationEngine_.noteOn(msg.getNoteNumber());
        }
        else if (msg.isNoteOff())
        {
            for (int i = 0; i < VoiceAlloc::kMaxVoices; ++i)
                voices_[i].noteOff(msg.getNoteNumber());
            animationEngine_.noteOff(msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (int i = 0; i < VoiceAlloc::kMaxVoices; ++i)
                voices_[i].noteOff(-1);
            animationEngine_.allNotesOff();
        }
    }
    midiMessages.clear();

    {
        double bpm = 0.0, ppqPos = 0.0;
        bool playheadValid = false;
        if (auto* playhead = getPlayHead())
        {
            if (auto pos = playhead->getPosition())
            {
                if (auto bpmOpt = pos->getBpm())
                    bpm = *bpmOpt;
                if (auto ppqOpt = pos->getPpqPosition())
                    ppqPos = *ppqOpt;
                playheadValid = bpm > 0.0;
            }
        }

        const bool engineWrote = animationEngine_.processBlock(numSamples, getSampleRate(),
                                                                bpm, ppqPos, playheadValid, animScratch_);
        if (engineWrote)
        {
            animationWritingInProgress_.store(true, std::memory_order_release);
            for (int i = 0; i < AnimationEngine::kNumParams; ++i)
            {
                if (auto* p = apvts.getParameter(kAnimatedParamIds[i]))
                {
                    const float clamped = juce::jlimit(0.0f, 1.0f, animScratch_[static_cast<size_t>(i)]);
                    p->setValueNotifyingHost(clamped);
                }
            }
            animationWritingInProgress_.store(false, std::memory_order_release);
        }

        const bool nowAnimating = animationEngine_.isPlaying() && animationEngine_.getAnimatedMode();
        if (previouslyAnimating_ && !nowAnimating)
        {
            loadSelectedKeyframeIntoApvts();
        }
        previouslyAnimating_ = nowAnimating;
    }

    if (curveParamsDirty_.exchange(false, std::memory_order_acquire))
        rebuildLUTFromParams();

    float attackMs  = apvts.getRawParameterValue("attack")->load();
    float decayMs   = apvts.getRawParameterValue("decay")->load();
    float sustainPct = apvts.getRawParameterValue("sustain")->load();
    float releaseMs = apvts.getRawParameterValue("release")->load();

    int readIdx = activeLutIndex_.load(std::memory_order_acquire);
    float volume = apvts.getRawParameterValue("volume")->load();
    float velocitySens = apvts.getRawParameterValue("velocity")->load();

    float* outPtr = buffer.getWritePointer(0);

    for (int v = 0; v < VoiceAlloc::kMaxVoices; ++v)
    {
        voices_[v].setADSR(attackMs, decayMs, sustainPct / 100.0f, releaseMs);
        voices_[v].setVelocitySensitivity(velocitySens);

        if (!voices_[v].isActive()) continue;

        voices_[v].renderBlock(voiceBuffer_.data(), numSamples,
                               lutBuffers_[readIdx], leftLutBuffers_[readIdx],
                               BezierCurve::kLutSize, volume);

        for (int s = 0; s < numSamples; ++s)
            outPtr[s] += voiceBuffer_[s];
    }

    for (int ch = 1; ch < numCh; ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor* QuirkAudioProcessor::createEditor()
{
    return new QuirkAudioProcessorEditor(*this);
}

bool QuirkAudioProcessor::hasEditor() const { return true; }

BezierCurve::SlotValues QuirkAudioProcessor::readSlotValues(const juce::String& prefix) const
{
    BezierCurve::SlotValues v;
    v.startOutDx = apvts.getRawParameterValue(prefix + "sh_dx")->load();
    v.startOutDy = apvts.getRawParameterValue(prefix + "sh_dy")->load();
    v.endInDx    = apvts.getRawParameterValue(prefix + "eh_dx")->load();
    v.endInDy    = apvts.getRawParameterValue(prefix + "eh_dy")->load();
    for (int i = 0; i < BezierCurve::SlotValues::kMaxSlots; ++i)
    {
        auto s = juce::String(i);
        auto& slot = v.slots[i];
        slot.on   = apvts.getRawParameterValue(prefix + "p" + s + "_on")->load() >= 0.5f;
        slot.x    = apvts.getRawParameterValue(prefix + "p" + s + "_x")->load();
        slot.y    = apvts.getRawParameterValue(prefix + "p" + s + "_y")->load();
        slot.inDx = apvts.getRawParameterValue(prefix + "p" + s + "_idx")->load();
        slot.inDy = apvts.getRawParameterValue(prefix + "p" + s + "_idy")->load();
        slot.outDx = apvts.getRawParameterValue(prefix + "p" + s + "_odx")->load();
        slot.outDy = apvts.getRawParameterValue(prefix + "p" + s + "_ody")->load();
    }
    return v;
}

void QuirkAudioProcessor::writeSlotValues(const juce::String& prefix,
                                           const BezierCurve::SlotValues& vals)
{
    auto set = [&](const juce::String& id, float v) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    set(prefix + "sh_dx", vals.startOutDx);
    set(prefix + "sh_dy", vals.startOutDy);
    set(prefix + "eh_dx", vals.endInDx);
    set(prefix + "eh_dy", vals.endInDy);
    for (int i = 0; i < BezierCurve::SlotValues::kMaxSlots; ++i)
    {
        auto s = juce::String(i);
        auto& slot = vals.slots[i];
        set(prefix + "p" + s + "_on", slot.on ? 1.0f : 0.0f);
        set(prefix + "p" + s + "_x", slot.x);
        set(prefix + "p" + s + "_y", slot.y);
        set(prefix + "p" + s + "_idx", slot.inDx);
        set(prefix + "p" + s + "_idy", slot.inDy);
        set(prefix + "p" + s + "_odx", slot.outDx);
        set(prefix + "p" + s + "_ody", slot.outDy);
    }
}

int QuirkAudioProcessor::findFreeSlot(const juce::String& prefix) const
{
    for (int i = 0; i < BezierCurve::SlotValues::kMaxSlots; ++i)
    {
        auto s = juce::String(i);
        if (apvts.getRawParameterValue(prefix + "p" + s + "_on")->load() < 0.5f)
            return i;
    }
    return -1;
}

void QuirkAudioProcessor::initializeKeyframesFromCurrentParams()
{
    AnimationEngine::AnimatedParams snapshot{};
    for (int i = 0; i < AnimationEngine::kNumParams; ++i)
    {
        if (auto* p = apvts.getParameter(kAnimatedParamIds[i]))
            snapshot[static_cast<size_t>(i)] = p->getValue();
    }
    for (int f = 0; f < AnimationEngine::kNumKeyframes; ++f)
        animationEngine_.writeKeyframe(f, snapshot);
}

void QuirkAudioProcessor::loadSelectedKeyframeIntoApvts()
{
    AnimationEngine::AnimatedParams snapshot{};
    animationEngine_.getKeyframeValues(animationEngine_.getSelectedKeyframe(), snapshot);
    for (int i = 0; i < AnimationEngine::kNumParams; ++i)
    {
        if (auto* p = apvts.getParameter(kAnimatedParamIds[i]))
        {
            const float clamped = juce::jlimit(0.0f, 1.0f, snapshot[static_cast<size_t>(i)]);
            p->setValueNotifyingHost(clamped);
        }
    }
}

void QuirkAudioProcessor::captureCurrentToSelectedKeyframe()
{
    AnimationEngine::AnimatedParams snapshot{};
    for (int i = 0; i < AnimationEngine::kNumParams; ++i)
    {
        if (auto* p = apvts.getParameter(kAnimatedParamIds[i]))
            snapshot[static_cast<size_t>(i)] = p->getValue();
    }
    animationEngine_.writeKeyframe(animationEngine_.getSelectedKeyframe(), snapshot);
}

void QuirkAudioProcessor::updateDisplayCurves()
{
    auto rcVals = readSlotValues("rc_");
    BezierCurve::buildFromSlots(bezierCurve_, rcVals);
    auto lcVals = readSlotValues("lc_");
    BezierCurve::buildFromSlots(leftBezierCurve_, lcVals);
}

void QuirkAudioProcessor::rebuildLUTFromParams()
{
    BezierCurve tempRC, tempLC;
    auto rcVals = readSlotValues("rc_");
    BezierCurve::buildFromSlots(tempRC, rcVals);
    auto lcVals = readSlotValues("lc_");
    BezierCurve::buildFromSlots(tempLC, lcVals);

    int current = activeLutIndex_.load(std::memory_order_acquire);
    int writeIdx = 1 - current;
    tempRC.generateLUT(lutBuffers_[writeIdx]);
    tempLC.generateLUT(leftLutBuffers_[writeIdx]);
    activeLutIndex_.store(writeIdx, std::memory_order_release);
    curveVersion_.fetch_add(1, std::memory_order_relaxed);
}

void QuirkAudioProcessor::rebuildLUT()
{
    int current = activeLutIndex_.load(std::memory_order_acquire);
    int writeIdx = 1 - current;
    bezierCurve_.generateLUT(lutBuffers_[writeIdx]);
    leftBezierCurve_.generateLUT(leftLutBuffers_[writeIdx]);
    activeLutIndex_.store(writeIdx, std::memory_order_release);
    curveVersion_.fetch_add(1, std::memory_order_relaxed);
}

static BezierCurve::SlotValues legacyTreeToSlots(const juce::ValueTree& child)
{
    BezierCurve::SlotValues v;

    auto startH = child.getChildWithName("StartHandle");
    if (startH.isValid())
    {
        v.startOutDx = static_cast<float>(startH.getProperty("outDx", 1.0f / 3.0f));
        v.startOutDy = static_cast<float>(startH.getProperty("outDy", -1.0f));
    }

    auto endH = child.getChildWithName("EndHandle");
    if (endH.isValid())
    {
        v.endInDx = static_cast<float>(endH.getProperty("inDx", -1.0f / 3.0f));
        v.endInDy = static_cast<float>(endH.getProperty("inDy", -1.0f));
    }

    int slotIdx = 0;
    for (int i = 0; i < child.getNumChildren() && slotIdx < BezierCurve::SlotValues::kMaxSlots; ++i)
    {
        auto ch = child.getChild(i);
        if (ch.hasType("Point"))
        {
            auto& slot = v.slots[slotIdx];
            slot.on = true;
            slot.x = static_cast<float>(ch.getProperty("x", 0.5f));
            slot.y = static_cast<float>(ch.getProperty("y", 0.5f));
            slot.inDx = static_cast<float>(ch.getProperty("inDx", 0.0f));
            slot.inDy = static_cast<float>(ch.getProperty("inDy", 0.0f));
            slot.outDx = static_cast<float>(ch.getProperty("outDx", 0.0f));
            slot.outDy = static_cast<float>(ch.getProperty("outDy", 0.0f));
            ++slotIdx;
        }
    }

    return v;
}

static juce::ValueTree animationToValueTree(AnimationEngine& engine)
{
    juce::ValueTree tree("Animation");
    tree.setProperty("animatedMode",   engine.getAnimatedMode() ? 1 : 0, nullptr);
    tree.setProperty("hostClock",      engine.getHostClock()    ? 1 : 0, nullptr);
    tree.setProperty("selectedFrame",  engine.getSelectedKeyframe(),     nullptr);

    for (int t = 0; t < AnimationEngine::kNumTransitions; ++t)
    {
        tree.setProperty("internalMs" + juce::String(t),
                         engine.getInternalDuration(t), nullptr);
        tree.setProperty("hostDiv" + juce::String(t),
                         static_cast<int>(engine.getHostDuration(t)), nullptr);
    }

    AnimationEngine::AnimatedParams snapshot{};
    for (int f = 0; f < AnimationEngine::kNumKeyframes; ++f)
    {
        engine.getKeyframeValues(f, snapshot);
        juce::ValueTree frame("Frame");
        frame.setProperty("index", f, nullptr);
        for (int p = 0; p < AnimationEngine::kNumParams; ++p)
            frame.setProperty("p" + juce::String(p),
                              snapshot[static_cast<size_t>(p)], nullptr);
        tree.appendChild(frame, nullptr);
    }
    return tree;
}

static void animationFromValueTree(AnimationEngine& engine, const juce::ValueTree& tree)
{
    if (!tree.isValid()) return;
    engine.setAnimatedMode(static_cast<int>(tree.getProperty("animatedMode", 1)) != 0);
    engine.setHostClock   (static_cast<int>(tree.getProperty("hostClock",    0)) != 0);
    engine.selectKeyframe (static_cast<int>(tree.getProperty("selectedFrame", 0)));

    for (int t = 0; t < AnimationEngine::kNumTransitions; ++t)
    {
        if (tree.hasProperty("internalMs" + juce::String(t)))
            engine.setInternalDuration(t, static_cast<float>(tree.getProperty("internalMs" + juce::String(t))));
        if (tree.hasProperty("hostDiv" + juce::String(t)))
        {
            int v = static_cast<int>(tree.getProperty("hostDiv" + juce::String(t)));
            if (v >= 0 && v < static_cast<int>(HostDivision::NumDivisions))
                engine.setHostDuration(t, static_cast<HostDivision>(v));
        }
    }

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto frame = tree.getChild(i);
        if (!frame.hasType("Frame")) continue;
        int idx = static_cast<int>(frame.getProperty("index", -1));
        if (idx < 0 || idx >= AnimationEngine::kNumKeyframes) continue;
        AnimationEngine::AnimatedParams snapshot{};
        for (int p = 0; p < AnimationEngine::kNumParams; ++p)
        {
            auto key = "p" + juce::String(p);
            if (frame.hasProperty(key))
                snapshot[static_cast<size_t>(p)] = static_cast<float>(frame.getProperty(key));
        }
        engine.writeKeyframe(idx, snapshot);
    }
}

void QuirkAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.appendChild(animationToValueTree(animationEngine_), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void QuirkAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml(*xml);

        BezierCurve::SlotValues legacyRC, legacyLC;
        bool hasLegacyRC = false, hasLegacyLC = false;

        auto bezierChild = tree.getChildWithName("BezierCurve");
        if (bezierChild.isValid())
        {
            legacyRC = legacyTreeToSlots(bezierChild);
            hasLegacyRC = true;
            tree.removeChild(bezierChild, nullptr);
        }

        auto leftChild = tree.getChildWithName("LeftBezierCurve");
        if (leftChild.isValid())
        {
            legacyLC = legacyTreeToSlots(leftChild);
            hasLegacyLC = true;
            tree.removeChild(leftChild, nullptr);
        }

        auto animChild = tree.getChildWithName("Animation");
        bool hasAnim = animChild.isValid();
        juce::ValueTree animChildCopy;
        if (hasAnim)
        {
            animChildCopy = animChild.createCopy();
            tree.removeChild(animChild, nullptr);
        }

        apvts.replaceState(tree);

        if (hasLegacyRC) writeSlotValues("rc_", legacyRC);
        if (hasLegacyLC) writeSlotValues("lc_", legacyLC);

        if (hasAnim)
        {
            animationFromValueTree(animationEngine_, animChildCopy);
            loadSelectedKeyframeIntoApvts();
        }
        else
        {
            initializeKeyframesFromCurrentParams();
        }

        updateDisplayCurves();
        rebuildLUT();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuirkAudioProcessor();
}
