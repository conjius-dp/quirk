#pragma once

#include <algorithm>
#include <cmath>
#include "DSP/ADSREnvelope.h"

class Oscillator
{
public:
    void prepare(double sampleRate);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void setADSR(float attackMs, float decayMs, float sustainLevel, float releaseMs);
    void setVelocitySensitivity(float s) { velocitySensitivity_ = std::clamp(s, 0.0f, 1.0f); }
    void renderBlock(float* output, int numSamples,
                     const float* lut, const float* leftLut, int lutSize,
                     float volume);
    bool isActive() const { return envelope_.isActive(); }
    int getCurrentNote() const { return currentNote_; }

private:
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    double phaseIncrement_ = 0.0;
    float velocity_ = 0.0f;
    float velocitySensitivity_ = 1.0f;
    int currentNote_ = -1;
    ADSREnvelope envelope_;

    static double midiNoteToFreq(int note);
    static float lookupLUT(float x, const float* lut, const float* leftLut, int lutSize);
};
