#include "PluginEditor.h"

QuirkAudioProcessorEditor::QuirkAudioProcessorEditor(QuirkAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    conjusLAF.loadFonts(BinaryData::InconsolataBold_ttf,
                        BinaryData::InconsolataBold_ttfSize,
                        BinaryData::InconsolataRegular_ttf,
                        BinaryData::InconsolataRegular_ttfSize);
    setLookAndFeel(&conjusLAF);

    volumeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    volumeSlider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible(volumeSlider);

    volumeLabel.setJustificationType(juce::Justification::centredBottom);
    addAndMakeVisible(volumeLabel);
    volumeLabel.toBack();

    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "volume", volumeSlider);

    volumeSlider.onValueChange = [this]() { repaint(); };
    volumeSlider.onDoubleClick = [this]() {
        startSnapAnimation(volumeSlider, volumeAnim, "volume");
    };

    auto setupFader = [this](juce::Slider& slider, const juce::String& paramId,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment) {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible(slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.getAPVTS(), paramId, slider);
        slider.onValueChange = [this]() { repaint(); };
    };
    setupFader(attackSlider, "attack", attackAttachment);
    setupFader(decaySlider, "decay", decayAttachment);
    setupFader(sustainSlider, "sustain", sustainAttachment);
    setupFader(releaseSlider, "release", releaseAttachment);

    velocitySlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    velocitySlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    velocitySlider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    velocitySlider.setRotaryParameters(juce::degreesToRadians(KnobDesign::rotationStartAngle),
                                        juce::degreesToRadians(KnobDesign::rotationEndAngle),
                                        true);
    addAndMakeVisible(velocitySlider);

    velocityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "velocity", velocitySlider);

    velocitySlider.onValueChange = [this]() { repaint(); };
    velocitySlider.onDoubleClick = [this]() {
        startSnapAnimation(velocitySlider, velocityAnim, "velocity");
    };

    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef.getAPVTS(), "bypass", bypassButton);

    logoImage = juce::ImageCache::getFromMemory(
        BinaryData::conjiusavatartransparentbg_png,
        BinaryData::conjiusavatartransparentbg_pngSize);
    quirkLogoImage = juce::ImageCache::getFromMemory(
        BinaryData::quirklogotransparentbg_png,
        BinaryData::quirklogotransparentbg_pngSize);

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

    float sF = static_cast<float>(getWidth()) / static_cast<float>(KnobDesign::defaultWidth);

    float vpX = 508.0f * sF, vpY = 287.181f * sF;
    float vpW = 60.0f * sF, vpH = 22.0f * sF;
    bool overPill = pos.x >= vpX && pos.x <= vpX + vpW
                 && pos.y >= vpY && pos.y <= vpY + vpH;
    voiceMinusHoverTarget_ = overPill && pos.x < vpX + 20.0f * sF;
    voicePlusHoverTarget_  = overPill && pos.x > vpX + 40.0f * sF;

    float faderPillXs[4] = { 199.75f, 269.75f, 339.75f, 409.75f };
    faderPillHover_ = -1;
    for (int i = 0; i < 4; ++i)
    {
        float px = faderPillXs[i] * sF;
        float py = 418.155f * sF;
        if (pos.x >= px && pos.x <= px + 60.0f * sF
            && pos.y >= py && pos.y <= py + 22.0f * sF)
        {
            faderPillHover_ = i;
            break;
        }
    }

    float volPx = 76.86f * sF, volPy = 416.681f * sF;
    volumePillHover_ = pos.x >= volPx && pos.x <= volPx + 60.28f * sF
                    && pos.y >= volPy && pos.y <= volPy + 21.5f * sF;

    float velPx = 515.778f * sF, velPy = 410.578f * sF;
    velocityPillHover_ = pos.x >= velPx && pos.x <= velPx + 46.0f * sF
                      && pos.y >= velPy && pos.y <= velPy + 13.0f * sF;

    const float kfX[4] = { 166.75f, 266.75f, 366.75f, 466.75f };
    const float kfY = 465.181f, kfSize = 24.0f;
    keyframeHover_ = -1;
    for (int i = 0; i < 4; ++i)
    {
        const float x = kfX[i] * sF;
        const float y = kfY * sF;
        const float s = kfSize * sF;
        if (pos.x >= x && pos.x <= x + s && pos.y >= y && pos.y <= y + s)
        {
            keyframeHover_ = i;
            break;
        }
    }

    const float pillXs[4] = { 206.75f, 306.75f, 406.75f, 306.75f };
    const float pillYs[4] = { 470.181f, 470.181f, 470.181f, 501.181f };
    const float dpW = 44.0f, dpH = 14.0f, sideW = 10.0f;
    durationPillMinusHover_ = -1;
    durationPillPlusHover_ = -1;
    int durationPillBodyHover = -1;
    for (int i = 0; i < 4; ++i)
    {
        const float x = pillXs[i] * sF;
        const float y = pillYs[i] * sF;
        const float w = dpW * sF;
        const float h = dpH * sF;
        if (pos.x >= x && pos.x <= x + w && pos.y >= y && pos.y <= y + h)
        {
            if (pos.x <= x + sideW * sF) durationPillMinusHover_ = i;
            else if (pos.x >= x + w - sideW * sF) durationPillPlusHover_ = i;
            else durationPillBodyHover = i;
            break;
        }
    }

    const float modeBtnX = 72.0f * sF, modeBtnY = 477.181f * sF;
    const float modeBtnW = 70.0f * sF, modeBtnH = 18.0f * sF;
    modeButtonHover_ = pos.x >= modeBtnX && pos.x <= modeBtnX + modeBtnW
                   && pos.y >= modeBtnY && pos.y <= modeBtnY + modeBtnH;

    const float csBtnX = 506.0f * sF, csBtnY = 477.181f * sF;
    const float csBtnW = 70.0f * sF, csBtnH = 18.0f * sF;
    clockSourceButtonHover_ = pos.x >= csBtnX && pos.x <= csBtnX + csBtnW
                          && pos.y >= csBtnY && pos.y <= csBtnY + csBtnH;

    bool overBtn = voiceMinusHoverTarget_ || voicePlusHoverTarget_
                || durationPillMinusHover_ >= 0 || durationPillPlusHover_ >= 0;
    bool overToggle = modeButtonHover_ || clockSourceButtonHover_ || keyframeHover_ >= 0;
    if (overBtn)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else if (overToggle)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else if (faderPillHover_ >= 0 || volumePillHover_ || velocityPillHover_ || durationPillBodyHover >= 0)
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void QuirkAudioProcessorEditor::mouseExit(const juce::MouseEvent& e)
{
    if (e.eventComponent == this)
    {
        logoHoverTarget = false;
        voiceMinusHoverTarget_ = false;
        voicePlusHoverTarget_ = false;
        faderPillHover_ = -1;
        volumePillHover_ = false;
        velocityPillHover_ = false;
        keyframeHover_ = -1;
        modeButtonHover_ = false;
        clockSourceButtonHover_ = false;
        durationPillMinusHover_ = -1;
        durationPillPlusHover_ = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void QuirkAudioProcessorEditor::timerCallback()
{
    float target = logoHoverTarget ? 1.0f : 0.0f;
    if (std::abs(target - logoHoverProgress) > 0.002f)
    {
        logoHoverProgress += (target - logoHoverProgress) * 0.18f;
        repaint(logoBounds.expanded(static_cast<int>(logoBounds.getWidth() * 0.2f)));
    }

    bool hoverDirty = false;
    auto animateHover = [&hoverDirty](juce::Component& c, bool hovered) {
        float current = static_cast<float>(c.getProperties().getWithDefault("hoverProgress", 0.0));
        float dest = hovered ? 1.0f : 0.0f;
        if (std::abs(dest - current) > 0.002f)
        {
            current += (dest - current) * 0.22f;
            c.getProperties().set("hoverProgress", current);
            c.repaint();
            for (int i = 0; i < c.getNumChildComponents(); ++i)
                c.getChildComponent(i)->repaint();
            hoverDirty = true;
        }
    };
    animateHover(volumeSlider, volumeSlider.isMouseOverOrDragging(true) || volumePillHover_);
    animateHover(velocitySlider, velocitySlider.isMouseOverOrDragging(true) || velocityPillHover_);

    juce::Slider* faderSliders[4] = { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider };
    for (int i = 0; i < 4; ++i)
        animateHover(*faderSliders[i], faderSliders[i]->isMouseOverOrDragging(true) || faderPillHover_ == i);

    auto animateFloat = [&hoverDirty](float& current, bool target) {
        float dest = target ? 1.0f : 0.0f;
        if (std::abs(dest - current) > 0.002f)
        {
            current += (dest - current) * 0.22f;
            hoverDirty = true;
        }
    };
    animateFloat(voiceMinusHoverProgress_, voiceMinusHoverTarget_);
    animateFloat(voicePlusHoverProgress_, voicePlusHoverTarget_);
    if (voiceMinusClickProgress_ > 0.002f)
    {
        voiceMinusClickProgress_ *= 0.82f;
        hoverDirty = true;
    }
    if (voicePlusClickProgress_ > 0.002f)
    {
        voicePlusClickProgress_ *= 0.82f;
        hoverDirty = true;
    }

    animateFloat(modeButtonHoverProgress_, modeButtonHover_);
    animateFloat(clockSourceButtonHoverProgress_, clockSourceButtonHover_);
    if (modeButtonClickProgress_ > 0.002f) { modeButtonClickProgress_ *= 0.82f; hoverDirty = true; }
    if (clockSourceButtonClickProgress_ > 0.002f) { clockSourceButtonClickProgress_ *= 0.82f; hoverDirty = true; }

    for (int i = 0; i < 4; ++i)
    {
        animateFloat(keyframeHoverProgress_[i], keyframeHover_ == i);
        if (keyframeClickProgress_[i] > 0.002f) { keyframeClickProgress_[i] *= 0.82f; hoverDirty = true; }
    }
    for (int i = 0; i < 4; ++i)
    {
        animateFloat(durationPillMinusHoverProgress_[i], durationPillMinusHover_ == i);
        animateFloat(durationPillPlusHoverProgress_[i],  durationPillPlusHover_  == i);
        if (durationPillMinusClickProgress_[i] > 0.002f) { durationPillMinusClickProgress_[i] *= 0.82f; hoverDirty = true; }
        if (durationPillPlusClickProgress_[i]  > 0.002f) { durationPillPlusClickProgress_[i]  *= 0.82f; hoverDirty = true; }
    }

    auto& engine = processorRef.getAnimationEngine();
    const bool nowAnimated  = engine.getAnimatedMode();
    const bool nowHostClock = engine.getHostClock();
    const int  nowSelected  = engine.getSelectedKeyframe();
    if (nowAnimated != lastAnimatedMode_ || nowHostClock != lastHostClock_ || nowSelected != lastSelectedKeyframe_)
    {
        lastAnimatedMode_      = nowAnimated;
        lastHostClock_         = nowHostClock;
        lastSelectedKeyframe_  = nowSelected;
        hoverDirty = true;
    }

    const bool userInteracting = volumeSlider.isMouseButtonDown()
                              || attackSlider.isMouseButtonDown()
                              || decaySlider.isMouseButtonDown()
                              || sustainSlider.isMouseButtonDown()
                              || releaseSlider.isMouseButtonDown()
                              || velocitySlider.isMouseButtonDown()
                              || pillDragSlider_ != nullptr
                              || dragging_;
    if (userInteracting && !engine.isPlaying())
        processorRef.captureCurrentToSelectedKeyframe();

    if (hoverDirty) repaint();

    updateSnapAnimation(volumeSlider, volumeAnim);
    updateSnapAnimation(velocitySlider, velocityAnim);

    int curVer = processorRef.curveVersion_.load(std::memory_order_relaxed);
    if (curVer != lastCurveVersion)
    {
        processorRef.updateDisplayCurves();
        lastCurveVersion = curVer;
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
    float scaleF = w / static_cast<float>(KnobDesign::defaultWidth);

    {
        float gOuterLeft = 168.0f * scaleF;
        float gOuterTop = 57.0f * scaleF;
        float gOuterW = 417.0f * scaleF;
        float gOuterH = 191.0f * scaleF;

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

        if (quirkLogoImage.isValid())
        {
            float logoAspect = static_cast<float>(quirkLogoImage.getWidth())
                             / static_cast<float>(quirkLogoImage.getHeight());
            float logoX = 40.7462f * scaleF;
            float logoY = 119.5f * scaleF;
            float logoW = 115.0f * scaleF;
            float logoH = 67.0f * scaleF;
            float drawH = logoW / logoAspect;
            float drawY = logoY + (logoH - drawH) * 0.5f;

            juce::AffineTransform logoTransform =
                juce::AffineTransform::rotation(
                    juce::degreesToRadians(-6.7317f),
                    logoX + logoW * 0.5f,
                    logoY + logoH * 0.5f);

            g.saveState();
            g.addTransform(logoTransform);
            g.setOpacity(1.0f);
            g.drawImage(quirkLogoImage,
                        juce::Rectangle<float>(logoX, drawY, logoW, drawH));
            g.restoreState();

            float stX = 86.2462f * scaleF;
            float stY = 174.5f * scaleF;
            float stW = 88.0f * scaleF;
            float stFontSize = 10.0f * scaleF;
            float stSpacing = 2.0f * scaleF;
            auto stFont = conjusLAF.getBoldFont(stFontSize);
            g.setColour(KnobDesign::accentColour);
            g.setFont(stFont);

            juce::StringArray stLines;
            stLines.add("WAVESCANNING");
            stLines.add("POLYSYNTH");
            float stLineH = stFontSize * 1.2f;
            for (int li = 0; li < stLines.size(); ++li)
            {
                auto& line = stLines[li];
                float totalW = 0;
                for (int ci = 0; ci < line.length(); ++ci)
                    totalW += stFont.getStringWidthFloat(line.substring(ci, ci + 1));
                totalW += stSpacing * static_cast<float>(line.length() - 1);
                float cx = stX;
                float cy = stY + static_cast<float>(li) * stLineH;
                for (int ci = 0; ci < line.length(); ++ci)
                {
                    auto ch = line.substring(ci, ci + 1);
                    float chW = stFont.getStringWidthFloat(ch);
                    g.drawText(ch, juce::Rectangle<float>(cx, cy, chW, stLineH),
                               juce::Justification::centred);
                    cx += chW + stSpacing;
                }
            }
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

        auto& curve = processorRef.getBezierCurve();
        auto& leftCurve = processorRef.getLeftBezierCurve();

        auto yForX = [&](float xVal) -> float {
            float sign = (xVal < 0.0f) ? -1.0f : 1.0f;
            auto& c = (xVal < 0.0f) ? leftCurve : curve;
            float result = c.evaluate(std::abs(xVal));
            return sign * result;
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

        float pointR = 5.0f * scaleF;
        float handleR = 3.0f * scaleF;
        float handleStroke = 1.5f * scaleF;

        auto drawControlPoint = [&](float bx, float by, bool negate, bool selected)
        {
            auto px = negate ? bezierToPixel(-bx, -by) : bezierToPixel(bx, by);
            auto col = selected ? KnobDesign::accentHoverColour.brighter(0.3f) : KnobDesign::accentHoverColour;
            g.setColour(col);
            g.fillEllipse(px.x - pointR, px.y - pointR, pointR * 2.0f, pointR * 2.0f);
        };

        auto drawHandle = [&](float ptBx, float ptBy, float hDx, float hDy, bool negate)
        {
            float hx = ptBx + hDx;
            float hy = ptBy + hDy;
            juce::Point<float> ptPx, hPx;
            if (negate)
            {
                ptPx = bezierToPixel(-ptBx, -ptBy);
                hPx = bezierToPixel(-hx, -hy);
            }
            else
            {
                ptPx = bezierToPixel(ptBx, ptBy);
                hPx = bezierToPixel(hx, hy);
            }
            g.setColour(KnobDesign::accentHoverColour.withAlpha(0.6f));
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
        drawHandle(0.0f, 0.0f, startH.dx, startH.dy, false);
        auto endH = curve.getEndInHandle();
        drawHandle(1.0f, 0.0f, endH.dx, endH.dy, false);
        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            auto& pt = curve.getPoint(i);
            bool sel = (dragTarget_.type == BezierHitType::Point
                        && dragTarget_.pointIndex == i && !dragTarget_.leftCurve);
            drawHandle(pt.x, pt.y, pt.in.dx, pt.in.dy, false);
            drawHandle(pt.x, pt.y, pt.out.dx, pt.out.dy, false);
            drawControlPoint(pt.x, pt.y, false, sel);
        }

        auto lStartH = leftCurve.getStartOutHandle();
        drawHandle(0.0f, 0.0f, lStartH.dx, lStartH.dy, true);
        auto lEndH = leftCurve.getEndInHandle();
        drawHandle(1.0f, 0.0f, lEndH.dx, lEndH.dy, true);
        for (int i = 0; i < leftCurve.getNumPoints(); ++i)
        {
            auto& pt = leftCurve.getPoint(i);
            bool sel = (dragTarget_.type == BezierHitType::Point
                        && dragTarget_.pointIndex == i && dragTarget_.leftCurve);
            drawHandle(pt.x, pt.y, pt.in.dx, pt.in.dy, true);
            drawHandle(pt.x, pt.y, pt.out.dx, pt.out.dy, true);
            drawControlPoint(pt.x, pt.y, true, sel);
        }
    }

    {
        float faderLabelFontSize = 14.0f * scaleF;
        g.setColour(KnobDesign::accentColour);
        auto faderLabelFont = conjusLAF.getBoldFont(faderLabelFontSize)
            .withExtraKerningFactor(1.0f / faderLabelFontSize);
        g.setFont(faderLabelFont);

        struct { const char* label; float x; } faderLabels[4] = {
            { "ATTACK",  189.75f },
            { "DECAY",   259.75f },
            { "SUSTAIN", 329.75f },
            { "RELEASE", 402.75f },
        };

        for (auto& fl : faderLabels)
        {
            g.drawText(fl.label,
                       juce::Rectangle<float>(fl.x * scaleF, 265.0f * scaleF,
                                              80.0f * scaleF, 17.0f * scaleF),
                       juce::Justification::centred);
        }

        juce::Slider* faderSliders[4] = { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider };
        float faderValueXs[4] = { 199.75f, 269.75f, 339.75f, 409.75f };

        for (int i = 0; i < 4; ++i)
        {
            float hp = static_cast<float>(
                faderSliders[i]->getProperties().getWithDefault("hoverProgress", 0.0));
            auto pillColour = KnobDesign::accentColour
                .interpolatedWith(KnobDesign::accentHoverColour, hp);

            juce::Rectangle<float> valueRect(faderValueXs[i] * scaleF, 418.155f * scaleF,
                                              60.0f * scaleF, 22.0f * scaleF);
            g.setColour(pillColour);
            g.fillRoundedRectangle(valueRect, 11.0f * scaleF);

            double val = faderSliders[i]->getValue();
            juce::String pillText;
            if (i == 2)
                pillText = juce::String(val, 1) + " %";
            else if (val >= 1000.0)
                pillText = juce::String(val / 1000.0, 2) + " s";
            else
                pillText = juce::String(static_cast<int>(val)) + " ms";

            g.setColour(KnobDesign::bgColour);
            g.setFont(conjusLAF.getBoldFont(faderLabelFontSize));
            g.drawText(pillText, valueRect, juce::Justification::centred);
        }

        {
            float vhp = static_cast<float>(
                volumeSlider.getProperties().getWithDefault("hoverProgress", 0.0));
            auto volPillColour = KnobDesign::accentColour
                .interpolatedWith(KnobDesign::accentHoverColour, vhp);

            juce::Rectangle<float> volRect(76.86f * scaleF, 416.681f * scaleF,
                                            60.28f * scaleF, 21.5f * scaleF);
            g.setColour(volPillColour);
            g.fillRoundedRectangle(volRect, 10.75f * scaleF);

            juce::String volText = juce::String(volumeSlider.getValue() * 100.0, 1) + " %";
            g.setColour(KnobDesign::bgColour);
            g.setFont(conjusLAF.getBoldFont(faderLabelFontSize));
            g.drawText(volText, volRect, juce::Justification::centred);
        }
    }

    {
        g.setColour(KnobDesign::accentColour);
        g.setFont(conjusLAF.getBoldFont(14.0f * scaleF)
            .withExtraKerningFactor(1.0f / (14.0f * scaleF)));
        g.drawText("VOICES",
                   juce::Rectangle<float>(509.0f * scaleF, 266.181f * scaleF,
                                          60.0f * scaleF, 14.0f * scaleF),
                   juce::Justification::centred);

        float pillX = 508.0f * scaleF;
        float pillY = 287.181f * scaleF;
        float pillW = 60.0f * scaleF;
        float pillH = 22.0f * scaleF;

        float pillR = 11.0f * scaleF;
        g.setColour(KnobDesign::accentColour);
        g.fillRoundedRectangle(pillX, pillY, pillW, pillH, pillR);

        juce::Path pillPath;
        pillPath.addRoundedRectangle(pillX, pillY, pillW, pillH, pillR);

        if (voiceMinusHoverProgress_ > 0.001f)
        {
            g.saveState();
            g.reduceClipRegion(pillPath);
            g.setColour(KnobDesign::accentColour.interpolatedWith(
                KnobDesign::accentHoverColour, voiceMinusHoverProgress_));
            g.fillRect(pillX, pillY, 20.0f * scaleF, pillH);
            g.restoreState();
        }
        if (voicePlusHoverProgress_ > 0.001f)
        {
            g.saveState();
            g.reduceClipRegion(pillPath);
            g.setColour(KnobDesign::accentColour.interpolatedWith(
                KnobDesign::accentHoverColour, voicePlusHoverProgress_));
            g.fillRect(pillX + 40.0f * scaleF, pillY, 20.0f * scaleF, pillH);
            g.restoreState();
        }

        int voiceCount = static_cast<int>(
            processorRef.getAPVTS().getRawParameterValue("voices")->load());

        float minusFontSize = 20.0f * scaleF * (1.0f - 0.3f * voiceMinusClickProgress_);
        g.setColour(KnobDesign::bgColour);
        g.setFont(conjusLAF.getBoldFont(minusFontSize));
        g.drawText("-",
                   juce::Rectangle<float>(510.0f * scaleF, pillY,
                                          16.0f * scaleF, pillH),
                   juce::Justification::centred);

        g.setFont(conjusLAF.getBoldFont(16.0f * scaleF));
        g.drawText(juce::String(voiceCount),
                   juce::Rectangle<float>(526.0f * scaleF, pillY,
                                          24.0f * scaleF, pillH),
                   juce::Justification::centred);

        float plusFontSize = 20.0f * scaleF * (1.0f - 0.3f * voicePlusClickProgress_);
        g.setFont(conjusLAF.getBoldFont(plusFontSize));
        g.drawText("+",
                   juce::Rectangle<float>(550.0f * scaleF, pillY,
                                          16.0f * scaleF, pillH),
                   juce::Justification::centred);
    }

    {
        float velKnobX = 521.0f * scaleF;
        float velKnobY = 362.181f * scaleF;
        float velKnobW = 35.0f * scaleF;
        float velRadius = velKnobW * 0.5f;
        float velCx = velKnobX + velRadius;
        float velCy = velKnobY + velRadius;

        float volSliderBoundsW = (w * 0.2088f) * 0.90f;
        float volSliderH = static_cast<float>(getHeight()) * 0.878f;
        float volDiameter = juce::jmin(juce::jmin(volSliderBoundsW, volSliderH) * 0.78f,
                                       volSliderBoundsW * 0.60f);
        float strokeRef = volDiameter;

        float velStrokeW = strokeRef * KnobDesign::knobStrokeFrac;
        float velIndW    = strokeRef * KnobDesign::indicatorWidthFrac;
        float velTickW   = strokeRef * KnobDesign::tickStrokeFrac;

        float velHover = static_cast<float>(
            velocitySlider.getProperties().getWithDefault("hoverProgress", 0.0));
        auto velInteractive = KnobDesign::accentColour.interpolatedWith(
            KnobDesign::accentHoverColour, velHover);

        g.setColour(KnobDesign::bgColour);
        g.fillEllipse(velCx - velRadius, velCy - velRadius, velKnobW, velKnobW);

        g.setColour(velInteractive);
        g.drawEllipse(velCx - velRadius + velStrokeW * 0.5f,
                      velCy - velRadius + velStrokeW * 0.5f,
                      velKnobW - velStrokeW,
                      velKnobW - velStrokeW,
                      velStrokeW);

        float velNorm = static_cast<float>(velocitySlider.getValue());
        float velAngle = KnobDesign::normToAngleRad(velNorm);
        float velInnerR = velRadius * KnobDesign::indicatorStart;
        float velOuterR = velRadius * KnobDesign::indicatorEnd;
        juce::Path velIndicator;
        velIndicator.startNewSubPath(velCx + std::sin(velAngle) * velInnerR,
                                     velCy - std::cos(velAngle) * velInnerR);
        velIndicator.lineTo(velCx + std::sin(velAngle) * velOuterR,
                            velCy - std::cos(velAngle) * velOuterR);
        g.strokePath(velIndicator,
                     juce::PathStrokeType(velIndW,
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));

        float velTickStartR = velRadius * KnobDesign::tickGap;
        float velTickEndR   = velRadius * (KnobDesign::tickGap + KnobDesign::tickLength);
        float velTickAngles[3] = {
            juce::degreesToRadians(KnobDesign::rotationStartAngle),
            KnobDesign::normToAngleRad(0.5f),
            juce::degreesToRadians(KnobDesign::rotationEndAngle)
        };

        g.setColour(KnobDesign::accentColour);
        float velHalfTickW = velTickW * 0.5f;
        for (int i = 0; i < 3; ++i)
        {
            juce::Path tick;
            tick.startNewSubPath(velCx + std::sin(velTickAngles[i]) * (velTickStartR + velHalfTickW),
                                 velCy - std::cos(velTickAngles[i]) * (velTickStartR + velHalfTickW));
            tick.lineTo(velCx + std::sin(velTickAngles[i]) * (velTickEndR - velHalfTickW),
                        velCy - std::cos(velTickAngles[i]) * (velTickEndR - velHalfTickW));
            g.strokePath(tick,
                         juce::PathStrokeType(velTickW,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
        }

        float velTlFont = 9.0f * scaleF;
        g.setFont(conjusLAF.getBoldFont(velTlFont));
        juce::String velTickLabels[3] = { "0", "50", "100" };
        for (int i = 0; i < 3; ++i)
        {
            float a = velTickAngles[i];
            float lr = velTickEndR + velTlFont * 0.8f;
            float yOff = velTlFont * 0.05f;
            if (i == 1)
            {
                lr = velTickEndR + velTlFont * 0.3f;
                yOff = -velTlFont * 0.5f;
            }
            float lx = velCx + std::sin(a) * lr;
            float ly = velCy - std::cos(a) * lr + yOff;
            g.drawText(velTickLabels[i],
                       juce::Rectangle<float>(lx - velTlFont * 2.5f,
                                              ly - velTlFont * 0.5f,
                                              velTlFont * 5.0f,
                                              velTlFont * 1.2f),
                       juce::Justification::centred, false);
        }

        float velLabelFont = 10.0f * scaleF;
        g.setFont(conjusLAF.getBoldFont(velLabelFont)
            .withExtraKerningFactor(1.0f / velLabelFont));
        g.drawText("VELOCITY",
                   juce::Rectangle<float>(504.0f * scaleF, 324.749f * scaleF,
                                          68.0f * scaleF, 12.0f * scaleF),
                   juce::Justification::centred);

        auto velPillColour = KnobDesign::accentColour.interpolatedWith(
            KnobDesign::accentHoverColour, velHover);
        juce::Rectangle<float> velPillRect(515.778f * scaleF,
                                            410.578f * scaleF,
                                            46.0f * scaleF, 13.0f * scaleF);
        g.setColour(velPillColour);
        g.fillRoundedRectangle(velPillRect, 6.5f * scaleF);

        juce::String velPillText = juce::String(velocitySlider.getValue() * 100.0, 1) + " %";
        g.setColour(KnobDesign::bgColour);
        g.setFont(conjusLAF.getBoldFont(9.0f * scaleF));
        g.drawText(velPillText, velPillRect, juce::Justification::centred);
    }

    drawKeyframeRow(g, scaleF);
}

static juce::String formatHostDivision(HostDivision d)
{
    switch (d)
    {
        case HostDivision::Div1_32:  return "1/32";
        case HostDivision::Div1_16:  return "1/16";
        case HostDivision::Div1_8:   return "1/8";
        case HostDivision::Div1_4:   return "1/4";
        case HostDivision::Div1_2:   return "1/2";
        case HostDivision::Div1_1:   return "1/1";
        case HostDivision::Div1_8d:  return "1/8d";
        case HostDivision::Div1_4d:  return "1/4d";
        case HostDivision::Div1_2d:  return "1/2d";
        case HostDivision::Div1_16t: return "1/16t";
        case HostDivision::Div1_8t:  return "1/8t";
        case HostDivision::Div1_4t:  return "1/4t";
        case HostDivision::Div1_2t:  return "1/2t";
        default:                     return "1/4";
    }
}

static juce::String formatInternalMs(float ms)
{
    if (ms >= 1000.0f)
        return juce::String(ms / 1000.0f, ms >= 10000.0f ? 1 : 2) + "s";
    return juce::String(static_cast<int>(std::round(ms))) + "ms";
}

void QuirkAudioProcessorEditor::drawKeyframeRow(juce::Graphics& g, float scaleF)
{
    auto& engine = processorRef.getAnimationEngine();
    const bool animatedMode = engine.getAnimatedMode();
    const bool hostClock    = engine.getHostClock();
    const int  selected     = engine.getSelectedKeyframe();

    const float kfX[4]      = { 166.75f, 266.75f, 366.75f, 466.75f };
    const float transX[3]   = { 190.75f, 290.75f, 390.75f };
    const float pillX[3]    = { 206.75f, 306.75f, 406.75f };
    const float kfY = 465.181f, kfSize = 24.0f;
    const float transY = 476.181f, transW = 76.0f, transH = 2.0f;
    const float pillY = 470.181f, pillW = 44.0f, pillH = 14.0f;

    const float loopBottomY = 508.181f;
    const float loopPillX = 306.75f;
    const float loopPillY = loopBottomY - pillH * 0.5f;
    const float loopCornerR = 4.0f;

    for (int i = 0; i < 3; ++i)
    {
        g.setColour(juce::Colour(0xff8a5500));
        g.fillRect(transX[i] * scaleF, transY * scaleF, transW * scaleF, transH * scaleF);
    }

    {
        const float dBotX  = (kfX[3] + kfSize * 0.5f) * scaleF;
        const float aBotX  = (kfX[0] + kfSize * 0.5f) * scaleF;
        const float kfBotY = (kfY + kfSize) * scaleF;
        const float botY   = loopBottomY * scaleF;
        const float cr     = loopCornerR * scaleF;
        juce::Path loop;
        loop.startNewSubPath(dBotX, kfBotY);
        loop.lineTo(dBotX, botY - cr);
        loop.quadraticTo(dBotX, botY, dBotX - cr, botY);
        loop.lineTo(aBotX + cr, botY);
        loop.quadraticTo(aBotX, botY, aBotX, botY - cr);
        loop.lineTo(aBotX, kfBotY);
        g.setColour(juce::Colour(0xff8a5500));
        g.strokePath(loop, juce::PathStrokeType(transH * scaleF));
    }

    for (int i = 0; i < 4; ++i)
    {
        const float x = kfX[i] * scaleF;
        const float y = kfY * scaleF;
        const float s = kfSize * scaleF;
        const float strokeRef = 73.28f * scaleF;
        const float strokeW = strokeRef * KnobDesign::knobStrokeFrac * 0.7f;

        const bool isSelected = (i == selected);
        const bool isStaticDim = (!animatedMode && i != 0);
        const float hover = keyframeHoverProgress_[i];
        const float clickP = keyframeClickProgress_[i];
        const float shrink = clickP * 0.06f;
        const float drawX = x + s * shrink;
        const float drawY = y + s * shrink;
        const float drawS = s - 2.0f * s * shrink;

        juce::Colour fillC, textC, borderC;
        if (isSelected)
        {
            fillC = KnobDesign::accentColour.interpolatedWith(KnobDesign::accentHoverColour, hover);
            textC = KnobDesign::bgColour;
            borderC = fillC;
        }
        else if (isStaticDim)
        {
            fillC = KnobDesign::bgColour;
            textC = juce::Colour(0xff6a4200);
            borderC = juce::Colour(0xff6a4200);
        }
        else
        {
            fillC = KnobDesign::bgColour;
            textC = KnobDesign::accentColour.interpolatedWith(KnobDesign::accentHoverColour, hover);
            borderC = textC;
        }

        g.setColour(fillC);
        g.fillEllipse(drawX, drawY, drawS, drawS);
        g.setColour(borderC);
        g.drawEllipse(drawX + strokeW * 0.5f, drawY + strokeW * 0.5f,
                      drawS - strokeW, drawS - strokeW, strokeW);

        const float letterFontSize = 13.0f * scaleF * (1.0f - 0.2f * clickP);
        g.setColour(textC);
        g.setFont(conjusLAF.getBoldFont(letterFontSize));
        static const char* keyframeLabels[4] = { "A", "B", "C", "D" };
        g.drawText(keyframeLabels[i],
                   juce::Rectangle<float>(drawX, drawY, drawS, drawS),
                   juce::Justification::centred);
    }

    for (int i = 0; i < 4; ++i)
    {
        float px, py;
        if (i < 3) { px = pillX[i] * scaleF; py = pillY * scaleF; }
        else       { px = loopPillX * scaleF; py = loopPillY * scaleF; }
        const float pw = pillW * scaleF;
        const float ph = pillH * scaleF;

        g.setColour(KnobDesign::accentColour);
        g.fillRoundedRectangle(px, py, pw, ph, ph * 0.5f);

        juce::Path pillPath;
        pillPath.addRoundedRectangle(px, py, pw, ph, ph * 0.5f);

        const float sideW = 10.0f * scaleF;
        if (durationPillMinusHoverProgress_[i] > 0.001f)
        {
            g.saveState();
            g.reduceClipRegion(pillPath);
            g.setColour(KnobDesign::accentColour.interpolatedWith(
                KnobDesign::accentHoverColour, durationPillMinusHoverProgress_[i]));
            g.fillRect(px, py, sideW, ph);
            g.restoreState();
        }
        if (durationPillPlusHoverProgress_[i] > 0.001f)
        {
            g.saveState();
            g.reduceClipRegion(pillPath);
            g.setColour(KnobDesign::accentColour.interpolatedWith(
                KnobDesign::accentHoverColour, durationPillPlusHoverProgress_[i]));
            g.fillRect(px + pw - sideW, py, sideW, ph);
            g.restoreState();
        }

        const juce::String label = hostClock
            ? formatHostDivision(engine.getHostDuration(i))
            : formatInternalMs(engine.getInternalDuration(i));

        const float minusFontSize = 11.0f * scaleF * (1.0f - 0.3f * durationPillMinusClickProgress_[i]);
        const float plusFontSize  = 11.0f * scaleF * (1.0f - 0.3f * durationPillPlusClickProgress_[i]);

        g.setColour(KnobDesign::bgColour);
        g.setFont(conjusLAF.getBoldFont(minusFontSize));
        g.drawText("-",
                   juce::Rectangle<float>(px, py, sideW, ph),
                   juce::Justification::centred);

        g.setFont(conjusLAF.getBoldFont(12.0f * scaleF));
        g.drawText(label,
                   juce::Rectangle<float>(px + sideW, py, pw - 2.0f * sideW, ph),
                   juce::Justification::centred);

        g.setFont(conjusLAF.getBoldFont(plusFontSize));
        g.drawText("+",
                   juce::Rectangle<float>(px + pw - sideW, py, sideW, ph),
                   juce::Justification::centred);
    }

    {
        const float bx = 72.0f  * scaleF, by = 477.181f * scaleF;
        const float bw = 70.0f  * scaleF, bh = 18.0f * scaleF;
        const float ly = 465.181f * scaleF, lh = 10.0f * scaleF;
        const float clickShrink = modeButtonClickProgress_ * 0.06f;
        const float dbx = bx + bw * clickShrink;
        const float dby = by + bh * clickShrink * 1.5f;
        const float dbw = bw - 2.0f * bw * clickShrink;
        const float dbh = bh - 2.0f * bh * clickShrink * 1.5f;

        const float labelCenterX = bx + bw * 0.5f;
        const float labelDrawW = 100.0f * scaleF;
        const float modeLabelFont = 9.0f * scaleF;
        g.setColour(KnobDesign::accentColour);
        g.setFont(conjusLAF.getBoldFont(modeLabelFont).withExtraKerningFactor(1.0f / modeLabelFont));
        g.drawText("MODE",
                   juce::Rectangle<float>(labelCenterX - labelDrawW * 0.5f, ly, labelDrawW, lh),
                   juce::Justification::centred);

        const juce::Colour interactiveAccent = KnobDesign::accentColour.interpolatedWith(
            KnobDesign::accentHoverColour, modeButtonHoverProgress_);
        const juce::Colour fill = animatedMode
            ? interactiveAccent
            : KnobDesign::bgColour;
        const juce::Colour textC = animatedMode
            ? KnobDesign::bgColour
            : interactiveAccent;
        const juce::Colour borderC = interactiveAccent;
        const float strokeRef = 73.28f * scaleF;
        const float borderW = strokeRef * KnobDesign::knobStrokeFrac * 0.7f;

        g.setColour(fill);
        g.fillRoundedRectangle(dbx, dby, dbw, dbh, dbh * 0.5f);
        if (!animatedMode)
        {
            g.setColour(borderC);
            g.drawRoundedRectangle(dbx + borderW * 0.5f, dby + borderW * 0.5f,
                                   dbw - borderW, dbh - borderW,
                                   dbh * 0.5f, borderW);
        }

        g.setColour(textC);
        g.setFont(conjusLAF.getBoldFont(9.0f * scaleF).withExtraKerningFactor(1.0f / (9.0f * scaleF) * 0.5f));
        g.drawText(animatedMode ? "WAVESCAN" : "STATIC",
                   juce::Rectangle<float>(dbx, dby, dbw, dbh),
                   juce::Justification::centred);
    }

    {
        const float bx = 506.0f * scaleF, by = 477.181f * scaleF;
        const float bw = 70.0f  * scaleF, bh = 18.0f * scaleF;
        const float ly = 464.181f * scaleF, lh = 10.0f * scaleF;
        const float clickShrink = clockSourceButtonClickProgress_ * 0.06f;
        const float dbx = bx + bw * clickShrink;
        const float dby = by + bh * clickShrink * 1.5f;
        const float dbw = bw - 2.0f * bw * clickShrink;
        const float dbh = bh - 2.0f * bh * clickShrink * 1.5f;

        const float labelCenterX = bx + bw * 0.5f;
        const float labelDrawW = 100.0f * scaleF;
        const float csLabelFont = 8.5f * scaleF;
        g.setColour(KnobDesign::accentColour);
        g.setFont(conjusLAF.getBoldFont(csLabelFont).withExtraKerningFactor(1.0f / csLabelFont));
        g.drawText("CLOCK SOURCE",
                   juce::Rectangle<float>(labelCenterX - labelDrawW * 0.5f, ly, labelDrawW, lh),
                   juce::Justification::centred);

        const juce::Colour interactiveAccent = KnobDesign::accentColour.interpolatedWith(
            KnobDesign::accentHoverColour, clockSourceButtonHoverProgress_);
        const juce::Colour fill = (!hostClock)
            ? interactiveAccent
            : KnobDesign::bgColour;
        const juce::Colour textC = (!hostClock)
            ? KnobDesign::bgColour
            : interactiveAccent;
        const juce::Colour borderC = interactiveAccent;
        const float strokeRef = 73.28f * scaleF;
        const float borderW = strokeRef * KnobDesign::knobStrokeFrac * 0.7f;

        g.setColour(fill);
        g.fillRoundedRectangle(dbx, dby, dbw, dbh, dbh * 0.5f);
        if (hostClock)
        {
            g.setColour(borderC);
            g.drawRoundedRectangle(dbx + borderW * 0.5f, dby + borderW * 0.5f,
                                   dbw - borderW, dbh - borderW,
                                   dbh * 0.5f, borderW);
        }

        g.setColour(textC);
        g.setFont(conjusLAF.getBoldFont(9.0f * scaleF).withExtraKerningFactor(1.0f / (9.0f * scaleF) * 0.5f));
        g.drawText(hostClock ? "HOST" : "INTERNAL",
                   juce::Rectangle<float>(dbx, dby, dbw, dbh),
                   juce::Justification::centred);
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

    float sF = w / static_cast<float>(KnobDesign::defaultWidth);

    float knobColW = w * 0.2088f;
    float halfCol = knobColW * 0.5f;
    float knobColX = w * (108.666f / 650.0f) - halfCol;

    volumeLabel.setFont(conjusLAF.getBoldFont(14.0f * sF));
    volumeLabel.setBounds(static_cast<int>(72.0f * sF), static_cast<int>(266.031f * sF),
                          static_cast<int>(72.0f * sF), static_cast<int>(16.0f * sF));

    int sliderBottom = static_cast<int>(h * 0.918f);
    int sliderTop = static_cast<int>(h * 0.04f);
    int sliderH = sliderBottom - sliderTop;

    const float knobClusterExtraShift = KnobDesign::knobClusterShiftDefault * (h / static_cast<float>(KnobDesign::defaultHeight));
    const int sliderTopEditor = sliderTop + static_cast<int>(knobClusterExtraShift);

    float sliderBoundsW = knobColW * 0.90f;
    float sliderOffset = knobColX + (knobColW - sliderBoundsW) * 0.5f;

    volumeSlider.setMouseDragSensitivity(static_cast<int>(w * 0.5f));
    volumeSlider.setBounds(static_cast<int>(sliderOffset), sliderTopEditor,
                           static_cast<int>(sliderBoundsW), sliderH);
    volumeSlider.setPaintingIsUnclipped(true);

    float knobDiameter = juce::jmin(juce::jmin(sliderBoundsW, static_cast<float>(sliderH)) * 0.78f,
                                    sliderBoundsW * 0.60f);
    float knobStrokeW = knobDiameter * KnobDesign::knobStrokeFrac;
    bypassButton.setRingStrokeWidth(knobStrokeW);

    {
        const float faderTrackH = 120.0f;
        const float faderCapW = 28.0f;
        const float faderTrackXs[4] = { 216.75f, 286.75f, 356.75f, 426.75f };
        const float faderTrackY = 288.655f;

        juce::Slider* faderSliders[4] = { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider };

        for (int i = 0; i < 4; ++i)
        {
            float fx = (faderTrackXs[i] - 11.0f) * sF;
            float fy = faderTrackY * sF;
            float fw = faderCapW * sF;
            float fh = faderTrackH * sF;

            faderSliders[i]->setMouseDragSensitivity(static_cast<int>(faderTrackH * sF));
            faderSliders[i]->setBounds(static_cast<int>(fx), static_cast<int>(fy),
                                        static_cast<int>(fw), static_cast<int>(fh));
            faderSliders[i]->setPaintingIsUnclipped(true);
        }
    }

    {
        float velX = 521.0f * sF;
        float velY = 362.181f * sF;
        float velSize = 35.0f * sF;
        velocitySlider.setMouseDragSensitivity(static_cast<int>(w * 0.5f));
        velocitySlider.setBounds(static_cast<int>(velX), static_cast<int>(velY),
                                  static_cast<int>(velSize), static_cast<int>(velSize));
        velocitySlider.setPaintingIsUnclipped(true);
    }
}

void QuirkAudioProcessorEditor::startSnapAnimation(juce::Slider& slider, SliderAnimation& anim, const juce::String& paramId)
{
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
        float hx = sh.dx * sign, hy = sh.dy * sign;
        checkAt(hx, hy, BezierHitType::StartOutHandle, -1, isLeft, handleHitR);

        auto eh = c.getEndInHandle();
        float ehx = (1.0f + eh.dx) * sign;
        float ehy = eh.dy * sign;
        checkAt(ehx, ehy, BezierHitType::EndInHandle, -1, isLeft, handleHitR);

        for (int i = 0; i < c.getNumPoints(); ++i)
        {
            auto& pt = c.getPoint(i);
            float pdx = pt.x * sign, pdy = pt.y * sign;
            checkAt(pdx, pdy, BezierHitType::Point, i, isLeft, hitR);

            float ihx = (pt.x + pt.in.dx) * sign;
            float ihy = (pt.y + pt.in.dy) * sign;
            checkAt(ihx, ihy, BezierHitType::InHandle, i, isLeft, handleHitR);

            float ohx = (pt.x + pt.out.dx) * sign;
            float ohy = (pt.y + pt.out.dy) * sign;
            checkAt(ohx, ohy, BezierHitType::OutHandle, i, isLeft, handleHitR);
        }
    };

    auto& curve = processorRef.getBezierCurve();
    checkCurve(curve, false, false);

    auto& leftCurve = processorRef.getLeftBezierCurve();
    checkCurve(leftCurve, true, true);

    return best;
}

float QuirkAudioProcessorEditor::distToNearestCurvePoint(float bx, float by) const
{
    float sign = (bx < 0.0f) ? -1.0f : 1.0f;
    auto& c = (bx < 0.0f) ? processorRef.getLeftBezierCurve()
                           : processorRef.getBezierCurve();
    float curveY = sign * c.evaluate(std::abs(bx));

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

    {
        float sF = static_cast<float>(getWidth()) / static_cast<float>(KnobDesign::defaultWidth);
        float pillX = 508.0f * sF;
        float pillY = 287.181f * sF;
        float pillW = 60.0f * sF;
        float pillH = 22.0f * sF;

        if (mx >= pillX && mx <= pillX + pillW && my >= pillY && my <= pillY + pillH)
        {
            auto* param = processorRef.getAPVTS().getParameter("voices");
            int current = static_cast<int>(param->convertFrom0to1(param->getValue()));

            if (mx < pillX + 20.0f * sF)
            {
                if (current > 1)
                    param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(current - 1)));
                voiceMinusClickProgress_ = 1.0f;
                repaint();
            }
            else if (mx > pillX + 40.0f * sF)
            {
                if (current < 10)
                    param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(current + 1)));
                voicePlusClickProgress_ = 1.0f;
                repaint();
            }
            return;
        }
    }

    {
        float sF2 = static_cast<float>(getWidth()) / static_cast<float>(KnobDesign::defaultWidth);
        float faderPillXs[4] = { 199.75f, 269.75f, 339.75f, 409.75f };
        juce::Slider* faderSliders[4] = { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider };
        for (int i = 0; i < 4; ++i)
        {
            float px = faderPillXs[i] * sF2;
            float py = 418.155f * sF2;
            if (mx >= px && mx <= px + 60.0f * sF2 && my >= py && my <= py + 22.0f * sF2)
            {
                pillDragSlider_ = faderSliders[i];
                pillDragStartY_ = my;
                pillDragStartValue_ = faderSliders[i]->getValue();
                pillDragSensitivity_ = 120.0f * sF2;
                return;
            }
        }

        float volPx = 76.86f * sF2, volPy = 416.681f * sF2;
        if (mx >= volPx && mx <= volPx + 60.28f * sF2 && my >= volPy && my <= volPy + 21.5f * sF2)
        {
            pillDragSlider_ = &volumeSlider;
            pillDragStartY_ = my;
            pillDragStartValue_ = volumeSlider.getValue();
            pillDragSensitivity_ = static_cast<float>(getWidth()) * 0.5f;
            return;
        }

        float velPx = 515.778f * sF2, velPy = 410.578f * sF2;
        if (mx >= velPx && mx <= velPx + 46.0f * sF2 && my >= velPy && my <= velPy + 13.0f * sF2)
        {
            pillDragSlider_ = &velocitySlider;
            pillDragStartY_ = my;
            pillDragStartValue_ = velocitySlider.getValue();
            pillDragSensitivity_ = static_cast<float>(getWidth()) * 0.5f;
            return;
        }

        const float kfX[4] = { 166.75f, 266.75f, 366.75f, 466.75f };
        for (int i = 0; i < 4; ++i)
        {
            const float kx = kfX[i] * sF2;
            const float ky = 465.181f * sF2;
            const float ks = 24.0f * sF2;
            if (mx >= kx && mx <= kx + ks && my >= ky && my <= ky + ks)
            {
                keyframeClickProgress_[i] = 1.0f;
                processorRef.getAnimationEngine().selectKeyframe(i);
                processorRef.loadSelectedKeyframeIntoApvts();
                repaint();
                return;
            }
        }

        const float pillXs[4] = { 206.75f, 306.75f, 406.75f, 306.75f };
        const float pillYs[4] = { 470.181f, 470.181f, 470.181f, 501.181f };
        const float dpW = 44.0f, dpH = 14.0f, sideW = 10.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float px = pillXs[i] * sF2;
            const float py = pillYs[i] * sF2;
            const float pw = dpW * sF2;
            const float ph = dpH * sF2;
            if (mx >= px && mx <= px + pw && my >= py && my <= py + ph)
            {
                auto& engine = processorRef.getAnimationEngine();
                const bool host = engine.getHostClock();
                if (mx <= px + sideW * sF2)
                {
                    durationPillMinusClickProgress_[i] = 1.0f;
                    if (host)
                    {
                        int idx = static_cast<int>(engine.getHostDuration(i));
                        if (idx > 0) engine.setHostDuration(i, static_cast<HostDivision>(idx - 1));
                    }
                    else
                    {
                        float v = engine.getInternalDuration(i);
                        engine.setInternalDuration(i, juce::jmax(50.0f, v - 100.0f));
                    }
                    repaint();
                }
                else if (mx >= px + pw - sideW * sF2)
                {
                    durationPillPlusClickProgress_[i] = 1.0f;
                    if (host)
                    {
                        int idx = static_cast<int>(engine.getHostDuration(i));
                        if (idx < static_cast<int>(HostDivision::NumDivisions) - 1)
                            engine.setHostDuration(i, static_cast<HostDivision>(idx + 1));
                    }
                    else
                    {
                        float v = engine.getInternalDuration(i);
                        engine.setInternalDuration(i, juce::jmin(60000.0f, v + 100.0f));
                    }
                    repaint();
                }
                else
                {
                    durationDragTransition_ = i;
                    durationDragStartY_ = my;
                    durationDragStartMs_ = engine.getInternalDuration(i);
                    durationDragStartHostDiv_ = static_cast<int>(engine.getHostDuration(i));
                }
                return;
            }
        }

        const float modeBtnX = 72.0f * sF2, modeBtnY = 477.181f * sF2;
        const float modeBtnW = 70.0f * sF2, modeBtnH = 18.0f * sF2;
        if (mx >= modeBtnX && mx <= modeBtnX + modeBtnW && my >= modeBtnY && my <= modeBtnY + modeBtnH)
        {
            modeButtonClickProgress_ = 1.0f;
            auto& engine = processorRef.getAnimationEngine();
            const bool newMode = !engine.getAnimatedMode();
            engine.setAnimatedMode(newMode);
            if (!newMode)
                processorRef.loadSelectedKeyframeIntoApvts();
            repaint();
            return;
        }

        const float csBtnX = 506.0f * sF2, csBtnY = 477.181f * sF2;
        const float csBtnW = 70.0f * sF2, csBtnH = 18.0f * sF2;
        if (mx >= csBtnX && mx <= csBtnX + csBtnW && my >= csBtnY && my <= csBtnY + csBtnH)
        {
            clockSourceButtonClickProgress_ = 1.0f;
            auto& engine = processorRef.getAnimationEngine();
            engine.setHostClock(!engine.getHostClock());
            repaint();
            return;
        }
    }

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
    if (pillDragSlider_ != nullptr)
    {
        float my = static_cast<float>(e.getPosition().y);
        double range = pillDragSlider_->getMaximum() - pillDragSlider_->getMinimum();
        double delta = static_cast<double>(pillDragStartY_ - my)
                     / static_cast<double>(pillDragSensitivity_) * range;
        double newVal = juce::jlimit(pillDragSlider_->getMinimum(),
                                     pillDragSlider_->getMaximum(),
                                     pillDragStartValue_ + delta);
        pillDragSlider_->setValue(newVal, juce::sendNotificationSync);
        return;
    }

    if (durationDragTransition_ >= 0)
    {
        float my = static_cast<float>(e.getPosition().y);
        float deltaY = durationDragStartY_ - my;
        auto& engine = processorRef.getAnimationEngine();
        if (engine.getHostClock())
        {
            int stepDelta = static_cast<int>(std::round(deltaY / 12.0f));
            int newIdx = juce::jlimit(0,
                                      static_cast<int>(HostDivision::NumDivisions) - 1,
                                      durationDragStartHostDiv_ + stepDelta);
            engine.setHostDuration(durationDragTransition_, static_cast<HostDivision>(newIdx));
        }
        else
        {
            float msPerPixel = 20.0f;
            float newMs = juce::jlimit(50.0f, 60000.0f,
                                       durationDragStartMs_ + deltaY * msPerPixel);
            engine.setInternalDuration(durationDragTransition_, newMs);
        }
        repaint();
        return;
    }

    if (!dragging_) return;

    float mx = static_cast<float>(e.getPosition().x);
    float my = static_cast<float>(e.getPosition().y);
    auto bz = pixelToBezier(mx, my);
    float absBx = std::abs(bz.x);
    float absby = (bz.x < 0.0f) ? -bz.y : bz.y;

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
            setParam(dragPrefix_ + "eh_dy", absby);
            break;
        default:
            break;
    }

    processorRef.updateDisplayCurves();
    repaint(graphBounds);
}

void QuirkAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    if (pillDragSlider_ != nullptr)
    {
        pillDragSlider_ = nullptr;
        return;
    }
    if (durationDragTransition_ >= 0)
    {
        durationDragTransition_ = -1;
        return;
    }
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
            const auto onId = prefix + "p" + juce::String(slot) + "_on";
            setParam(onId, 0.0f);
            processorRef.setKeyframeParamForAllFrames(onId, 0.0f);
            processorRef.updateDisplayCurves();
            repaint(graphBounds);
        }
        return;
    }

    auto bz = pixelToBezier(mx, my);
    float absBx = std::clamp(std::abs(bz.x), 0.0f, 1.0f);
    float absby = (bz.x < 0.0f) ? -bz.y : bz.y;

    float curveHitDist = distToNearestCurvePoint(bz.x, bz.y);
    if (curveHitDist > 12.0f * gc_.scaleF)
        return;

    bool useLeft = bz.x < 0.0f;
    auto prefix = curvePrefix(useLeft);
    int slot = processorRef.findFreeSlot(prefix);
    if (slot < 0) return;

    auto s = juce::String(slot);
    float defDx = std::min(absBx, 1.0f - absBx) / 3.0f;
    const juce::String pids[7] = {
        prefix + "p" + s + "_on",
        prefix + "p" + s + "_x",
        prefix + "p" + s + "_y",
        prefix + "p" + s + "_idx",
        prefix + "p" + s + "_idy",
        prefix + "p" + s + "_odx",
        prefix + "p" + s + "_ody",
    };
    const float vals[7] = { 1.0f, absBx, absby, -defDx, 0.0f, defDx, 0.0f };
    for (int i = 0; i < 7; ++i)
    {
        setParam(pids[i], vals[i]);
        processorRef.setKeyframeParamForAllFrames(pids[i], vals[i]);
    }
    processorRef.updateDisplayCurves();
    repaint(graphBounds);
}

void QuirkAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& e,
                                                const juce::MouseWheelDetails& wheel)
{
    auto pos = getLocalPoint(e.eventComponent, e.getPosition());
    float sF = static_cast<float>(getWidth()) / static_cast<float>(KnobDesign::defaultWidth);

    float faderPillXs[4] = { 199.75f, 269.75f, 339.75f, 409.75f };
    juce::Slider* faderSliders[4] = { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider };
    for (int i = 0; i < 4; ++i)
    {
        float px = faderPillXs[i] * sF;
        float py = 418.155f * sF;
        if (pos.x >= px && pos.x <= px + 60.0f * sF
            && pos.y >= py && pos.y <= py + 22.0f * sF)
        {
            auto localE = e.getEventRelativeTo(faderSliders[i]);
            faderSliders[i]->mouseWheelMove(localE, wheel);
            return;
        }
    }

    float volPx = 76.86f * sF, volPy = 416.681f * sF;
    if (pos.x >= volPx && pos.x <= volPx + 60.28f * sF
        && pos.y >= volPy && pos.y <= volPy + 21.5f * sF)
    {
        auto localE = e.getEventRelativeTo(&volumeSlider);
        volumeSlider.mouseWheelMove(localE, wheel);
        return;
    }

    float velPx = 515.778f * sF, velPy = 410.578f * sF;
    if (pos.x >= velPx && pos.x <= velPx + 46.0f * sF
        && pos.y >= velPy && pos.y <= velPy + 13.0f * sF)
    {
        auto localE = e.getEventRelativeTo(&velocitySlider);
        velocitySlider.mouseWheelMove(localE, wheel);
        return;
    }

    juce::AudioProcessorEditor::mouseWheelMove(e, wheel);
}
