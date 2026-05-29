#pragma once

#include <cmath>

class Oscillator
{
public:
    void prepare(double sampleRate);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void renderBlock(float* output, int numSamples,
                     const float* lut, const float* leftLut, int lutSize,
                     float gain, float volume);
    bool isActive() const { return noteActive_; }

private:
    double sampleRate_ = 44100.0;
    double phase_ = 0.0;
    double phaseIncrement_ = 0.0;
    float velocity_ = 0.0f;
    bool noteActive_ = false;
    int currentNote_ = -1;

    static double midiNoteToFreq(int note);
    static float polyBlep(double t, double dt);
    static float lookupLUT(float x, const float* lut, const float* leftLut, int lutSize);
};
