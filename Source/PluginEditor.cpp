#include "PluginEditor.h"

QuirkAudioProcessorEditor::QuirkAudioProcessorEditor(QuirkAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    conjusLAF.loadFonts(BinaryData::InconsolataBold_ttf,
                        BinaryData::InconsolataBold_ttfSize,
                        BinaryData::InconsolataRegular_ttf,
                        BinaryData::InconsolataRegular_ttfSize);
    setLookAndFeel(&conjusLAF);

    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 120, 30);
    ConjusKnobLookAndFeel::setKnobType(gainSlider, KnobType::Drive);
    gainSlider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible(gainSlider);

    volumeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 120, 30);
    ConjusKnobLookAndFeel::setKnobType(volumeSlider, KnobType::Volume);
    volumeSlider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible(volumeSlider);

    gainLabel.setJustificationType(juce::Justification::centredBottom);
    addAndMakeVisible(gainLabel);
    volumeLabel.setJustificationType(juce::Justification::centredBottom);
    addAndMakeVisible(volumeLabel);

    gainLabel.toBack();
    volumeLabel.toBack();

    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "gain", gainSlider);
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "volume", volumeSlider);

    gainSlider.textFromValueFunction = [](double value) -> juce::String {
        return juce::String(value, 1);
    };
    gainSlider.valueFromTextFunction = [](const juce::String& text) -> double {
        return text.getDoubleValue();
    };
    gainSlider.updateText();

    volumeSlider.textFromValueFunction = [](double value) -> juce::String {
        return juce::String(value * 100.0, 1) + " %";
    };
    volumeSlider.valueFromTextFunction = [](const juce::String& text) -> double {
        return text.getDoubleValue() / 100.0;
    };
    volumeSlider.updateText();

    gainSlider.onDoubleClick = [this]() {
        startSnapAnimation(gainSlider, gainAnim);
    };
    volumeSlider.onDoubleClick = [this]() {
        startSnapAnimation(volumeSlider, volumeAnim);
    };

    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.getAPVTS(), "bypass", bypassButton);

    symButton.setClickingTogglesState(true);
    symButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    symButton.setConnectedEdges(0);
    bool asym = !processorRef.isSymmetric();
    symButton.getProperties().set("stateTarget", asym ? 1.0 : 0.0);
    symButton.getProperties().set("stateProgress", asym ? 1.0 : 0.0);
    symButton.onStateChange = [this]() {
        symButton.getProperties().set("stateTarget", symButton.getToggleState() ? 1.0 : 0.0);
    };
    addAndMakeVisible(symButton);
    symAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.getAPVTS(), "asymmetric", symButton);

    logoImage = juce::ImageCache::getFromMemory(
        BinaryData::conjiusavatartransparentbg_png,
        BinaryData::conjiusavatartransparentbg_pngSize);

    int savedW = processorRef.editorWidth.load();
    int savedH = processorRef.editorHeight.load();
    setResizable(true, false);
    setResizeLimits(KnobDesign::minWidth, KnobDesign::minHeight,
                    KnobDesign::maxWidth, KnobDesign::maxHeight);
    resizer = std::make_unique<juce::ResizableCornerComponent>(this, getConstrainer());
    resizer->setLookAndFeel(&conjusLAF);
    addAndMakeVisible(resizer.get());
    getConstrainer()->setFixedAspectRatio(
        static_cast<double>(KnobDesign::defaultWidth) / KnobDesign::defaultHeight);
    setSize(savedW, savedH);

    startTimerHz(60);
}

QuirkAudioProcessorEditor::~QuirkAudioProcessorEditor()
{
    if (resizer) resizer->setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
    stopTimer();
}

void QuirkAudioProcessorEditor::setChromeVisible(bool visible)
{
    showChrome = visible;
    bypassButton.setVisible(visible);
    repaint();
}

void QuirkAudioProcessorEditor::mouseMove(const juce::MouseEvent& e)
{
    auto pos = getLocalPoint(e.eventComponent, e.getPosition());
    bool inside = logoBounds.contains(pos);
    if (inside != logoHoverTarget)
        logoHoverTarget = inside;
}

void QuirkAudioProcessorEditor::mouseExit(const juce::MouseEvent& e)
{
    if (e.eventComponent == this)
        logoHoverTarget = false;
}

void QuirkAudioProcessorEditor::timerCallback()
{
    float target = logoHoverTarget ? 1.0f : 0.0f;
    if (std::abs(target - logoHoverProgress) > 0.002f)
    {
        logoHoverProgress += (target - logoHoverProgress) * 0.18f;
        repaint(logoBounds.expanded(static_cast<int>(logoBounds.getWidth() * 0.2f)));
    }

    auto animateHover = [](juce::Component& c, bool hovered) {
        float current = static_cast<float>(c.getProperties().getWithDefault("hoverProgress", 0.0));
        float dest = hovered ? 1.0f : 0.0f;
        if (std::abs(dest - current) > 0.002f)
        {
            current += (dest - current) * 0.22f;
            c.getProperties().set("hoverProgress", current);
            c.repaint();
        }
    };
    animateHover(gainSlider, gainSlider.isMouseOverOrDragging(true));
    animateHover(volumeSlider, volumeSlider.isMouseOverOrDragging(true));
    animateHover(symButton, symButton.isOver() || symButton.isDown());

    {
        auto& props = symButton.getProperties();
        float stateDest = static_cast<float>(props.getWithDefault("stateTarget", 0.0));
        float current = static_cast<float>(props.getWithDefault("stateProgress", 0.0));
        if (std::abs(stateDest - current) > 0.002f)
        {
            current += (stateDest - current) * 0.20f;
            props.set("stateProgress", current);
            symButton.repaint();
        }
    }

    updateSnapAnimation(gainSlider, gainAnim);
    updateSnapAnimation(volumeSlider, volumeAnim);

    float currentGain   = processorRef.getAPVTS().getRawParameterValue("gain")->load();
    float currentVolume = processorRef.getAPVTS().getRawParameterValue("volume")->load();
    int curVer = processorRef.curveVersion_.load(std::memory_order_relaxed);
    bool curSym = processorRef.isSymmetric();
    if (curVer != lastCurveVersion || curSym != lastSymmetric)
        processorRef.updateDisplayCurves();

    if (std::abs(currentGain - lastGraphGain) > 0.001f
        || std::abs(currentVolume - lastGraphVolume) > 0.001f
        || curVer != lastCurveVersion
        || curSym != lastSymmetric)
    {
        lastGraphGain   = currentGain;
        lastGraphVolume = currentVolume;
        lastCurveVersion = curVer;
        lastSymmetric = curSym;
        repaint(graphBounds);
    }
}

void QuirkAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(KnobDesign::bgColour);

    if (logoImage.isValid() && showChrome)
    {
        float scale = static_cast<float>(getWidth()) / static_cast<float>(KnobDesign::defaultWidth);
        int baseSize = static_cast<int>(37.5f * scale);
        int padLeft = static_cast<int>(6.0f * scale);
        int baseX = padLeft;
        int baseY = getHeight() - baseSize;
        logoBounds = { baseX, baseY, baseSize, baseSize };

        float hoverScale = 1.0f + 0.2f * logoHoverProgress;
        int drawSize = static_cast<int>(baseSize * hoverScale);
        int drawX = baseX + (baseSize - drawSize) / 2;
        int drawY = baseY + (baseSize - drawSize) / 2;

        float brightness = 0.35f + 0.65f * logoHoverProgress;
        g.setOpacity(brightness);
        g.drawImage(logoImage,
                    drawX, drawY, drawSize, drawSize,
                    0, 0, logoImage.getWidth(), logoImage.getHeight());
        g.setOpacity(1.0f);
    }

    float w = static_cast<float>(getWidth());
    float h = static_cast<float>(getHeight());
    float scaleF = w / static_cast<float>(KnobDesign::defaultWidth);

    {
        float gOuterLeft = 267.246f * scaleF;
        float gOuterTop = 54.5f * scaleF;
        float gOuterW = 301.0f * scaleF;
        float gOuterH = 189.0f * scaleF;

        float gPad = 14.0f * scaleF;
        float gLeft = gOuterLeft + gPad;
        float gTop = gOuterTop + gPad;
        float gW = gOuterW - 2.0f * gPad;
        float gH = gOuterH - 2.0f * gPad;
        float gRight = gLeft + gW;
        float gBottom = gTop + gH;
        float gCx = gLeft + gW * 0.5f;
        float gCy = gTop + gH * 0.5f;

        graphBounds = juce::Rectangle<float>(gOuterLeft, gOuterTop, gOuterW, gOuterH).toNearestInt().expanded(2);

        {
            float titleFontSize = 28.0f * scaleF;
            g.setFont(conjusLAF.getBoldFont(titleFontSize));
            g.setColour(KnobDesign::accentColour);
            g.drawText("QUIRK",
                       60.0f * scaleF, 90.0f * scaleF,
                       180.0f * scaleF, 30.0f * scaleF,
                       juce::Justification::centred);

            float subtitleFontSize = 14.0f * scaleF;
            g.setFont(conjusLAF.getBoldFont(subtitleFontSize));
            g.setColour(KnobDesign::accentColour);
            g.drawText("SYNTH",
                       60.0f * scaleF, 120.0f * scaleF,
                       180.0f * scaleF, 16.0f * scaleF,
                       juce::Justification::centred);
        }

        float knobDiam = w * 0.216f;
        float graphStroke = knobDiam * KnobDesign::knobStrokeFrac;

        float axisStroke = 1.0f * scaleF;
        float tickStroke = 1.0f * scaleF;

        g.setColour(KnobDesign::accentColour.darker(0.7f));
        g.drawLine(gLeft, gCy, gRight, gCy, axisStroke);
        g.drawLine(gCx, gTop, gCx, gBottom, axisStroke);

        float tickLen = 5.0f * scaleF;
        for (int i = -4; i <= 4; ++i)
        {
            float val = static_cast<float>(i) * 0.25f;
            float px = gCx + val * gW * 0.5f;
            float py = gCy - val * gH * 0.5f;
            g.drawLine(px, gCy - tickLen, px, gCy + tickLen, tickStroke);
            g.drawLine(gCx - tickLen, py, gCx + tickLen, py, tickStroke);
        }

        gc_.gLeft = gLeft; gc_.gTop = gTop; gc_.gW = gW; gc_.gH = gH;
        gc_.gCx = gCx; gc_.gCy = gCy; gc_.gRight = gRight; gc_.gBottom = gBottom;
        gc_.scaleF = scaleF; gc_.valid = true;

        float gainParam = processorRef.getAPVTS().getRawParameterValue("gain")->load();
        float volume = processorRef.getAPVTS().getRawParameterValue("volume")->load();
        float gain = (gainParam < 1e-6f) ? 1.0f : std::pow(100.0f, gainParam / 100.0f);
        auto& curve = processorRef.getBezierCurve();
        auto& leftCurve = processorRef.getLeftBezierCurve();
        bool symmetric = processorRef.isSymmetric();

        auto yForX = [&](float xVal) -> float {
            float xEff = std::clamp(xVal * gain, -1.0f, 1.0f);
            float sign = (xEff < 0.0f) ? -1.0f : 1.0f;
            auto& c = (!symmetric && xEff < 0.0f) ? leftCurve : curve;
            float result = c.evaluate(std::abs(xEff));
            return sign * result * volume;
        };

        auto buildSegment = [&](float t0, float t1, int steps) -> juce::Path {
            juce::Path p;
            if (steps < 1) steps = 1;
            for (int i = 0; i <= steps; ++i)
            {
                float t = t0 + (t1 - t0) * static_cast<float>(i) / static_cast<float>(steps);
                float xVal = -1.0f + 2.0f * t;
                float px = gLeft + t * gW;
                float py = gCy - yForX(xVal) * gH * 0.5f;
                if (i == 0) p.startNewSubPath(px, py);
                else p.lineTo(px, py);
            }
            return p;
        };

        const int totalN = 300;
        juce::PathStrokeType stroke(graphStroke,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        g.setColour(KnobDesign::accentColour);
        g.strokePath(buildSegment(0.0f, 1.0f, totalN), stroke);

        float labelSize = 13.0f * scaleF;
        g.setFont(conjusLAF.getRegularFont(labelSize));
        g.setColour(KnobDesign::accentColour.darker(0.5f));
        g.drawText("-1",   271.0f * scaleF, 158.0f * scaleF, 18.0f * scaleF, 13.0f * scaleF, juce::Justification::centred);
        g.drawText("1",    546.0f * scaleF, 159.0f * scaleF, 18.0f * scaleF, 13.0f * scaleF, juce::Justification::centred);
        g.drawText("-0.5", 333.0f * scaleF, 157.5f * scaleF, 32.0f * scaleF, 13.0f * scaleF, juce::Justification::centred);
        g.drawText("0.5",  477.0f * scaleF, 159.0f * scaleF, 20.0f * scaleF, 13.0f * scaleF, juce::Justification::centred);
        g.drawText("1",    396.0f * scaleF,  61.0f * scaleF, 18.0f * scaleF, 13.0f * scaleF, juce::Justification::centred);
        g.drawText("-1",   423.0f * scaleF, 222.0f * scaleF, 18.0f * scaleF, 13.0f * scaleF, juce::Justification::centred);
        g.drawText("0.5",  387.0f * scaleF, 101.0f * scaleF, 20.0f * scaleF, 13.0f * scaleF, juce::Justification::centred);
        g.drawText("-0.5", 422.0f * scaleF, 182.0f * scaleF, 32.0f * scaleF, 13.0f * scaleF, juce::Justification::centred);

        float pointR = 5.0f * scaleF;
        float handleR = 3.0f * scaleF;
        float handleStroke = 1.5f * scaleF;
        float invGain = 1.0f / gain;
        auto dimAccent = KnobDesign::accentHoverColour.withAlpha(0.4f);

        auto drawControlPoint = [&](float bx, float by, bool negate, bool dim, bool selected)
        {
            float dx = bx * invGain;
            float dy = by * volume;
            auto px = negate ? bezierToPixel(-dx, -dy) : bezierToPixel(dx, dy);
            auto col = selected ? KnobDesign::accentHoverColour.brighter(0.3f) : KnobDesign::accentHoverColour;
            if (dim) col = dimAccent;
            g.setColour(col);
            g.fillEllipse(px.x - pointR, px.y - pointR, pointR * 2.0f, pointR * 2.0f);
        };

        auto drawHandle = [&](float ptBx, float ptBy, float hDx, float hDy, bool negate, bool dim)
        {
            float hx = ptBx + hDx;
            float hy = ptBy + hDy;
            float ptDispX = ptBx * invGain, ptDispY = ptBy * volume;
            float hDispX = hx * invGain, hDispY = hy * volume;
            juce::Point<float> ptPx, hPx;
            if (negate)
            {
                ptPx = bezierToPixel(-ptDispX, -ptDispY);
                hPx = bezierToPixel(-hDispX, -hDispY);
            }
            else
            {
                ptPx = bezierToPixel(ptDispX, ptDispY);
                hPx = bezierToPixel(hDispX, hDispY);
            }
            auto col = dim ? dimAccent : KnobDesign::accentHoverColour.withAlpha(0.6f);
            g.setColour(col);
            juce::Path linePath;
            linePath.startNewSubPath(ptPx);
            linePath.lineTo(hPx);
            juce::Path dashedPath;
            float dashLens[] = { 4.0f * scaleF, 3.0f * scaleF };
            juce::PathStrokeType(handleStroke).createDashedStroke(dashedPath, linePath, dashLens, 2);
            g.fillPath(dashedPath);
            g.fillEllipse(hPx.x - handleR, hPx.y - handleR, handleR * 2.0f, handleR * 2.0f);
        };

        auto startH = curve.getStartOutHandle();
        drawHandle(0.0f, 0.0f, startH.dx, startH.dy, false, false);

        auto endH = curve.getEndInHandle();
        drawHandle(1.0f, 1.0f, endH.dx, endH.dy, false, false);

        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            auto& pt = curve.getPoint(i);
            bool sel = (dragTarget_.type == BezierHitType::Point
                        && dragTarget_.pointIndex == i && !dragTarget_.leftCurve);
            drawHandle(pt.x, pt.y, pt.in.dx, pt.in.dy, false, false);
            drawHandle(pt.x, pt.y, pt.out.dx, pt.out.dy, false, false);
            drawControlPoint(pt.x, pt.y, false, false, sel);
        }

        if (symmetric)
        {
            drawHandle(0.0f, 0.0f, startH.dx, startH.dy, true, true);
            drawHandle(1.0f, 1.0f, endH.dx, endH.dy, true, true);
            for (int i = 0; i < curve.getNumPoints(); ++i)
            {
                auto& pt = curve.getPoint(i);
                drawHandle(pt.x, pt.y, pt.in.dx, pt.in.dy, true, true);
                drawHandle(pt.x, pt.y, pt.out.dx, pt.out.dy, true, true);
                drawControlPoint(pt.x, pt.y, true, true, false);
            }
        }
        else
        {
            auto lStartH = leftCurve.getStartOutHandle();
            drawHandle(0.0f, 0.0f, lStartH.dx, lStartH.dy, true, false);
            auto lEndH = leftCurve.getEndInHandle();
            drawHandle(1.0f, 1.0f, lEndH.dx, lEndH.dy, true, false);
            for (int i = 0; i < leftCurve.getNumPoints(); ++i)
            {
                auto& pt = leftCurve.getPoint(i);
                bool sel = (dragTarget_.type == BezierHitType::Point
                            && dragTarget_.pointIndex == i && dragTarget_.leftCurve);
                drawHandle(pt.x, pt.y, pt.in.dx, pt.in.dy, true, false);
                drawHandle(pt.x, pt.y, pt.out.dx, pt.out.dy, true, false);
                drawControlPoint(pt.x, pt.y, true, false, sel);
            }
        }
    }

}

void QuirkAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    const float scaleF  = static_cast<float>(getWidth())
                        / static_cast<float>(KnobDesign::defaultWidth);
    const float borderW = 4.0f  * scaleF;
    const float radius  = 70.0f * scaleF;
    juce::Rectangle<float> borderRect{ 24.75f * scaleF, 27.0f * scaleF,
                                       590.0f * scaleF,
                                       510.0f * scaleF };
    juce::Path border;
    border.addRoundedRectangle(borderRect, radius);
    g.setColour(KnobDesign::accentColour);
    g.strokePath(border, juce::PathStrokeType(borderW));
}

void QuirkAudioProcessorEditor::resized()
{
    processorRef.editorWidth.store(getWidth());
    processorRef.editorHeight.store(getHeight());

    if (resizer != nullptr)
    {
        const int handleSize = 28;
        resizer->setBounds(getWidth() - handleSize, getHeight() - handleSize,
                           handleSize, handleSize);
        resizer->toFront(false);
        resizer->repaint();
    }

    float w = static_cast<float>(getWidth());
    float h = static_cast<float>(getHeight());

    {
        const float scaleF  = w / static_cast<float>(KnobDesign::defaultWidth);
        const float edgeGap = 10.0f * scaleF;
        const float btnSize = 34.0f * scaleF;
        const float btnX = static_cast<float>(getWidth()) - edgeGap - btnSize;
        const float btnY = edgeGap;
        bypassButton.setBounds(static_cast<int>(btnX),
                               static_cast<int>(btnY),
                               static_cast<int>(btnSize),
                               static_cast<int>(btnSize));
        bypassButton.toFront(false);
    }

    float knobColW = w * 0.28f;
    float halfCol = knobColW * 0.5f;
    float knobColX0 = w * (200.0f / 650.0f) - halfCol;
    float knobColX1 = w * (450.0f / 650.0f) - halfCol;

    float sF = w / static_cast<float>(KnobDesign::defaultWidth);
    gainLabel.setFont(conjusLAF.getBoldFont(20.0f * sF));
    volumeLabel.setFont(conjusLAF.getBoldFont(21.5f * sF));

    gainLabel.setBounds(static_cast<int>(175.0f * sF), static_cast<int>(259.35f * sF),
                        static_cast<int>(50.0f * sF), static_cast<int>(17.0f * sF));
    volumeLabel.setBounds(static_cast<int>(413.0f * sF), static_cast<int>(259.35f * sF),
                          static_cast<int>(72.0f * sF), static_cast<int>(17.0f * sF));

    float dbFontSize = w * KnobDesign::dbTextScale;
    int sliderBottom = static_cast<int>(h * 0.918f);
    int sliderTop = static_cast<int>(h * 0.04f);
    int sliderH = sliderBottom - sliderTop;

    const float knobClusterExtraShift = KnobDesign::knobClusterShiftDefault * (h / static_cast<float>(KnobDesign::defaultHeight));
    const int sliderTopEditor = sliderTop + static_cast<int>(knobClusterExtraShift);

    float sliderBoundsW = knobColW * 0.90f;
    float sliderOffset0 = knobColX0 + (knobColW - sliderBoundsW) * 0.5f;
    float sliderOffset1 = knobColX1 + (knobColW - sliderBoundsW) * 0.5f;

    int textBoxW = static_cast<int>(sliderBoundsW * 0.95f);
    int textBoxH = static_cast<int>(dbFontSize * 2.6f);

    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
    gainSlider.setMouseDragSensitivity(static_cast<int>(w * 0.5f));
    gainSlider.setBounds(static_cast<int>(sliderOffset0), sliderTopEditor,
                         static_cast<int>(sliderBoundsW), sliderH);

    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
    volumeSlider.setMouseDragSensitivity(static_cast<int>(w * 0.5f));
    volumeSlider.setBounds(static_cast<int>(sliderOffset1), sliderTopEditor,
                           static_cast<int>(sliderBoundsW), sliderH);

    for (auto* slider : { &gainSlider, &volumeSlider })
    {
        slider->setPaintingIsUnclipped(true);
        if (auto* textBox = slider->getChildComponent(0))
        {
            if (auto* label = dynamic_cast<juce::Label*>(textBox))
            {
                label->setFont(conjusLAF.getRegularFont(dbFontSize));
                label->setMinimumHorizontalScale(1.0f);
                label->setColour(juce::Label::textColourId, KnobDesign::accentColour);
                label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
                label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
                label->setInterceptsMouseClicks(false, false);
                label->setPaintingIsUnclipped(true);
            }
        }
    }

    float knobAreaH = static_cast<float>(sliderH) - static_cast<float>(textBoxH);
    float knobDiameter = juce::jmin(sliderBoundsW, knobAreaH) * 0.78f;
    float knobStrokeW = knobDiameter * KnobDesign::knobStrokeFrac;
    bypassButton.setRingStrokeWidth(knobStrokeW);

    {
        float btnFontSize = 15.0f * sF;
        float knobDiam = w * 0.216f;
        float knobStrokeW = knobDiam * KnobDesign::knobStrokeFrac;
        float symBtnW = 95.0f * sF;
        float symBtnH = 26.0f * sF;
        float symBtnX = 82.86f * sF;
        float symBtnY = 195.0f * sF;
        symButton.setBounds(static_cast<int>(symBtnX), static_cast<int>(symBtnY),
                            static_cast<int>(symBtnW), static_cast<int>(symBtnH));
        symButton.setConnectedEdges(0);

        struct PillButtonLAF : juce::LookAndFeel_V4
        {
            juce::Font font;
            float knobStrokeW;
            PillButtonLAF(juce::Font f, float ksw) : font(f), knobStrokeW(ksw) {}

            static juce::Rectangle<float> pressBounds(juce::Button& button, bool isButtonDown)
            {
                auto b = button.getLocalBounds().toFloat();
                if (isButtonDown)
                    b = b.reduced(b.getWidth() * 0.04f, b.getHeight() * 0.10f);
                return b;
            }

            void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                      const juce::Colour&, bool, bool isButtonDown) override
            {
                auto bounds = pressBounds(button, isButtonDown);
                float cornerR = bounds.getHeight() * 0.5f;
                float hoverProgress = static_cast<float>(
                    button.getProperties().getWithDefault("hoverProgress", 0.0));
                float stateProgress = static_cast<float>(
                    button.getProperties().getWithDefault("stateProgress", 0.0));
                auto interactiveAccent = KnobDesign::accentColour
                    .interpolatedWith(KnobDesign::accentHoverColour, hoverProgress);
                auto fill = interactiveAccent.interpolatedWith(KnobDesign::bgColour, stateProgress);
                g.setColour(fill);
                g.fillRoundedRectangle(bounds, cornerR);
                float borderW = knobStrokeW * 0.70f * stateProgress;
                if (borderW > 0.001f)
                {
                    g.setColour(interactiveAccent);
                    g.drawRoundedRectangle(bounds.reduced(borderW * 0.5f), cornerR, borderW);
                }
            }

            void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                bool, bool isButtonDown) override
            {
                auto bounds = pressBounds(button, isButtonDown);
                float hoverProgress = static_cast<float>(
                    button.getProperties().getWithDefault("hoverProgress", 0.0));
                float stateProgress = static_cast<float>(
                    button.getProperties().getWithDefault("stateProgress", 0.0));
                auto interactiveAccent = KnobDesign::accentColour
                    .interpolatedWith(KnobDesign::accentHoverColour, hoverProgress);
                auto textColour = KnobDesign::bgColour.interpolatedWith(interactiveAccent, stateProgress);
                float alphaSym  = juce::jmax(0.0f, 1.0f - 2.0f * stateProgress);
                float alphaAsym = juce::jmax(0.0f, 2.0f * stateProgress - 1.0f);
                g.setFont(font);
                if (alphaSym > 0.001f)
                {
                    g.setColour(textColour.withMultipliedAlpha(alphaSym));
                    g.drawText("Symmetric", bounds, juce::Justification::centred, false);
                }
                if (alphaAsym > 0.001f)
                {
                    g.setColour(textColour.withMultipliedAlpha(alphaAsym));
                    g.drawText("Asymmetric", bounds, juce::Justification::centred, false);
                }
            }
        };
        static PillButtonLAF* pillLAF = nullptr;
        if (pillLAF) delete pillLAF;
        pillLAF = new PillButtonLAF(conjusLAF.getBoldFont(btnFontSize), knobStrokeW);
        symButton.setLookAndFeel(pillLAF);
    }
}

void QuirkAudioProcessorEditor::startSnapAnimation(juce::Slider& slider, SliderAnimation& anim)
{
    const char* paramId = (&slider == &gainSlider) ? "gain" : "volume";
    auto* param = processorRef.getAPVTS().getParameter(paramId);
    if (param == nullptr) return;

    anim.currentValue = slider.getValue();
    anim.targetValue = static_cast<double>(param->convertFrom0to1(param->getDefaultValue()));
    anim.active = true;
}

void QuirkAudioProcessorEditor::updateSnapAnimation(juce::Slider& slider, SliderAnimation& anim)
{
    if (!anim.active) return;

    constexpr double smoothing = 0.15;
    anim.currentValue += (anim.targetValue - anim.currentValue) * smoothing;

    if (std::abs(anim.targetValue - anim.currentValue) < 0.01)
    {
        anim.currentValue = anim.targetValue;
        anim.active = false;
    }

    slider.setValue(anim.currentValue, juce::sendNotificationAsync);
}

juce::Point<float> QuirkAudioProcessorEditor::bezierToPixel(float bx, float by) const
{
    if (!gc_.valid) return {0, 0};
    float t = (1.0f + bx) * 0.5f;
    float px = gc_.gLeft + t * gc_.gW;
    float py = gc_.gCy - by * gc_.gH * 0.5f;
    return {px, py};
}

juce::Point<float> QuirkAudioProcessorEditor::pixelToBezier(float px, float py) const
{
    if (!gc_.valid) return {0, 0};
    float t = (px - gc_.gLeft) / gc_.gW;
    float bx = -1.0f + 2.0f * t;
    float by = -(py - gc_.gCy) / (gc_.gH * 0.5f);
    return {bx, by};
}

bool QuirkAudioProcessorEditor::isInGraphArea(float px, float py) const
{
    if (!gc_.valid) return false;
    return px >= gc_.gLeft && px <= gc_.gRight && py >= gc_.gTop && py <= gc_.gBottom;
}

QuirkAudioProcessorEditor::BezierHit
QuirkAudioProcessorEditor::findBezierHit(float mx, float my) const
{
    if (!gc_.valid) return {};

    float gainParam = processorRef.getAPVTS().getRawParameterValue("gain")->load();
    float volume = processorRef.getAPVTS().getRawParameterValue("volume")->load();
    float gain = (gainParam < 1e-6f) ? 1.0f : std::pow(100.0f, gainParam / 100.0f);
    float invGain = 1.0f / gain;
    bool symmetric = processorRef.isSymmetric();
    float hitR = 8.0f * gc_.scaleF;
    float handleHitR = 6.0f * gc_.scaleF;
    float bestDist = hitR;
    BezierHit best;

    auto checkAt = [&](float dispX, float dispY, BezierHitType type, int idx,
                       bool isLeft, float threshold)
    {
        auto px = bezierToPixel(dispX, dispY);
        float d = std::hypot(mx - px.x, my - px.y);
        if (d < threshold && d < bestDist)
        {
            bestDist = d;
            best = {type, idx, isLeft};
        }
    };

    auto checkCurve = [&](const BezierCurve& c, bool negate, bool isLeft)
    {
        float sign = negate ? -1.0f : 1.0f;
        auto sh = c.getStartOutHandle();
        float hx = sh.dx * invGain * sign, hy = sh.dy * volume * sign;
        checkAt(hx, hy, BezierHitType::StartOutHandle, -1, isLeft, handleHitR);

        auto eh = c.getEndInHandle();
        float ehx = (1.0f + eh.dx) * invGain * sign;
        float ehy = (1.0f + eh.dy) * volume * sign;
        checkAt(ehx, ehy, BezierHitType::EndInHandle, -1, isLeft, handleHitR);

        for (int i = 0; i < c.getNumPoints(); ++i)
        {
            auto& pt = c.getPoint(i);
            float pdx = pt.x * invGain * sign, pdy = pt.y * volume * sign;
            checkAt(pdx, pdy, BezierHitType::Point, i, isLeft, hitR);

            float ihx = (pt.x + pt.in.dx) * invGain * sign;
            float ihy = (pt.y + pt.in.dy) * volume * sign;
            checkAt(ihx, ihy, BezierHitType::InHandle, i, isLeft, handleHitR);

            float ohx = (pt.x + pt.out.dx) * invGain * sign;
            float ohy = (pt.y + pt.out.dy) * volume * sign;
            checkAt(ohx, ohy, BezierHitType::OutHandle, i, isLeft, handleHitR);
        }
    };

    auto& curve = processorRef.getBezierCurve();
    checkCurve(curve, false, false);

    if (symmetric)
    {
        checkCurve(curve, true, false);
    }
    else
    {
        auto& leftCurve = processorRef.getLeftBezierCurve();
        checkCurve(leftCurve, true, true);
    }

    return best;
}

float QuirkAudioProcessorEditor::distToNearestCurvePoint(float bx, float by) const
{
    float gainParam = processorRef.getAPVTS().getRawParameterValue("gain")->load();
    float volume = processorRef.getAPVTS().getRawParameterValue("volume")->load();
    float gain = (gainParam < 1e-6f) ? 1.0f : std::pow(100.0f, gainParam / 100.0f);
    bool symmetric = processorRef.isSymmetric();

    float xEff = std::clamp(bx * gain, -1.0f, 1.0f);
    float sign = (xEff < 0.0f) ? -1.0f : 1.0f;
    auto& c = (!symmetric && xEff < 0.0f) ? processorRef.getLeftBezierCurve()
                                           : processorRef.getBezierCurve();
    float curveY = sign * c.evaluate(std::abs(xEff)) * volume;

    auto curvePx = bezierToPixel(bx, curveY);
    auto pointPx = bezierToPixel(bx, by);
    return std::hypot(curvePx.x - pointPx.x, curvePx.y - pointPx.y);
}

void QuirkAudioProcessorEditor::beginGestures(const std::vector<juce::RangedAudioParameter*>& params)
{
    gestureParams_ = params;
    for (auto* p : gestureParams_)
        if (p) p->beginChangeGesture();
}

void QuirkAudioProcessorEditor::endGestures()
{
    for (auto* p : gestureParams_)
        if (p) p->endChangeGesture();
    gestureParams_.clear();
}

void QuirkAudioProcessorEditor::setParam(const juce::String& id, float value)
{
    if (auto* p = processorRef.getAPVTS().getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

void QuirkAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    float mx = static_cast<float>(e.getPosition().x);
    float my = static_cast<float>(e.getPosition().y);

    if (!isInGraphArea(mx, my))
        return;

    auto hit = findBezierHit(mx, my);
    if (hit.type == BezierHitType::None)
        return;

    dragTarget_ = hit;
    dragging_ = true;
    dragPrefix_ = curvePrefix(hit.leftCurve);
    dragSlot_ = -1;

    auto& apvts = processorRef.getAPVTS();
    std::vector<juce::RangedAudioParameter*> params;

    if (hit.type == BezierHitType::Point || hit.type == BezierHitType::InHandle
        || hit.type == BezierHitType::OutHandle)
    {
        auto vals = processorRef.readSlotValues(dragPrefix_);
        dragSlot_ = vals.curvePointToSlot(hit.pointIndex);
        if (dragSlot_ < 0) { dragging_ = false; return; }
        auto s = juce::String(dragSlot_);

        if (hit.type == BezierHitType::Point)
        {
            params.push_back(apvts.getParameter(dragPrefix_ + "p" + s + "_x"));
            params.push_back(apvts.getParameter(dragPrefix_ + "p" + s + "_y"));
        }
        else if (hit.type == BezierHitType::InHandle)
        {
            params.push_back(apvts.getParameter(dragPrefix_ + "p" + s + "_idx"));
            params.push_back(apvts.getParameter(dragPrefix_ + "p" + s + "_idy"));
        }
        else
        {
            params.push_back(apvts.getParameter(dragPrefix_ + "p" + s + "_odx"));
            params.push_back(apvts.getParameter(dragPrefix_ + "p" + s + "_ody"));
        }
    }
    else if (hit.type == BezierHitType::StartOutHandle)
    {
        params.push_back(apvts.getParameter(dragPrefix_ + "sh_dx"));
        params.push_back(apvts.getParameter(dragPrefix_ + "sh_dy"));
    }
    else if (hit.type == BezierHitType::EndInHandle)
    {
        params.push_back(apvts.getParameter(dragPrefix_ + "eh_dx"));
        params.push_back(apvts.getParameter(dragPrefix_ + "eh_dy"));
    }

    beginGestures(params);
}

void QuirkAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (!dragging_) return;

    float mx = static_cast<float>(e.getPosition().x);
    float my = static_cast<float>(e.getPosition().y);
    auto bz = pixelToBezier(mx, my);
    float gainParam = processorRef.getAPVTS().getRawParameterValue("gain")->load();
    float volume = processorRef.getAPVTS().getRawParameterValue("volume")->load();
    float gain = (gainParam < 1e-6f) ? 1.0f : std::pow(100.0f, gainParam / 100.0f);
    float rawY = (std::abs(volume) > 1e-6f) ? bz.y / volume : bz.y;
    float absBx = std::abs(bz.x) * gain;
    float absby = (bz.x < 0.0f) ? -rawY : rawY;

    auto s = juce::String(dragSlot_);

    switch (dragTarget_.type)
    {
        case BezierHitType::Point:
            setParam(dragPrefix_ + "p" + s + "_x", absBx);
            setParam(dragPrefix_ + "p" + s + "_y", absby);
            break;
        case BezierHitType::InHandle:
        {
            auto& curve = dragTarget_.leftCurve ? processorRef.getLeftBezierCurve()
                                                : processorRef.getBezierCurve();
            auto& pt = curve.getPoint(dragTarget_.pointIndex);
            setParam(dragPrefix_ + "p" + s + "_idx", absBx - pt.x);
            setParam(dragPrefix_ + "p" + s + "_idy", absby - pt.y);
            break;
        }
        case BezierHitType::OutHandle:
        {
            auto& curve = dragTarget_.leftCurve ? processorRef.getLeftBezierCurve()
                                                : processorRef.getBezierCurve();
            auto& pt = curve.getPoint(dragTarget_.pointIndex);
            setParam(dragPrefix_ + "p" + s + "_odx", absBx - pt.x);
            setParam(dragPrefix_ + "p" + s + "_ody", absby - pt.y);
            break;
        }
        case BezierHitType::StartOutHandle:
            setParam(dragPrefix_ + "sh_dx", absBx);
            setParam(dragPrefix_ + "sh_dy", absby);
            break;
        case BezierHitType::EndInHandle:
            setParam(dragPrefix_ + "eh_dx", absBx - 1.0f);
            setParam(dragPrefix_ + "eh_dy", absby - 1.0f);
            break;
        default:
            break;
    }

    processorRef.updateDisplayCurves();
    repaint(graphBounds);
}

void QuirkAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    if (dragging_)
        endGestures();
    dragging_ = false;
    dragTarget_ = {};
    dragSlot_ = -1;
}

void QuirkAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    float mx = static_cast<float>(e.getPosition().x);
    float my = static_cast<float>(e.getPosition().y);

    if (!isInGraphArea(mx, my))
        return;

    auto hit = findBezierHit(mx, my);
    if (hit.type == BezierHitType::Point)
    {
        auto prefix = curvePrefix(hit.leftCurve);
        auto vals = processorRef.readSlotValues(prefix);
        int slot = vals.curvePointToSlot(hit.pointIndex);
        if (slot >= 0)
        {
            setParam(prefix + "p" + juce::String(slot) + "_on", 0.0f);
            processorRef.updateDisplayCurves();
            repaint(graphBounds);
        }
        return;
    }

    auto bz = pixelToBezier(mx, my);
    float gainParam = processorRef.getAPVTS().getRawParameterValue("gain")->load();
    float volume = processorRef.getAPVTS().getRawParameterValue("volume")->load();
    float gain = (gainParam < 1e-6f) ? 1.0f : std::pow(100.0f, gainParam / 100.0f);
    bool symmetric = processorRef.isSymmetric();
    float rawY = (std::abs(volume) > 1e-6f) ? bz.y / volume : bz.y;
    float absBx = std::clamp(std::abs(bz.x) * gain, 0.0f, 1.0f);
    float absby = (bz.x < 0.0f) ? -rawY : rawY;

    float curveHitDist = distToNearestCurvePoint(bz.x, bz.y);
    if (curveHitDist > 12.0f * gc_.scaleF)
        return;

    bool useLeft = !symmetric && bz.x < 0.0f;
    auto prefix = curvePrefix(useLeft);
    int slot = processorRef.findFreeSlot(prefix);
    if (slot < 0) return;

    auto s = juce::String(slot);
    float defDx = std::min(absBx, 1.0f - absBx) / 3.0f;
    setParam(prefix + "p" + s + "_on", 1.0f);
    setParam(prefix + "p" + s + "_x", absBx);
    setParam(prefix + "p" + s + "_y", absby);
    setParam(prefix + "p" + s + "_idx", -defDx);
    setParam(prefix + "p" + s + "_idy", 0.0f);
    setParam(prefix + "p" + s + "_odx", defDx);
    setParam(prefix + "p" + s + "_ody", 0.0f);
    processorRef.updateDisplayCurves();
    repaint(graphBounds);
}
