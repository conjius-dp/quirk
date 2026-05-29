#include "DSP/Oscillator.h"
#include <algorithm>
#include <cmath>

void Oscillator::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    phase_ = 0.0;
}

void Oscillator::noteOn(int midiNote, float velocity)
{
    currentNote_ = midiNote;
    velocity_ = velocity;
    noteActive_ = true;
    phaseIncrement_ = midiNoteToFreq(midiNote) / sampleRate_;
}

void Oscillator::noteOff(int midiNote)
{
    if (midiNote == currentNote_)
        noteActive_ = false;
}

double Oscillator::midiNoteToFreq(int note)
{
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

float Oscillator::polyBlep(double t, double dt)
{
    if (t < dt)
    {
        double n = t / dt;
        return static_cast<float>(n + n - n * n - 1.0);
    }
    if (t > 1.0 - dt)
    {
        double n = (t - 1.0) / dt;
        return static_cast<float>(n * n + n + n + 1.0);
    }
    return 0.0f;
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
                              float gain, float volume)
{
    if (!noteActive_)
    {
        std::fill(output, output + numSamples, 0.0f);
        return;
    }

    const float amp = velocity_ * volume;
    const float g = std::pow(100.0f, gain / 100.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        float saw = static_cast<float>(2.0 * phase_ - 1.0);
        saw -= polyBlep(phase_, phaseIncrement_);

        float shaped = lookupLUT(std::clamp(saw * g, -1.0f, 1.0f), lut, leftLut, lutSize);
        output[i] = shaped * amp;

        phase_ += phaseIncrement_;
        if (phase_ >= 1.0) phase_ -= 1.0;
    }
}
