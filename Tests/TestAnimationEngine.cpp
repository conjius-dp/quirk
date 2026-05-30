#include <juce_core/juce_core.h>
#include "DSP/AnimationEngine.h"
#include <cmath>

class AnimationEngineTests : public juce::UnitTest
{
public:
    AnimationEngineTests() : juce::UnitTest("AnimationEngine") {}

    void runTest() override
    {
        beginTest("HostDivision straight values");
        {
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_32), 0.125, 1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_16), 0.25,  1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_8),  0.5,   1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_4),  1.0,   1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_2),  2.0,   1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_1),  4.0,   1e-9);
        }

        beginTest("HostDivision dotted is 1.5x straight");
        {
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_8d),
                                      divisionToQuarterNotes(HostDivision::Div1_8) * 1.5, 1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_4d),
                                      divisionToQuarterNotes(HostDivision::Div1_4) * 1.5, 1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_2d),
                                      divisionToQuarterNotes(HostDivision::Div1_2) * 1.5, 1e-9);
        }

        beginTest("HostDivision triplet is 2/3x straight");
        {
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_16t),
                                      divisionToQuarterNotes(HostDivision::Div1_16) * 2.0/3.0, 1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_8t),
                                      divisionToQuarterNotes(HostDivision::Div1_8) * 2.0/3.0, 1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_4t),
                                      divisionToQuarterNotes(HostDivision::Div1_4) * 2.0/3.0, 1e-9);
            expectWithinAbsoluteError(divisionToQuarterNotes(HostDivision::Div1_2t),
                                      divisionToQuarterNotes(HostDivision::Div1_2) * 2.0/3.0, 1e-9);
        }

        beginTest("default state");
        {
            AnimationEngine engine;
            expect(engine.getAnimatedMode());
            expect(!engine.getHostClock());
            expectEquals(engine.getSelectedKeyframe(), 0);
            expectEquals(engine.getActiveNoteCount(), 0);
            expect(!engine.isPlaying());
            for (int i = 0; i < AnimationEngine::kNumTransitions; ++i)
            {
                expectWithinAbsoluteError(engine.getInternalDuration(i), 1000.0f, 0.001f);
                expect(engine.getHostDuration(i) == HostDivision::Div1_4);
            }
            for (int f = 0; f < AnimationEngine::kNumKeyframes; ++f)
                for (int p = 0; p < AnimationEngine::kNumParams; ++p)
                    expectWithinAbsoluteError(engine.getKeyframeValue(f, p), 0.0f, 0.0001f);
        }

        beginTest("keyframe snapshot round-trip and isolation");
        {
            AnimationEngine engine;
            engine.setKeyframeValue(1, AnimationEngine::kIdxVolume, 0.7f);
            expectWithinAbsoluteError(engine.getKeyframeValue(1, AnimationEngine::kIdxVolume), 0.7f, 0.0001f);
            for (int f : { 0, 2, 3 })
                expectWithinAbsoluteError(engine.getKeyframeValue(f, AnimationEngine::kIdxVolume), 0.0f, 0.0001f);
        }

        beginTest("writeKeyframe copies all values");
        {
            AnimationEngine engine;
            AnimationEngine::AnimatedParams a{};
            a[0] = 0.1f; a[5] = 0.5f; a[55] = 0.9f;
            engine.writeKeyframe(2, a);
            AnimationEngine::AnimatedParams out{};
            engine.getKeyframeValues(2, out);
            expectWithinAbsoluteError(out[0],  0.1f, 0.0001f);
            expectWithinAbsoluteError(out[5],  0.5f, 0.0001f);
            expectWithinAbsoluteError(out[55], 0.9f, 0.0001f);
        }

        beginTest("durations stored independently");
        {
            AnimationEngine engine;
            engine.setInternalDuration(0, 250.0f);
            engine.setInternalDuration(1, 2000.0f);
            engine.setInternalDuration(2, 500.0f);
            expectWithinAbsoluteError(engine.getInternalDuration(0), 250.0f,  0.001f);
            expectWithinAbsoluteError(engine.getInternalDuration(1), 2000.0f, 0.001f);
            expectWithinAbsoluteError(engine.getInternalDuration(2), 500.0f,  0.001f);

            engine.setHostDuration(0, HostDivision::Div1_8);
            engine.setHostDuration(1, HostDivision::Div1_2);
            engine.setHostDuration(2, HostDivision::Div1_4t);
            expect(engine.getHostDuration(0) == HostDivision::Div1_8);
            expect(engine.getHostDuration(1) == HostDivision::Div1_2);
            expect(engine.getHostDuration(2) == HostDivision::Div1_4t);

            expectWithinAbsoluteError(engine.getInternalDuration(0), 250.0f, 0.001f);
            expect(engine.getHostDuration(0) == HostDivision::Div1_8);
        }

        beginTest("setAnimatedMode(false) forces selection to 0");
        {
            AnimationEngine engine;
            engine.selectKeyframe(2);
            expectEquals(engine.getSelectedKeyframe(), 2);
            engine.setAnimatedMode(false);
            expectEquals(engine.getSelectedKeyframe(), 0);
        }

        beginTest("setAnimatedMode(true) keeps selection");
        {
            AnimationEngine engine;
            engine.selectKeyframe(2);
            engine.setAnimatedMode(true);
            expectEquals(engine.getSelectedKeyframe(), 2);
        }

        beginTest("note tracking ignores duplicates");
        {
            AnimationEngine engine;
            engine.noteOn(60);
            expectEquals(engine.getActiveNoteCount(), 1);
            expect(engine.isPlaying());
            engine.noteOn(60);
            expectEquals(engine.getActiveNoteCount(), 1);
            engine.noteOn(64);
            expectEquals(engine.getActiveNoteCount(), 2);
            engine.noteOff(60);
            expectEquals(engine.getActiveNoteCount(), 1);
            engine.noteOff(60);
            expectEquals(engine.getActiveNoteCount(), 1);
            engine.noteOff(64);
            expectEquals(engine.getActiveNoteCount(), 0);
            expect(!engine.isPlaying());
        }

        beginTest("allNotesOff clears count");
        {
            AnimationEngine engine;
            engine.noteOn(60); engine.noteOn(64); engine.noteOn(67);
            engine.allNotesOff();
            expectEquals(engine.getActiveNoteCount(), 0);
            expect(!engine.isPlaying());
        }

        beginTest("first noteOn resets phase, subsequent does not");
        {
            AnimationEngine engine;
            AnimationEngine::AnimatedParams out{};
            engine.noteOn(60);
            engine.processBlock(22050, 44100.0, 0.0, 0.0, false, out);
            const double progressAfterFirst = engine.getTransitionProgress();
            expect(progressAfterFirst > 0.4 && progressAfterFirst < 0.6);

            engine.noteOn(64);
            expectWithinAbsoluteError(engine.getTransitionProgress(), progressAfterFirst, 1e-9);

            engine.noteOff(60); engine.noteOff(64);
            expect(!engine.isPlaying());

            engine.noteOn(67);
            expectWithinAbsoluteError(engine.getTransitionProgress(), 0.0, 1e-9);
            expectEquals(engine.getCurrentTransition(), 0);
        }

        beginTest("processBlock STATIC returns false (engine does not drive)");
        {
            AnimationEngine engine;
            engine.setAnimatedMode(false);
            engine.setKeyframeValue(0, AnimationEngine::kIdxVolume, 0.4f);
            engine.noteOn(60);

            AnimationEngine::AnimatedParams out{};
            expect(!engine.processBlock(512, 44100.0, 0.0, 0.0, false, out));
            expectEquals(engine.getCurrentTransition(), 0);
            expectWithinAbsoluteError(engine.getTransitionProgress(), 0.0, 1e-9);
        }

        beginTest("processBlock ANIMATED + no notes returns false");
        {
            AnimationEngine engine;
            engine.selectKeyframe(2);

            AnimationEngine::AnimatedParams out{};
            expect(!engine.processBlock(512, 44100.0, 0.0, 0.0, false, out));
            expectWithinAbsoluteError(engine.getTransitionProgress(), 0.0, 1e-9);
        }

        beginTest("processBlock INTERNAL clock advances phase");
        {
            AnimationEngine engine;
            engine.setKeyframeValue(0, AnimationEngine::kIdxVolume, 0.0f);
            engine.setKeyframeValue(1, AnimationEngine::kIdxVolume, 1.0f);
            engine.setInternalDuration(0, 1000.0f);
            engine.noteOn(60);

            AnimationEngine::AnimatedParams out{};
            engine.processBlock(22050, 44100.0, 0.0, 0.0, false, out);
            expectWithinAbsoluteError(engine.getTransitionProgress(), 0.5, 0.01);
            expectWithinAbsoluteError(out[AnimationEngine::kIdxVolume], 0.5f, 0.01f);
        }

        beginTest("processBlock cycles transitions A->B->C->D->A");
        {
            AnimationEngine engine;
            for (int t = 0; t < AnimationEngine::kNumTransitions; ++t)
                engine.setInternalDuration(t, 100.0f);
            engine.noteOn(60);

            AnimationEngine::AnimatedParams out{};
            engine.processBlock(static_cast<int>(0.05 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 0);
            expect(engine.getTransitionProgress() > 0.4 && engine.getTransitionProgress() < 0.6);

            engine.processBlock(static_cast<int>(0.06 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 1);

            engine.processBlock(static_cast<int>(0.1 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 2);

            engine.processBlock(static_cast<int>(0.1 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 3);

            engine.processBlock(static_cast<int>(0.1 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 0);
        }

        beginTest("full cycle: every transition produces a midpoint sample");
        {
            AnimationEngine engine;
            engine.setKeyframeValue(0, AnimationEngine::kIdxVolume, 0.1f);
            engine.setKeyframeValue(1, AnimationEngine::kIdxVolume, 0.4f);
            engine.setKeyframeValue(2, AnimationEngine::kIdxVolume, 0.7f);
            engine.setKeyframeValue(3, AnimationEngine::kIdxVolume, 0.9f);
            for (int t = 0; t < AnimationEngine::kNumTransitions; ++t)
                engine.setInternalDuration(t, 100.0f);
            engine.noteOn(60);

            AnimationEngine::AnimatedParams out{};

            // advance into transition 0 at ~0.5
            engine.processBlock(static_cast<int>(0.05 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 0);
            expectWithinAbsoluteError(out[AnimationEngine::kIdxVolume], 0.25f, 0.02f);

            // jump into transition 1 at ~0.5
            engine.processBlock(static_cast<int>(0.1 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 1);
            expectWithinAbsoluteError(out[AnimationEngine::kIdxVolume], 0.55f, 0.02f);

            // transition 2 at ~0.5
            engine.processBlock(static_cast<int>(0.1 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 2);
            expectWithinAbsoluteError(out[AnimationEngine::kIdxVolume], 0.80f, 0.02f);

            // transition 3 (D->A) at ~0.5: should be midpoint of 0.9 and 0.1 = 0.5
            engine.processBlock(static_cast<int>(0.1 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 3);
            expectWithinAbsoluteError(out[AnimationEngine::kIdxVolume], 0.50f, 0.02f);
        }

        beginTest("D->A transition interpolates between D and A keyframes");
        {
            AnimationEngine engine;
            engine.setKeyframeValue(3, AnimationEngine::kIdxVolume, 0.0f);
            engine.setKeyframeValue(0, AnimationEngine::kIdxVolume, 1.0f);
            for (int t = 0; t < AnimationEngine::kNumTransitions; ++t)
                engine.setInternalDuration(t, 100.0f);
            engine.noteOn(60);

            AnimationEngine::AnimatedParams out{};
            engine.processBlock(static_cast<int>(0.3 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 3);

            engine.processBlock(static_cast<int>(0.05 * 44100), 44100.0, 0.0, 0.0, false, out);
            expectEquals(engine.getCurrentTransition(), 3);
            expectWithinAbsoluteError(out[AnimationEngine::kIdxVolume], 0.5f, 0.02f);
        }

        beginTest("processBlock HOST clock advances by PPQ delta");
        {
            AnimationEngine engine;
            engine.setHostClock(true);
            engine.setKeyframeValue(0, AnimationEngine::kIdxVolume, 0.0f);
            engine.setKeyframeValue(1, AnimationEngine::kIdxVolume, 1.0f);
            engine.setHostDuration(0, HostDivision::Div1_4);
            engine.noteOn(60);

            AnimationEngine::AnimatedParams out{};
            engine.processBlock(512, 44100.0, 120.0, 0.0, true, out);
            engine.processBlock(512, 44100.0, 120.0, 0.5, true, out);
            expectWithinAbsoluteError(engine.getTransitionProgress(), 0.5, 0.001);
            expectWithinAbsoluteError(out[AnimationEngine::kIdxVolume], 0.5f, 0.01f);
        }

        beginTest("processBlock HOST clock 1/8 doubles speed of 1/4");
        {
            AnimationEngine engine;
            engine.setHostClock(true);
            engine.setHostDuration(0, HostDivision::Div1_8);
            engine.noteOn(60);

            AnimationEngine::AnimatedParams out{};
            engine.processBlock(512, 44100.0, 120.0, 0.0, true, out);
            engine.processBlock(512, 44100.0, 120.0, 0.5, true, out);
            expectEquals(engine.getCurrentTransition(), 1);
        }

        beginTest("processBlock HOST falls back to INTERNAL when playhead invalid");
        {
            AnimationEngine engine;
            engine.setHostClock(true);
            engine.setInternalDuration(0, 1000.0f);
            engine.noteOn(60);

            AnimationEngine::AnimatedParams out{};
            engine.processBlock(22050, 44100.0, 0.0, 0.0, false, out);
            expectWithinAbsoluteError(engine.getTransitionProgress(), 0.5, 0.01);
        }

        beginTest("interpolation across all params");
        {
            AnimationEngine engine;
            for (int p = 0; p < AnimationEngine::kNumParams; ++p)
            {
                engine.setKeyframeValue(0, p, 0.0f);
                engine.setKeyframeValue(1, p, 1.0f);
            }
            engine.setInternalDuration(0, 1000.0f);
            engine.noteOn(60);

            AnimationEngine::AnimatedParams out{};
            engine.processBlock(22050, 44100.0, 0.0, 0.0, false, out);
            for (int p = 0; p < AnimationEngine::kNumParams; ++p)
            {
                if (AnimationEngine::isSlotOnIndex(p))
                    expectWithinAbsoluteError(out[static_cast<size_t>(p)], 0.0f, 0.0001f);
                else
                    expectWithinAbsoluteError(out[static_cast<size_t>(p)], 0.5f, 0.02f);
            }
        }

        beginTest("isSlotOnIndex identifies correct indices");
        {
            expect(!AnimationEngine::isSlotOnIndex(0));
            expect(!AnimationEngine::isSlotOnIndex(5));
            expect(!AnimationEngine::isSlotOnIndex(6));
            expect(!AnimationEngine::isSlotOnIndex(7));
            expect(!AnimationEngine::isSlotOnIndex(8));
            expect(!AnimationEngine::isSlotOnIndex(9));
            expect(AnimationEngine::isSlotOnIndex(10));
            expect(!AnimationEngine::isSlotOnIndex(11));
            expect(AnimationEngine::isSlotOnIndex(17));
            expect(AnimationEngine::isSlotOnIndex(24));
            expect(AnimationEngine::isSlotOnIndex(35));
            expect(AnimationEngine::isSlotOnIndex(42));
            expect(AnimationEngine::isSlotOnIndex(49));
            expect(!AnimationEngine::isSlotOnIndex(34));
            expect(!AnimationEngine::isSlotOnIndex(36));
        }
    }
};

static AnimationEngineTests animationEngineTests;
