#pragma once

#include <array>
#include <atomic>
#include <juce_core/juce_core.h>

enum class HostDivision
{
    Div1_32 = 0,
    Div1_16,
    Div1_8,
    Div1_4,
    Div1_2,
    Div1_1,
    Div1_8d,
    Div1_4d,
    Div1_2d,
    Div1_16t,
    Div1_8t,
    Div1_4t,
    Div1_2t,
    NumDivisions
};

inline double divisionToQuarterNotes(HostDivision d)
{
    switch (d)
    {
        case HostDivision::Div1_32:  return 0.125;
        case HostDivision::Div1_16:  return 0.25;
        case HostDivision::Div1_8:   return 0.5;
        case HostDivision::Div1_4:   return 1.0;
        case HostDivision::Div1_2:   return 2.0;
        case HostDivision::Div1_1:   return 4.0;
        case HostDivision::Div1_8d:  return 0.75;
        case HostDivision::Div1_4d:  return 1.5;
        case HostDivision::Div1_2d:  return 3.0;
        case HostDivision::Div1_16t: return 1.0 / 6.0;
        case HostDivision::Div1_8t:  return 1.0 / 3.0;
        case HostDivision::Div1_4t:  return 2.0 / 3.0;
        case HostDivision::Div1_2t:  return 4.0 / 3.0;
        default:                     return 1.0;
    }
}

class AnimationEngine
{
public:
    static constexpr int kNumKeyframes   = 4;
    static constexpr int kNumTransitions = 4;
    static constexpr int kNumParams      = 56;

    static constexpr int kIdxVolume   = 0;
    static constexpr int kIdxAttack   = 1;
    static constexpr int kIdxDecay    = 2;
    static constexpr int kIdxSustain  = 3;
    static constexpr int kIdxRelease  = 4;
    static constexpr int kIdxVelocity = 5;

    static constexpr int kCurveBase    = 6;
    static constexpr int kPerCurve     = 25;
    static constexpr int kRcBase       = kCurveBase;
    static constexpr int kLcBase       = kCurveBase + kPerCurve;
    static constexpr int kEndpointsPerCurve = 4;
    static constexpr int kSlotsPerCurve     = 3;
    static constexpr int kFloatsPerSlot     = 7;

    using AnimatedParams = std::array<float, kNumParams>;

    AnimationEngine();

    void setAnimatedMode(bool animated);
    bool getAnimatedMode() const { return animatedMode_.load(std::memory_order_acquire); }

    void setHostClock(bool host);
    bool getHostClock() const { return hostClock_.load(std::memory_order_acquire); }

    void selectKeyframe(int idx);
    int  getSelectedKeyframe() const { return selectedKeyframe_.load(std::memory_order_acquire); }

    void  setInternalDuration(int transitionIdx, float ms);
    float getInternalDuration(int transitionIdx) const;
    void  setHostDuration(int transitionIdx, HostDivision div);
    HostDivision getHostDuration(int transitionIdx) const;

    void  setKeyframeValue(int frame, int paramIdx, float value);
    float getKeyframeValue(int frame, int paramIdx) const;
    void  getKeyframeValues(int frame, AnimatedParams& out) const;
    void  writeKeyframe(int frame, const AnimatedParams& values);

    void noteOn(int pitch);
    void noteOff(int pitch);
    void allNotesOff();
    bool isPlaying() const { return isAnimating_.load(std::memory_order_acquire); }
    int  getActiveNoteCount() const { return activeNoteCount_; }

    bool processBlock(int numSamples, double sampleRate,
                      double bpm, double ppqPos, bool playheadValid,
                      AnimatedParams& outValues);

    int    getCurrentTransition() const { return currentTransition_; }
    double getTransitionProgress() const { return phaseProgress_; }

    static bool isSlotOnIndex(int paramIdx);

private:
    void resetPhase();

    mutable juce::SpinLock keyframeLock_;
    AnimatedParams keyframes_[kNumKeyframes]{};

    float internalDurationsMs_[kNumTransitions];
    HostDivision hostDurations_[kNumTransitions];

    std::atomic<bool> animatedMode_ { true };
    std::atomic<bool> hostClock_    { false };
    std::atomic<int>  selectedKeyframe_ { 0 };
    std::atomic<bool> isAnimating_      { false };

    std::array<bool, 128> activeNotes_{};
    int activeNoteCount_   = 0;
    int currentTransition_ = 0;
    double phaseProgress_  = 0.0;
    double lastPpqPos_     = 0.0;
    bool phaseJustReset_   = false;
};
