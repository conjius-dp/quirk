#pragma once

#include "DSP/Oscillator.h"

namespace VoiceAlloc
{
    static constexpr int kMaxVoices = 10;

    inline int findVoice(const Oscillator* voices, const int* ages, int voiceCount, int note)
    {
        for (int i = 0; i < voiceCount; ++i)
            if (voices[i].isActive() && voices[i].getCurrentNote() == note)
                return i;
        for (int i = 0; i < voiceCount; ++i)
            if (!voices[i].isActive())
                return i;
        int oldest = 0;
        for (int i = 1; i < voiceCount; ++i)
            if (ages[i] < ages[oldest])
                oldest = i;
        return oldest;
    }
}
