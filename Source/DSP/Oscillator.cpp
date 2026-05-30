#include "DSP/Oscillator.h"
#include <algorithm>

void Oscillator::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    phase_ = 0.0;
    envelope_.setSampleRate(sampleRate);
}

void Oscillator::noteOn(int midiNote, float velocity)
{
    currentNote_ = midiNote;
    velocity_ = velocity;
    phaseIncrement_ = midiNoteToFreq(midiNote) / sampleRate_;
    envelope_.noteOn();
}

void Oscillator::noteOff(int midiNote)
{
    if (midiNote == currentNote_ || midiNote < 0)
        envelope_.noteOff();
}

void Oscillator::setADSR(float attackMs, float decayMs, float sustainLevel, float releaseMs)
{
    envelope_.setParameters(attackMs, decayMs, sustainLevel, releaseMs);
}

double Oscillator::midiNoteToFreq(int note)
{
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

float Oscillator::lookupLUT(float x, const float* lut, const float* leftLut, int lutSize)
{
    if (lut == nullptr) return x;

    const float ax = std::abs(x);
    const float clamped = std::min(ax, 1.0f);
    const float* table = (x >= 0.0f) ? lut : leftLut;

    const float pos = clamped * static_cast<float>(lutSize - 1);
    const int idx = std::min(static_cast<int>(pos), lutSize - 2);
    const float frac = pos - static_cast<float>(idx);
    const float y = table[idx] + frac * (table[idx + 1] - table[idx]);

    return (x >= 0.0f) ? y : -y;
}

void Oscillator::renderBlock(float* output, int numSamples,
                              const float* lut, const float* leftLut, int lutSize,
                              float volume)
{
    if (!envelope_.isActive())
    {
        std::fill(output, output + numSamples, 0.0f);
        return;
    }

    const float effectiveVel = (1.0f - velocitySensitivity_) + velocitySensitivity_ * velocity_;
    const float amp = effectiveVel * volume;

    for (int i = 0; i < numSamples; ++i)
    {
        float envVal = envelope_.getNextSample();
        if (!envelope_.isActive())
        {
            std::fill(output + i, output + numSamples, 0.0f);
            return;
        }

        float x = static_cast<float>(2.0 * phase_ - 1.0);
        output[i] = lookupLUT(x, lut, leftLut, lutSize) * amp * envVal;

        phase_ += phaseIncrement_;
        if (phase_ >= 1.0) phase_ -= 1.0;
    }
}
