#include "DSP/AnimationEngine.h"

#include <algorithm>
#include <cmath>

AnimationEngine::AnimationEngine()
{
    for (int i = 0; i < kNumTransitions; ++i)
    {
        internalDurationsMs_[i] = 1000.0f;
        hostDurations_[i]       = HostDivision::Div1_4;
    }
}

bool AnimationEngine::isSlotOnIndex(int paramIdx)
{
    if (paramIdx < kCurveBase) return false;
    const int curveLocal = (paramIdx - kCurveBase) % kPerCurve;
    if (curveLocal < kEndpointsPerCurve) return false;
    const int slotLocal = (curveLocal - kEndpointsPerCurve) % kFloatsPerSlot;
    return slotLocal == 0;
}

void AnimationEngine::resetPhase()
{
    currentTransition_ = 0;
    phaseProgress_     = 0.0;
}

void AnimationEngine::setAnimatedMode(bool animated)
{
    animatedMode_.store(animated, std::memory_order_release);
    if (!animated)
    {
        selectedKeyframe_.store(0, std::memory_order_release);
        resetPhase();
    }
}

void AnimationEngine::setHostClock(bool host)
{
    hostClock_.store(host, std::memory_order_release);
}

void AnimationEngine::selectKeyframe(int idx)
{
    if (idx < 0 || idx >= kNumKeyframes) return;
    selectedKeyframe_.store(idx, std::memory_order_release);
}

void AnimationEngine::setInternalDuration(int transitionIdx, float ms)
{
    if (transitionIdx < 0 || transitionIdx >= kNumTransitions) return;
    internalDurationsMs_[transitionIdx] = std::max(1.0f, ms);
}

float AnimationEngine::getInternalDuration(int transitionIdx) const
{
    if (transitionIdx < 0 || transitionIdx >= kNumTransitions) return 0.0f;
    return internalDurationsMs_[transitionIdx];
}

void AnimationEngine::setHostDuration(int transitionIdx, HostDivision div)
{
    if (transitionIdx < 0 || transitionIdx >= kNumTransitions) return;
    hostDurations_[transitionIdx] = div;
}

HostDivision AnimationEngine::getHostDuration(int transitionIdx) const
{
    if (transitionIdx < 0 || transitionIdx >= kNumTransitions) return HostDivision::Div1_4;
    return hostDurations_[transitionIdx];
}

void AnimationEngine::setKeyframeValue(int frame, int paramIdx, float value)
{
    if (frame < 0 || frame >= kNumKeyframes) return;
    if (paramIdx < 0 || paramIdx >= kNumParams) return;
    const juce::SpinLock::ScopedLockType lock(keyframeLock_);
    keyframes_[frame][static_cast<size_t>(paramIdx)] = value;
}

float AnimationEngine::getKeyframeValue(int frame, int paramIdx) const
{
    if (frame < 0 || frame >= kNumKeyframes) return 0.0f;
    if (paramIdx < 0 || paramIdx >= kNumParams) return 0.0f;
    const juce::SpinLock::ScopedLockType lock(keyframeLock_);
    return keyframes_[frame][static_cast<size_t>(paramIdx)];
}

void AnimationEngine::getKeyframeValues(int frame, AnimatedParams& out) const
{
    if (frame < 0 || frame >= kNumKeyframes) { out.fill(0.0f); return; }
    const juce::SpinLock::ScopedLockType lock(keyframeLock_);
    out = keyframes_[frame];
}

void AnimationEngine::writeKeyframe(int frame, const AnimatedParams& values)
{
    if (frame < 0 || frame >= kNumKeyframes) return;
    const juce::SpinLock::ScopedLockType lock(keyframeLock_);
    keyframes_[frame] = values;
}

void AnimationEngine::noteOn(int pitch)
{
    if (pitch < 0 || pitch >= 128) return;
    if (activeNotes_[static_cast<size_t>(pitch)]) return;
    activeNotes_[static_cast<size_t>(pitch)] = true;
    ++activeNoteCount_;
    if (activeNoteCount_ == 1)
    {
        resetPhase();
        phaseJustReset_ = true;
        isAnimating_.store(true, std::memory_order_release);
    }
}

void AnimationEngine::noteOff(int pitch)
{
    if (pitch < 0 || pitch >= 128) return;
    if (!activeNotes_[static_cast<size_t>(pitch)]) return;
    activeNotes_[static_cast<size_t>(pitch)] = false;
    --activeNoteCount_;
    if (activeNoteCount_ <= 0)
    {
        activeNoteCount_ = 0;
        isAnimating_.store(false, std::memory_order_release);
    }
}

void AnimationEngine::allNotesOff()
{
    activeNotes_.fill(false);
    activeNoteCount_ = 0;
    isAnimating_.store(false, std::memory_order_release);
}

bool AnimationEngine::processBlock(int numSamples, double sampleRate,
                                    double bpm, double ppqPos, bool playheadValid,
                                    AnimatedParams& outValues)
{
    const bool animated = animatedMode_.load(std::memory_order_acquire);
    if (!animated) return false;

    if (!isAnimating_.load(std::memory_order_acquire)) return false;

    const juce::SpinLock::ScopedTryLockType lock(keyframeLock_);
    if (!lock.isLocked()) return false;

    const bool useHost = hostClock_.load(std::memory_order_acquire) && playheadValid && bpm > 0.0;

    double advance = 0.0;
    if (useHost)
    {
        if (phaseJustReset_)
        {
            lastPpqPos_ = ppqPos;
            phaseJustReset_ = false;
        }
        else
        {
            const double qnPerTransition = divisionToQuarterNotes(hostDurations_[currentTransition_]);
            if (qnPerTransition > 0.0)
            {
                const double deltaQn = ppqPos - lastPpqPos_;
                advance = deltaQn / qnPerTransition;
            }
            lastPpqPos_ = ppqPos;
        }
    }
    else
    {
        phaseJustReset_ = false;
        const double durationSec = static_cast<double>(internalDurationsMs_[currentTransition_]) * 0.001;
        if (durationSec > 0.0 && sampleRate > 0.0)
            advance = static_cast<double>(numSamples) / (sampleRate * durationSec);
    }

    phaseProgress_ += advance;

    int safety = 0;
    while (phaseProgress_ >= 1.0 && safety++ < kNumTransitions + 1)
    {
        phaseProgress_ -= 1.0;
        currentTransition_ = (currentTransition_ + 1) % kNumTransitions;
    }
    if (phaseProgress_ < 0.0) phaseProgress_ = 0.0;

    const int fromFrame = currentTransition_;
    const int toFrame   = (currentTransition_ + 1) % kNumKeyframes;
    const auto& from = keyframes_[fromFrame];
    const auto& to   = keyframes_[toFrame];
    const float t = static_cast<float>(phaseProgress_);

    for (int i = 0; i < kNumParams; ++i)
    {
        if (isSlotOnIndex(i))
            outValues[static_cast<size_t>(i)] = from[static_cast<size_t>(i)];
        else
            outValues[static_cast<size_t>(i)] = from[static_cast<size_t>(i)] + t * (to[static_cast<size_t>(i)] - from[static_cast<size_t>(i)]);
    }

    return true;
}
