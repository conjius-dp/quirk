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
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 120, 30);
    volumeSlider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible(volumeSlider);

    volumeLabel.setJustificationType(juce::Justification::centredBottom);
    addAndMakeVisible(volumeLabel);
    volumeLabel.toBack();

    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "volume", volumeSlider);

    volumeSlider.textFromValueFunction = [](double value) -> juce::String {
        return juce::String(value * 100.0, 1) + " %";
    };
    volumeSlider.valueFromTextFunction = [](const juce::String& text) -> double {
        return text.getDoubleValue() / 100.0;
    };
    volumeSlider.updateText();

    volumeSlider.onDoubleClick = [this]() {
        startSnapAnimation(volumeSlider, volumeAnim);
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
    animateHover(volumeSlider, volumeSlider.isMouseOverOrDragging(true));

    updateSnapAnimation(volumeSlider, volumeAnim);

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
        int baseSize = static_cast<int>(30.0f * scale);
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
        float gOuterLeft = 56.25f * scaleF;
        float gOuterTop = 61.0f * scaleF;
        float gOuterW = 527.0f * scaleF;
        float gOuterH = 193.0f * scaleF;

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
            float logoX = 260.5f * scaleF;
            float logoY = 267.85f * scaleF;
            float logoW = 129.0f * scaleF;
            float logoH = 50.0f * scaleF;
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

            float stX = 313.75f * scaleF;
            float stY = 314.524f * scaleF;
            float stW = 88.0f * scaleF;
            float stFontSize = 10.0f * scaleF;
            g.setColour(KnobDesign::accentColour);
            g.setFont(conjusLAF.getBoldFont(stFontSize));
            auto attr = juce::AttributedString();
            attr.setJustification(juce::Justification::centred);
            attr.setWordWrap(juce::AttributedString::none);
            attr.append("WAVESHAPING\nSYNTHESIZER",
                        conjusLAF.getBoldFont(stFontSize), KnobDesign::accentColour);
            juce::TextLayout subtitleLayout;
            subtitleLayout.createLayout(attr, stW);
            subtitleLayout.draw(g, juce::Rectangle<float>(stX, stY, stW, 34.0f * scaleF));
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

    float knobColW = w * 0.28f;
    float halfCol = knobColW * 0.5f;
    float knobColX = w * (132.0f / 650.0f) - halfCol;

    volumeLabel.setFont(conjusLAF.getBoldFont(21.5f * sF));
    volumeLabel.setBounds(static_cast<int>(96.0f * sF), static_cast<int>(259.35f * sF),
                          static_cast<int>(72.0f * sF), static_cast<int>(17.0f * sF));

    float dbFontSize = w * KnobDesign::textBoxFontScale;
    int sliderBottom = static_cast<int>(h * 0.918f);
    int sliderTop = static_cast<int>(h * 0.04f);
    int sliderH = sliderBottom - sliderTop;

    const float knobClusterExtraShift = KnobDesign::knobClusterShiftDefault * (h / static_cast<float>(KnobDesign::defaultHeight));
    const int sliderTopEditor = sliderTop + static_cast<int>(knobClusterExtraShift);

    float sliderBoundsW = knobColW * 0.90f;
    float sliderOffset = knobColX + (knobColW - sliderBoundsW) * 0.5f;

    int textBoxW = static_cast<int>(sliderBoundsW * 0.95f);
    int textBoxH = static_cast<int>(dbFontSize * 2.6f);

    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textBoxW, textBoxH);
    volumeSlider.setMouseDragSensitivity(static_cast<int>(w * 0.5f));
    volumeSlider.setBounds(static_cast<int>(sliderOffset), sliderTopEditor,
                           static_cast<int>(sliderBoundsW), sliderH);

    volumeSlider.setPaintingIsUnclipped(true);
    if (auto* textBox = volumeSlider.getChildComponent(0))
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

    float knobAreaH = static_cast<float>(sliderH) - static_cast<float>(textBoxH);
    float knobDiameter = juce::jmin(sliderBoundsW, knobAreaH) * 0.78f;
    float knobStrokeW = knobDiameter * KnobDesign::knobStrokeFrac;
    bypassButton.setRingStrokeWidth(knobStrokeW);
}

void QuirkAudioProcessorEditor::startSnapAnimation(juce::Slider& slider, SliderAnimation& anim)
{
    auto* param = processorRef.getAPVTS().getParameter("volume");
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
