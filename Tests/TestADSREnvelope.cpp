#include <juce_core/juce_core.h>
#include "DSP/ADSREnvelope.h"
#include <cmath>

class ADSREnvelopeTests : public juce::UnitTest
{
public:
    ADSREnvelopeTests() : juce::UnitTest("ADSREnvelope") {}

    void runTest() override
    {
        beginTest("starts idle");
        {
            ADSREnvelope env;
            env.setSampleRate(44100.0);
            expect(!env.isActive());
            expectEquals(static_cast<int>(env.getState()), static_cast<int>(ADSREnvelope::State::Idle));
            expectWithinAbsoluteError(env.getNextSample(), 0.0f, 0.001f);
        }

        beginTest("note on triggers attack");
        {
            ADSREnvelope env;
            env.setSampleRate(1000.0);
            env.setParameters(100.0f, 0.0f, 1.0f, 0.0f);
            env.noteOn();
            expect(env.isActive());
            expectEquals(static_cast<int>(env.getState()), static_cast<int>(ADSREnvelope::State::Attack));
        }

        beginTest("linear attack ramps from 0 to 1");
        {
            ADSREnvelope env;
            env.setSampleRate(1000.0);
            env.setParameters(10.0f, 0.0f, 1.0f, 0.0f);
            env.noteOn();

            for (int i = 0; i < 5; ++i)
                env.getNextSample();
            expectWithinAbsoluteError(env.getNextSample(), 0.6f, 0.05f);

            for (int i = 0; i < 4; ++i)
                env.getNextSample();

            expectEquals(static_cast<int>(env.getState()), static_cast<int>(ADSREnvelope::State::Sustain));
        }

        beginTest("zero attack jumps to decay/sustain");
        {
            ADSREnvelope env;
            env.setSampleRate(44100.0);
            env.setParameters(0.0f, 0.0f, 0.8f, 0.0f);
            env.noteOn();
            float val = env.getNextSample();
            expectWithinAbsoluteError(val, 0.8f, 0.01f);
        }

        beginTest("linear decay from 1 to sustain level");
        {
            ADSREnvelope env;
            env.setSampleRate(1000.0);
            env.setParameters(0.0f, 10.0f, 0.5f, 0.0f);
            env.noteOn();

            for (int i = 0; i < 5; ++i)
                env.getNextSample();
            float mid = env.getNextSample();
            expect(mid > 0.5f && mid < 1.0f);

            for (int i = 0; i < 10; ++i)
                env.getNextSample();
            expectEquals(static_cast<int>(env.getState()), static_cast<int>(ADSREnvelope::State::Sustain));
            expectWithinAbsoluteError(env.getNextSample(), 0.5f, 0.02f);
        }

        beginTest("sustain holds level");
        {
            ADSREnvelope env;
            env.setSampleRate(1000.0);
            env.setParameters(0.0f, 0.0f, 0.7f, 0.0f);
            env.noteOn();

            for (int i = 0; i < 100; ++i)
            {
                float val = env.getNextSample();
                expectWithinAbsoluteError(val, 0.7f, 0.01f);
            }
            expect(env.isActive());
        }

        beginTest("note off triggers release");
        {
            ADSREnvelope env;
            env.setSampleRate(1000.0);
            env.setParameters(0.0f, 0.0f, 1.0f, 20.0f);
            env.noteOn();
            env.getNextSample();
            env.noteOff();

            expectEquals(static_cast<int>(env.getState()), static_cast<int>(ADSREnvelope::State::Release));

            for (int i = 0; i < 10; ++i)
                env.getNextSample();
            float mid = env.getNextSample();
            expect(mid > 0.0f && mid < 1.0f);

            for (int i = 0; i < 20; ++i)
                env.getNextSample();
            expect(!env.isActive());
        }

        beginTest("zero release jumps to idle");
        {
            ADSREnvelope env;
            env.setSampleRate(44100.0);
            env.setParameters(0.0f, 0.0f, 1.0f, 0.0f);
            env.noteOn();
            env.getNextSample();
            env.noteOff();
            float val = env.getNextSample();
            expectWithinAbsoluteError(val, 0.0f, 0.001f);
            expect(!env.isActive());
        }

        beginTest("retrigger during release restarts attack");
        {
            ADSREnvelope env;
            env.setSampleRate(1000.0);
            env.setParameters(10.0f, 0.0f, 1.0f, 100.0f);
            env.noteOn();
            for (int i = 0; i < 20; ++i)
                env.getNextSample();
            env.noteOff();
            for (int i = 0; i < 5; ++i)
                env.getNextSample();

            float levelBeforeRetrigger = env.getNextSample();
            expect(levelBeforeRetrigger > 0.0f);

            env.noteOn();
            expectEquals(static_cast<int>(env.getState()), static_cast<int>(ADSREnvelope::State::Attack));
            float afterRetrigger = env.getNextSample();
            expect(afterRetrigger >= levelBeforeRetrigger);
        }

        beginTest("sample rate affects timing");
        {
            ADSREnvelope env1, env2;
            env1.setSampleRate(1000.0);
            env2.setSampleRate(2000.0);
            env1.setParameters(10.0f, 0.0f, 1.0f, 0.0f);
            env2.setParameters(10.0f, 0.0f, 1.0f, 0.0f);
            env1.noteOn();
            env2.noteOn();

            env1.getNextSample();
            env2.getNextSample();
            float v1 = env1.getNextSample();
            float v2 = env2.getNextSample();
            expect(v2 < v1);
        }

        beginTest("sustain at zero means silence after decay");
        {
            ADSREnvelope env;
            env.setSampleRate(1000.0);
            env.setParameters(0.0f, 10.0f, 0.0f, 0.0f);
            env.noteOn();
            for (int i = 0; i < 20; ++i)
                env.getNextSample();
            expectWithinAbsoluteError(env.getNextSample(), 0.0f, 0.01f);
            expect(env.isActive());
        }

        beginTest("full ADSR cycle");
        {
            ADSREnvelope env;
            env.setSampleRate(1000.0);
            env.setParameters(5.0f, 5.0f, 0.6f, 10.0f);
            env.noteOn();

            float peak = 0.0f;
            for (int i = 0; i < 5; ++i)
            {
                float v = env.getNextSample();
                if (v > peak) peak = v;
            }
            expect(peak > 0.8f);

            for (int i = 0; i < 10; ++i)
                env.getNextSample();
            float sustainVal = env.getNextSample();
            expectWithinAbsoluteError(sustainVal, 0.6f, 0.05f);

            env.noteOff();
            for (int i = 0; i < 15; ++i)
                env.getNextSample();
            expect(!env.isActive());
        }
    }
};

static ADSREnvelopeTests adsrEnvelopeTests;
