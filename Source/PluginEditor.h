#pragma once

#include "PluginProcessor.h"
#include "KnobDesign.h"
#include "LayoutSizes.h"
#include "BypassButton.h"
#include "BinaryData.h"

class VelocitySlider : public juce::Slider
{
public:
    std::function<void()> onDoubleClick;

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (onDoubleClick)
            onDoubleClick();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        auto reversed = wheel;
        reversed.deltaY = -wheel.deltaY;
        juce::Slider::mouseWheelMove(e, reversed);
    }

    void paint(juce::Graphics&) override {}
};

class AnimatedSlider : public juce::Slider
{
public:
    std::function<void()> onDoubleClick;

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (onDoubleClick)
            onDoubleClick();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        auto reversed = wheel;
        reversed.deltaY = -wheel.deltaY;
        juce::Slider::mouseWheelMove(e, reversed);
    }

    bool hitTest(int x, int y) override
    {
        if (!isRotary())
            return juce::Component::hitTest(x, y);

        float sw = static_cast<float>(getWidth());
        float sh = static_cast<float>(getHeight());

        float parentH = sh;
        if (auto* editor = getParentComponent())
            parentH = static_cast<float>(editor->getHeight());
        const float knobShiftBase = 80.7f;
        const float knobShift = knobShiftBase * (parentH / static_cast<float>(KnobDesign::defaultHeight));

        float d = juce::jmin(juce::jmin(sw, sh) * 0.78f, sw * 0.60f);
        float r = d * 0.5f;
        float cx = sw * 0.5f;
        float cy = parentH * 0.5f - static_cast<float>(getY()) + knobShift;

        float tickEndR = r * (KnobDesign::tickGap + KnobDesign::tickLength);
        float dx = static_cast<float>(x) - cx;
        float dy = static_cast<float>(y) - cy;
        return dx * dx + dy * dy <= tickEndR * tickEndR;
    }
};

class QuirkAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit QuirkAudioProcessorEditor(QuirkAudioProcessor&);
    ~QuirkAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void setChromeVisible(bool visible);

private:
    bool showChrome = true;
    void timerCallback() override;

    QuirkAudioProcessor& processorRef;
    ConjusKnobLookAndFeel conjusLAF;

    AnimatedSlider volumeSlider;
    juce::Label volumeLabel { {}, "VOLUME" };

    AnimatedSlider attackSlider, decaySlider, sustainSlider, releaseSlider;

    VelocitySlider velocitySlider;

    BypassButton bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velocityAttachment;

    juce::Image logoImage;
    juce::Image quirkLogoImage;

    std::unique_ptr<juce::ResizableCornerComponent> resizer;

    juce::Rectangle<int> logoBounds;
    juce::Rectangle<int> graphBounds;
    bool  logoHoverTarget   = false;
    float logoHoverProgress = 0.0f;

    struct SliderAnimation
    {
        bool active = false;
        double targetValue = 0.0;
        double currentValue = 0.0;
    };
    SliderAnimation volumeAnim;
    SliderAnimation velocityAnim;

    void startSnapAnimation(juce::Slider& slider, SliderAnimation& anim, const juce::String& paramId);
    void updateSnapAnimation(juce::Slider& slider, SliderAnimation& anim);
    void drawKeyframeRow(juce::Graphics& g, float scaleF);

    int lastCurveVersion = -1;

    bool voiceMinusHoverTarget_ = false;
    bool voicePlusHoverTarget_ = false;
    float voiceMinusHoverProgress_ = 0.0f;
    float voicePlusHoverProgress_ = 0.0f;
    float voiceMinusClickProgress_ = 0.0f;
    float voicePlusClickProgress_ = 0.0f;

    int faderPillHover_ = -1;
    bool volumePillHover_ = false;
    bool velocityPillHover_ = false;

    int keyframeHover_ = -1;
    float keyframeHoverProgress_[4] = {};
    float keyframeClickProgress_[4] = {};

    bool modeButtonHover_ = false;
    float modeButtonHoverProgress_ = 0.0f;
    float modeButtonClickProgress_ = 0.0f;

    bool clockSourceButtonHover_ = false;
    float clockSourceButtonHoverProgress_ = 0.0f;
    float clockSourceButtonClickProgress_ = 0.0f;

    int durationPillMinusHover_ = -1;
    int durationPillPlusHover_  = -1;
    float durationPillMinusHoverProgress_[4] = {};
    float durationPillPlusHoverProgress_[4]  = {};
    float durationPillMinusClickProgress_[4] = {};
    float durationPillPlusClickProgress_[4]  = {};

    int durationDragTransition_ = -1;
    float durationDragStartY_ = 0.0f;
    float durationDragStartMs_ = 0.0f;
    int   durationDragStartHostDiv_ = 0;

    bool lastAnimatedMode_ = true;
    bool lastHostClock_ = false;
    int lastSelectedKeyframe_ = 0;

    juce::Slider* pillDragSlider_ = nullptr;
    float pillDragStartY_ = 0.0f;
    double pillDragStartValue_ = 0.0;
    float pillDragSensitivity_ = 200.0f;

    struct GraphCoords
    {
        float gLeft, gTop, gW, gH, gCx, gCy, gRight, gBottom;
        float scaleF;
        bool valid = false;
    };
    GraphCoords gc_;

    enum class BezierHitType { None, Point, InHandle, OutHandle, StartOutHandle, EndInHandle };
    struct BezierHit
    {
        BezierHitType type = BezierHitType::None;
        int pointIndex = -1;
        bool leftCurve = false;
    };

    BezierHit dragTarget_;
    bool dragging_ = false;
    juce::String dragPrefix_;
    int dragSlot_ = -1;
    std::vector<juce::RangedAudioParameter*> gestureParams_;

    void beginGestures(const std::vector<juce::RangedAudioParameter*>& params);
    void endGestures();
    void setParam(const juce::String& id, float value);
    juce::String curvePrefix(bool leftCurve) const { return leftCurve ? "lc_" : "rc_"; }

    BezierHit findBezierHit(float mx, float my) const;
    juce::Point<float> bezierToPixel(float bx, float by) const;
    juce::Point<float> pixelToBezier(float px, float py) const;
    bool isInGraphArea(float px, float py) const;
    float distToNearestCurvePoint(float bx, float by) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuirkAudioProcessorEditor)
};
