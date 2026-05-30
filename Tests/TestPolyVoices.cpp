#include <juce_core/juce_core.h>
#include "DSP/Oscillator.h"
#include "DSP/VoiceAllocator.h"

class TestPolyVoices : public juce::UnitTest
{
public:
    TestPolyVoices() : juce::UnitTest("PolyVoices") {}

    void prepareVoices(Oscillator* voices, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            voices[i].prepare(44100.0);
            voices[i].setADSR(0.0f, 0.0f, 1.0f, 0.0f);
        }
    }

    void drainVoice(Oscillator& voice)
    {
        float buf[4];
        float lut[4] = { 0.0f, 0.333f, 0.667f, 1.0f };
        voice.renderBlock(buf, 4, lut, lut, 4, 1.0f);
    }

    void runTest() override
    {
        Oscillator voices[VoiceAlloc::kMaxVoices];
        int ages[VoiceAlloc::kMaxVoices] = {};
        int nextAge = 0;

        beginTest("free voice selected when all inactive");
        {
            prepareVoices(voices, VoiceAlloc::kMaxVoices);

            int v = VoiceAlloc::findVoice(voices, ages, 4, 60);
            expect(v == 0);
        }

        beginTest("retrigger same note returns existing voice");
        {
            voices[1].noteOn(60, 1.0f);
            ages[1] = nextAge++;
            drainVoice(voices[1]);

            int v = VoiceAlloc::findVoice(voices, ages, 4, 60);
            expect(v == 1);

            voices[1].noteOff(60);
            drainVoice(voices[1]);
        }

        beginTest("allocates next free voice for different notes");
        {
            for (int i = 0; i < VoiceAlloc::kMaxVoices; ++i)
            {
                voices[i].noteOff(-1);
                drainVoice(voices[i]);
            }

            voices[0].noteOn(60, 1.0f);
            ages[0] = nextAge++;
            drainVoice(voices[0]);

            voices[1].noteOn(62, 1.0f);
            ages[1] = nextAge++;
            drainVoice(voices[1]);

            int v = VoiceAlloc::findVoice(voices, ages, 4, 64);
            expect(v == 2);
        }

        beginTest("steals oldest when all voices used");
        {
            voices[2].noteOn(64, 1.0f);
            ages[2] = nextAge++;
            drainVoice(voices[2]);

            voices[3].noteOn(65, 1.0f);
            ages[3] = nextAge++;
            drainVoice(voices[3]);

            int v = VoiceAlloc::findVoice(voices, ages, 4, 67);
            expect(v == 0);
        }

        beginTest("voice count limits allocation range");
        {
            for (int i = 0; i < VoiceAlloc::kMaxVoices; ++i)
            {
                voices[i].noteOff(-1);
                drainVoice(voices[i]);
            }

            voices[0].noteOn(60, 1.0f);
            ages[0] = nextAge++;
            drainVoice(voices[0]);

            voices[1].noteOn(62, 1.0f);
            ages[1] = nextAge++;
            drainVoice(voices[1]);

            int v = VoiceAlloc::findVoice(voices, ages, 2, 64);
            expect(v == 0);
        }

        beginTest("inactive voice preferred over stealing");
        {
            for (int i = 0; i < VoiceAlloc::kMaxVoices; ++i)
            {
                voices[i].noteOff(-1);
                drainVoice(voices[i]);
            }

            voices[0].noteOn(60, 1.0f);
            ages[0] = nextAge++;
            drainVoice(voices[0]);

            voices[2].noteOn(64, 1.0f);
            ages[2] = nextAge++;
            drainVoice(voices[2]);

            int v = VoiceAlloc::findVoice(voices, ages, 4, 67);
            expect(v == 1);
        }

        beginTest("retrigger preferred over free voice");
        {
            for (int i = 0; i < VoiceAlloc::kMaxVoices; ++i)
            {
                voices[i].noteOff(-1);
                drainVoice(voices[i]);
            }

            voices[2].noteOn(60, 1.0f);
            ages[2] = nextAge++;
            drainVoice(voices[2]);

            int v = VoiceAlloc::findVoice(voices, ages, 4, 60);
            expect(v == 2);
        }
    }
};

static TestPolyVoices testPolyVoices;
