#include "MainComponent.h"
//==============================================================================
MainComponent::MainComponent()
{
    setLookAndFeel(&darkLAF);
    setSize(600, 800);  // Slightly wider for better proportions
    formatManager.registerBasicFormats();

    menuBar = std::make_unique<AccessibleMenuBar>(this, this);
    addAndMakeVisible(menuBar.get());
    if (menuBar) {
        menuBar->setModel(this);
        menuBar->setBounds(0, 0, getWidth(), 30); // Slightly taller for 20pt text
    }
    setWantsKeyboardFocus(true);
    addKeyListener(this);
    juce::MessageManager::callAsync([this]() { grabKeyboardFocus(); });
    setAudioChannels(0, 2);
    
    startTimerHz(60);
}

MainComponent::~MainComponent()
{
    setLookAndFeel(nullptr);
    shutdownAudio();
}

//==============================================================================
// ── UI Helpers ────────────────────────────────────────────────────────────
namespace
{
    void drawEffectBox(juce::Graphics& g, juce::Rectangle<int> rect,
                       const juce::String& title, const juce::String& status,
                       bool isActive, bool isEditMode, juce::Colour accent)
    {
        g.setColour(juce::Colour(DarkLookAndFeel::bg2));
        g.fillRoundedRectangle(rect.toFloat(), 6.0f);
        g.setColour(juce::Colour(DarkLookAndFeel::line1));
        g.drawRoundedRectangle(rect.toFloat(), 6.0f, 1.0f);

        if (isActive || isEditMode)
        {
            g.setColour(accent.withAlpha(0.12f));
            g.fillRoundedRectangle(rect.toFloat(), 6.0f);
            g.setColour(accent.withAlpha(0.60f));
            g.drawRoundedRectangle(rect.toFloat(), 6.0f, 1.5f);
        }

        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.setColour(isActive ? accent : juce::Colour(DarkLookAndFeel::txt0));
        g.drawText(title, rect.getX() + 10, rect.getY() + 4, rect.getWidth() - 20, 22, juce::Justification::centredLeft);

        g.setFont(juce::Font(14.0f));
        g.setColour(isActive ? accent.brighter(0.20f) : juce::Colour(DarkLookAndFeel::txt1));
        g.drawFittedText(status, rect.getX() + 10, rect.getY() + 26, rect.getWidth() - 20, rect.getHeight() - 30, juce::Justification::centredLeft, 2);
    }
} // namespace

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(DarkLookAndFeel::bg0));
    auto bounds = getLocalBounds().reduced(16, 16);
    bounds.removeFromTop(30); // Match menu bar height

    // 🔒 Early exit
    if (bounds.getWidth() < 340 || bounds.getHeight() < 580)
    {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawText("Resize window to view UI", getLocalBounds(), juce::Justification::centred);
        return;
    }

    // ── Frequency Response Curve ────────────────────────────────────────────
    auto curveBounds = bounds.removeFromTop(140);
    g.setColour(juce::Colour(DarkLookAndFeel::bg2));
    g.fillRoundedRectangle(curveBounds.toFloat(), 6.0f);
    g.setColour(juce::Colour(DarkLookAndFeel::line1));
    g.drawRoundedRectangle(curveBounds.toFloat(), 6.0f, 1.0f);

    if (curveBounds.getHeight() > 50)
    {
        const int dbGrid[] = { 12, 6, 0, -6, -12 };
        g.setFont(juce::Font(11.0f));
        for (int db : dbGrid)
        {
            float frac = juce::jmap((double)db, 24.0, -24.0, 0.0, 1.0);
            int lineY = curveBounds.getY() + (int)(frac * curveBounds.getHeight());
            g.setColour(juce::Colour(DarkLookAndFeel::line1));
            g.drawHorizontalLine(lineY, (float)curveBounds.getX() + 4, (float)curveBounds.getRight() - 4);
            g.setColour(juce::Colour(DarkLookAndFeel::txt2));
            g.drawText(juce::String(db) + " dB", juce::Rectangle<int>(curveBounds.getX() + 2, lineY - 9, 42, 18), juce::Justification::centredRight, false);
        }

        g.setFont(juce::Font(11.0f));
        double minFreq = 20.0, maxFreq = 20000.0;
        const int freqGrid[] = { 50, 100, 500, 1000, 5000, 10000, 20000 };
        for (int f : freqGrid)
        {
            double prop = std::log((double)f / minFreq) / std::log(maxFreq / minFreq);
            int lineX = curveBounds.getX() + (int)(prop * curveBounds.getWidth());
            if (lineX > curveBounds.getX() + 10 && lineX < curveBounds.getRight() - 10)
            {
                g.setColour(juce::Colour(DarkLookAndFeel::line1));
                g.drawVerticalLine(lineX, (float)curveBounds.getY(), (float)curveBounds.getBottom());
                g.setColour(juce::Colour(DarkLookAndFeel::txt2));
                juce::String lbl = (f >= 1000) ? juce::String(f / 1000) + "k" : juce::String(f);
                g.drawText(lbl, juce::Rectangle<int>(lineX - 14, curveBounds.getBottom() - 18, 28, 18), juce::Justification::centred, false);
            }
        }
    }

    // Draw response curve
    {
        juce::Path curve;
        int numPoints = juce::jmax(1, curveBounds.getWidth());
        double minFreq = 20.0, maxFreq = 20000.0;
        for (int x = 0; x <= numPoints; ++x)
        {
            double proportion = (double)x / numPoints;
            double freq = minFreq * std::pow(maxFreq / minFreq, proportion);
            double db = 0.0;
            if (lowPassEnabled)  { double ratio = freq / juce::jmax(1.0, lowShelfFreq);  db -= 12.0 / (1.0 + std::pow(ratio, 4.0)); }
            if (highPassEnabled) { double ratio = juce::jmax(1.0, highShelfFreq) / freq; db -= 12.0 / (1.0 + std::pow(ratio, 4.0)); }
            if (eqEnabled)
            {
                for (int b = 0; b < kNumEQBands; ++b)
                {
                    if (!eqBands[b].enabled || eqBands[b].freq <= 0.0) continue;
                    double ratio = freq / eqBands[b].freq;
                    if (ratio <= 0.0) continue;
                    double w = std::log(ratio) * std::log(ratio);
                    db += eqBands[b].gainDB * std::exp(-w * 3.5);
                }
            }
            double yProp = juce::jlimit(0.0, 1.0, juce::jmap(db, 24.0, -24.0, 0.0, 1.0));
            double py = curveBounds.getY() + yProp * curveBounds.getHeight();
            float px = (float)(curveBounds.getX() + x);
            if (x == 0) curve.startNewSubPath(px, (float)py);
            else        curve.lineTo(px, (float)py);
        }
        juce::Path fillPath = curve;
        fillPath.lineTo((float)curveBounds.getRight(), (float)curveBounds.getBottom());
        fillPath.lineTo((float)curveBounds.getX(),     (float)curveBounds.getBottom());
        fillPath.closeSubPath();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x332dd4e8), (float)curveBounds.getX(), (float)curveBounds.getY(),
                                               juce::Colour(0x002dd4e8), (float)curveBounds.getX(), (float)curveBounds.getBottom(), false));
        g.fillPath(fillPath);
        g.setColour(juce::Colour(0xff2dd4e8));
        g.strokePath(curve, juce::PathStrokeType(2.0f));
    }

    bounds.removeFromTop(14);

    // ── Timeline ────────────────────────────────────────────────────────────
    auto timelineOuter = bounds.removeFromTop(75);
    g.setColour(juce::Colour(DarkLookAndFeel::bg2));
    g.fillRoundedRectangle(timelineOuter.toFloat(), 6.0f);
    g.setColour(juce::Colour(DarkLookAndFeel::line2));
    g.drawRoundedRectangle(timelineOuter.toFloat(), 6.0f, 1.0f);

    double pos = transportSource.getCurrentPosition();
    double len = transportSource.getLengthInSeconds();

    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.setColour(juce::Colour(DarkLookAndFeel::txt2));
    g.drawText("TIMELINE", timelineOuter.getX() + 12, timelineOuter.getY() + 3, 80, 16, juce::Justification::centredLeft);

    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.setColour(juce::Colour(DarkLookAndFeel::txt0));
    if (len > 0.0)
    {
        int posS = (int)pos, posM = posS / 60; posS %= 60;
        int lenS = (int)len, lenM = lenS / 60; lenS %= 60;
        juce::String timeStr = juce::String::formatted("%d:%02d / %d:%02d", posM, posS, lenM, lenS);
        g.drawText(timeStr, timelineOuter.getX(), timelineOuter.getY() + 3, timelineOuter.getWidth() - 12, 16, juce::Justification::centredRight, false);
    }

    auto trackBar = timelineOuter.reduced(10, 0);
    trackBar.setY(timelineOuter.getY() + 22);
    trackBar.setHeight(timelineOuter.getBottom() - trackBar.getY() - 6);

    g.setColour(juce::Colour(DarkLookAndFeel::bg1));
    g.fillRoundedRectangle(trackBar.toFloat(), 4.0f);
    g.setColour(juce::Colour(DarkLookAndFeel::line1));
    g.drawRoundedRectangle(trackBar.toFloat(), 4.0f, 1.0f);

    if (len > 0.0)
    {
        double frac = juce::jlimit(0.0, 1.0, pos / len);
        int fillW = (int)(frac * (trackBar.getWidth() - 2));
        if (fillW > 0)
        {
            g.setColour(juce::Colour(0x334da6ff));
            g.fillRoundedRectangle((float)(trackBar.getX() + 1), (float)(trackBar.getY() + 1), (float)fillW, (float)(trackBar.getHeight() - 2), 3.0f);
        }

        int tickCount = juce::jlimit(4, 10, (int)(len / 5.0));
        g.setFont(juce::Font(10.0f));
        for (int t = 0; t <= tickCount; ++t)
        {
            double tickSec = (len / tickCount) * t;
            double tickFrac = tickSec / len;
            int tickX = trackBar.getX() + (int)(tickFrac * trackBar.getWidth());
            tickX = juce::jlimit(trackBar.getX(), trackBar.getRight(), tickX);
            bool isMajor = (t % 2 == 0);
            int tickH = isMajor ? 8 : 4;
            g.setColour(juce::Colour(isMajor ? DarkLookAndFeel::line2 : DarkLookAndFeel::line1));
            g.drawVerticalLine(tickX, (float)trackBar.getY(), (float)(trackBar.getY() + tickH));
            if (isMajor)
            {
                int ts = (int)tickSec, tm = ts / 60; ts %= 60;
                juce::String lbl = tm > 0 ? juce::String(tm) + ":" + juce::String::formatted("%02d", ts) : juce::String(ts) + "s";
                g.setColour(juce::Colour(DarkLookAndFeel::txt2));
                g.drawText(lbl, tickX - 16, trackBar.getY() + trackBar.getHeight() - 14, 32, 14, juce::Justification::centred, false);
            }
        }

        if (cropStart >= 0.0) {
            int cx = trackBar.getX() + (int)((cropStart / len) * trackBar.getWidth());
            g.setColour(juce::Colour(0xff4da6ff));
            g.fillRect(cx - 1, trackBar.getY(), 2, trackBar.getHeight());
        }
        if (cropEnd >= 0.0) {
            int cx = trackBar.getX() + (int)((cropEnd / len) * trackBar.getWidth());
            g.setColour(juce::Colour(0xff4da6ff));
            g.fillRect(cx - 1, trackBar.getY(), 2, trackBar.getHeight());
        }

        int headX = trackBar.getX() + (int)(frac * trackBar.getWidth());
        headX = juce::jlimit(trackBar.getX(), trackBar.getRight() - 1, headX);
        g.setColour(juce::Colours::white);
        g.fillRect(headX - 1, trackBar.getY(), 2, trackBar.getHeight());

        juce::Path diamond;
        diamond.addTriangle((float)headX, (float)(trackBar.getY() - 2),
                            (float)(headX - 5), (float)(trackBar.getY() + 6),
                            (float)(headX + 5), (float)(trackBar.getY() + 6));
        g.fillPath(diamond);
    }
    else
    {
        g.setFont(juce::Font(14.0f));
        g.setColour(juce::Colour(DarkLookAndFeel::txt2));
        g.drawText("No audio loaded", trackBar, juce::Justification::centred, false);
    }

    bounds.removeFromTop(14);

    // ── Effects Grid ───────────────────────────────────────────────────────
    const int boxH = 72, rowGap = 12, colGap = 16;
    const int colW = juce::jmax(10, (bounds.getWidth() - colGap) / 2);
    const int gridTotalH = 3 * boxH + 2 * rowGap;
    auto gridArea = bounds.removeFromTop(gridTotalH);

    struct EffectInfo { juce::String title, status; bool active, edit; juce::Colour accent; };
    std::array<EffectInfo, 6> effects = {
        EffectInfo{ "Gain",       juce::String(gain, 2) + "x" + (gainEditMode ? " | ↑↓" : " | G"),       std::abs(gain - 1.0f) > 0.01f, gainEditMode,       juce::Colour(DarkLookAndFeel::accentOrange) },
        EffectInfo{
            "Mid Fade",
            juce::String(midFadeEnabled ? "ON" : "off") +
            " | Pos: " + juce::String(midFadeCenter, 1) + "s" +
            " | Dur: " + juce::String(midFadeDuration, 1) + "s" +
            (midFadeEditMode ? " (Editing)" : ""),
            midFadeEnabled,
            midFadeEditMode,
            juce::Colour(DarkLookAndFeel::accentPink)
        },
        EffectInfo{ "Fade In",    juce::String(fadeInEnabled ? "ON" : "off") + "  " + juce::String(fadeInDuration, 1) + "s" + (fadeInEditMode ? " | ↑↓" : " | Ctrl+Q"), fadeInEnabled, fadeInEditMode,  juce::Colour(DarkLookAndFeel::accentYellow) },
        EffectInfo{ "Fade Out",   juce::String(fadeOutEnabled ? "ON" : "off") + "  " + juce::String(fadeOutDuration, 1) + "s" + (fadeOutEditMode ? " | ↑↓" : " | Ctrl+W"), fadeOutEnabled, fadeOutEditMode, juce::Colour(DarkLookAndFeel::accentGold) },
        EffectInfo{ "Low Shelf",  juce::String(lowPassEnabled ? "ON" : "off") + "  " + juce::String((int)lowShelfFreq) + "Hz" + (lowFreqEditMode ? " | ↑↓" : " | Ctrl+L"), lowPassEnabled, lowFreqEditMode,  juce::Colour(DarkLookAndFeel::accentBlue) },
        EffectInfo{ "High Shelf", juce::String(highPassEnabled ? "ON" : "off") + "  " + juce::String((int)highShelfFreq) + "Hz" + (highFreqEditMode ? " | ↑↓" : " | Ctrl+H"), highPassEnabled, highFreqEditMode, juce::Colour(DarkLookAndFeel::accentCyan) }
    };

    for (int row = 0; row < 3; ++row)
    {
        int rowY = gridArea.getY() + row * (boxH + rowGap);
        juce::Rectangle<int> leftBox (gridArea.getX(),          rowY, colW,                   boxH);
        juce::Rectangle<int> rightBox(gridArea.getX() + colW + colGap, rowY, gridArea.getWidth() - colW - colGap, boxH);
        int i0 = row * 2, i1 = row * 2 + 1;
        if (i0 < 6) drawEffectBox(g, leftBox,  effects[i0].title, effects[i0].status, effects[i0].active, effects[i0].edit, effects[i0].accent);
        if (i1 < 6) drawEffectBox(g, rightBox, effects[i1].title, effects[i1].status, effects[i1].active, effects[i1].edit, effects[i1].accent);
    }

    bounds.removeFromTop(14);

    // ── Crop Section ────────────────────────────────────────────────────────
    auto cropBounds = bounds.removeFromTop(70);
    bool hasCropPoints = (cropStart >= 0.0 || cropEnd >= 0.0);
    g.setColour(juce::Colour(DarkLookAndFeel::bg2));
    g.fillRoundedRectangle(cropBounds.toFloat(), 6.0f);
    if (hasCropPoints)
    {
        g.setColour(juce::Colour(DarkLookAndFeel::accentPurple).withAlpha(0.12f));
        g.fillRoundedRectangle(cropBounds.toFloat(), 6.0f);
        g.setColour(juce::Colour(DarkLookAndFeel::accentPurple).withAlpha(0.60f));
        g.drawRoundedRectangle(cropBounds.toFloat(), 6.0f, 1.5f);
    }
    else
    {
        g.setColour(juce::Colour(DarkLookAndFeel::line1));
        g.drawRoundedRectangle(cropBounds.toFloat(), 6.0f, 1.0f);
    }

    auto cropHeader = cropBounds.removeFromTop(28);
    g.setColour(juce::Colour(DarkLookAndFeel::line1));
    g.drawHorizontalLine(cropHeader.getBottom(), (float)cropBounds.getX() + 6, (float)cropBounds.getRight() - 6);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.setColour(hasCropPoints ? juce::Colour(DarkLookAndFeel::accentPurple) : juce::Colour(DarkLookAndFeel::txt1));
    g.drawText("Crop", cropHeader.getX() + 14, cropHeader.getY() + 4, 60, 20, juce::Justification::centredLeft);
    g.setFont(juce::Font(13.0f));
    g.setColour(juce::Colour(DarkLookAndFeel::txt2));
    g.drawText("I = In  |  O = Out  |  Shift+C = Apply", cropHeader.getX() + 76, cropHeader.getY() + 4, cropHeader.getWidth() - 84, 20, juce::Justification::centredLeft, false);

    auto cropDetail = cropBounds;
    juce::String inStr  = (cropStart >= 0.0) ? formatTime(cropStart) : "--";
    juce::String outStr = (cropEnd   >= 0.0) ? formatTime(cropEnd)   : "--";

    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.setColour(juce::Colour(DarkLookAndFeel::txt2));
    g.drawText("IN", cropDetail.getX() + 14, cropDetail.getY() + 6, 26, 18, juce::Justification::centredLeft);
    g.setFont(juce::Font(14.0f));
    g.setColour(cropStart >= 0.0 ? juce::Colour(DarkLookAndFeel::accentPurple) : juce::Colour(DarkLookAndFeel::txt2));
    g.drawText(inStr, cropDetail.getX() + 42, cropDetail.getY() + 6, cropDetail.getWidth() / 2 - 48, 18, juce::Justification::centredLeft);

    int halfW = cropDetail.getWidth() / 2;
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.setColour(juce::Colour(DarkLookAndFeel::txt2));
    g.drawText("OUT", cropDetail.getX() + halfW, cropDetail.getY() + 6, 32, 18, juce::Justification::centredLeft);
    g.setFont(juce::Font(14.0f));
    g.setColour(cropEnd >= 0.0 ? juce::Colour(DarkLookAndFeel::accentPurple) : juce::Colour(DarkLookAndFeel::txt2));
    g.drawText(outStr, cropDetail.getX() + halfW + 36, cropDetail.getY() + 6, halfW - 46, 18, juce::Justification::centredLeft);

    bounds.removeFromTop(14);

    // ── EQ Section ──────────────────────────────────────────────────────────
    auto eqBounds = bounds.removeFromTop(juce::jmin(bounds.getHeight() - 10, 260)); // Caps height cleanly
    eqSlidersArea = eqBounds;
    g.setColour(juce::Colour(DarkLookAndFeel::bg2));
    g.fillRoundedRectangle(eqBounds.toFloat(), 6.0f);
    g.setColour(juce::Colour(DarkLookAndFeel::line1));
    g.drawRoundedRectangle(eqBounds.toFloat(), 6.0f, 1.0f);

    auto eqHeader = eqBounds.removeFromTop(30);
    g.setColour(juce::Colour(DarkLookAndFeel::line2));
    g.drawHorizontalLine(eqHeader.getBottom(), (float)eqHeader.getX() + 4, (float)eqHeader.getRight() - 4);

    juce::String eqStat = juce::String(eqEnabled ? "ON" : "off");
    int active = 0; for (auto& b : eqBands) if (b.enabled) ++active;
    eqStat += "   " + juce::String(active) + " band(s) active";
    eqStat += eqEditMode ? "   Tab=band  ↑↓=gain" : "   Ctrl+E to edit";

    g.setFont(juce::Font(17.0f, juce::Font::bold));
    g.setColour(juce::Colour(DarkLookAndFeel::accentGreen));
    g.drawText("EQ", juce::Rectangle<int>(eqHeader.getX() + 12, eqHeader.getY() + 3, 40, 24), juce::Justification::centredLeft);
    g.setFont(juce::Font(14.0f));
    g.setColour(eqEnabled ? juce::Colour(DarkLookAndFeel::txt0) : juce::Colour(DarkLookAndFeel::txt1));
    g.drawText(eqStat, eqHeader.getX() + 52, eqHeader.getY() + 3, juce::jmax(0, eqHeader.getWidth() - 56), 22, juce::Justification::centredLeft, false);

    drawEQSliders(g, eqBounds.reduced(6, 6));
}

void MainComponent::resized()
{
    if (menuBar)
        menuBar->setBounds(0, 0, getWidth(), 25);
}

// Build IIR coefficients and push them into both channel filters.
// Low shelf  : -12 dB shelf at lowShelfFreq  (cuts low end when enabled)
// High shelf : -12 dB shelf at highShelfFreq (cuts high end when enabled)
// Gain factor < 1.0 = cut; adjust frequencies / dB to taste.
void MainComponent::updateFilterCoefficients()
{
    const double Q = 0.7;
    const float  shelfGain = 0.25f; // ~-12 dB
    auto lowCoeffs = juce::IIRCoefficients::makeLowShelf(currentSampleRate, lowShelfFreq, Q, shelfGain);
    auto highCoeffs = juce::IIRCoefficients::makeHighShelf(currentSampleRate, highShelfFreq, Q, shelfGain);
    for (int ch = 0; ch < 2; ++ch)
    {
        lowShelfFilter[ch].setCoefficients(lowCoeffs);
        highShelfFilter[ch].setCoefficients(highCoeffs);
        // Reset state to avoid a click when coefficients are first loaded
        lowShelfFilter[ch].reset();
        highShelfFilter[ch].reset();
    }
}

void MainComponent::updateEQCoefficients()
{
    // Use the file's sample rate if we have one loaded,
    // otherwise fall back to the device rate.
    // This matches what the export path does, fixing the live/export mismatch.
    double sr = (fileSampleRate > 0.0) ? fileSampleRate : currentSampleRate;
    if (sr <= 0.0) return;
    for (int b = 0; b < kNumEQBands; ++b)
    {
        float linearGain = juce::Decibels::decibelsToGain((float)eqBands[b].gainDB);
        auto coeffs = juce::IIRCoefficients::makePeakFilter(
            sr, eqBands[b].freq, eqBands[b].Q, linearGain);
        for (int ch = 0; ch < 2; ++ch)
        {
            eqFilter[b][ch].setCoefficients(coeffs);
            eqFilter[b][ch].reset();
        }
    }
}

void MainComponent::drawEQSliders(juce::Graphics& g, juce::Rectangle<int> area)
{
    static constexpr int kDbMax = 12;
    if (area.getWidth() < 80 || area.getHeight() < 100) return;

    const int labelTopH = 20;
    const int labelBotH = 18;
    const int axisW     = 32;
    const int trackPad  = 6;

    juce::Rectangle<int> trackArea(
        area.getX() + axisW,
        area.getY() + labelTopH,
        juce::jmax(0, area.getWidth() - axisW),
        juce::jmax(0, area.getHeight() - labelTopH - labelBotH));

    const int trackLeft = trackArea.getX();
    const int trackTop  = trackArea.getY();
    const int trackH    = trackArea.getHeight();
    const int bandW     = juce::jmax(1, trackArea.getWidth() / kNumEQBands);

    g.setColour(juce::Colour(DarkLookAndFeel::bg2));
    g.fillRect(area);

    const int gridDBs[] = { 12, 6, 0, -6, -12 };
    for (int db : gridDBs)
    {
        float frac = 0.5f - (float)db / (2.0f * kDbMax);
        int   lineY = trackTop + (int)(frac * trackH);
        g.setFont(juce::Font(10.0f));
        g.setColour(juce::Colour(DarkLookAndFeel::txt2));
        juce::String lbl = (db > 0 ? "+" : "") + juce::String(db);
        g.drawText(lbl, area.getX(), lineY - 8, axisW - 6, 16, juce::Justification::centredRight, false);
        g.setColour(db == 0 ? juce::Colour(DarkLookAndFeel::line2) : juce::Colour(DarkLookAndFeel::line1));
        g.drawHorizontalLine(lineY, (float)trackLeft, (float)trackArea.getRight());
    }

    g.setColour(juce::Colour(DarkLookAndFeel::line1));
    g.drawRect(trackArea, 1);

    for (int b = 0; b < kNumEQBands; ++b)
    {
        bool   sel = eqEditMode && (b == eqSelectedBand);
        bool   on  = eqBands[b].enabled;
        double db  = eqBands[b].gainDB;
        juce::Rectangle<int> col(trackLeft + b * bandW, trackTop, bandW, trackH);

        if (sel) { g.setColour(juce::Colour(0x221e6eff)); g.fillRect(col); }
        if (b < kNumEQBands - 1) { g.setColour(juce::Colour(DarkLookAndFeel::line1)); g.drawVerticalLine(col.getRight(), (float)trackTop, (float)(trackTop + trackH)); }

        int trackCX = col.getCentreX();
        g.setColour(juce::Colour(DarkLookAndFeel::line2));
        g.fillRect(trackCX - 1, col.getY() + 6, 2, col.getHeight() - 12);

        float thumbFrac = 0.5f - (float)(db / (2.0 * kDbMax));
        int   thumbCY   = col.getY() + (int)(thumbFrac * col.getHeight());
        const int thumbW = juce::jmax(1, bandW - trackPad * 2 - 4);
        const int thumbH = 12;
        juce::Rectangle<int> thumb(trackCX - thumbW / 2, thumbCY - thumbH / 2, thumbW, thumbH);

        juce::Colour thumbFace = !on ? juce::Colour(DarkLookAndFeel::bg3) : (sel ? juce::Colour(0xff1e4060) : juce::Colour(0xff283848));
        g.setColour(thumbFace);
        g.fillRoundedRectangle(thumb.toFloat(), 3.0f);
        g.setColour(sel ? juce::Colour(DarkLookAndFeel::accentGreen) : (on ? juce::Colour(0xff3a6080) : juce::Colour(DarkLookAndFeel::line2)));
        g.drawRoundedRectangle(thumb.toFloat(), 3.0f, 1.0f);

        if (on || sel)
        {
            g.setColour(sel ? juce::Colour(DarkLookAndFeel::accentGreen).withAlpha(0.8f) : juce::Colour(DarkLookAndFeel::txt2));
            for (int n = -1; n <= 1; ++n) g.fillRect(trackCX + n * 4, thumb.getCentreY() - 4, 1, 8);
        }

        g.setFont(juce::Font(10.5f, sel ? juce::Font::bold : juce::Font::plain));
        g.setColour(sel  ? juce::Colour(DarkLookAndFeel::txt0) : (on  ? juce::Colour(DarkLookAndFeel::txt1) : juce::Colour(DarkLookAndFeel::txt2)));
        juce::String dbStr = (db >= 0 ? "+" : "") + juce::String((int)std::round(db));
        g.drawText(dbStr, juce::Rectangle<int>(col.getX(), area.getY(), bandW, labelTopH), juce::Justification::centred);

        g.setFont(10.0f);
        g.setColour(sel  ? juce::Colour(DarkLookAndFeel::accentGreen) : (on  ? juce::Colour(DarkLookAndFeel::txt1) : juce::Colour(DarkLookAndFeel::txt2)));
        g.drawText(eqBands[b].name, juce::Rectangle<int>(col.getX(), area.getBottom() - labelBotH, bandW, labelBotH), juce::Justification::centred);
    }
}


void MainComponent::applyEQToBuffer(float* data, int numSamples, int channel)
{
    if (!eqEnabled) return;
    for (int b = 0; b < kNumEQBands; ++b)
        if (eqBands[b].enabled)
            eqFilter[b][channel].processSamples(data, numSamples);
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    updateFilterCoefficients();
    updateEQCoefficients();
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource == nullptr && memorySource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }
    transportSource.getNextAudioBlock(bufferToFill);
    if (bufferToFill.buffer == nullptr)
        return;
    auto* buf = bufferToFill.buffer;
    int startSamp = bufferToFill.startSample;
    int numSamps = bufferToFill.numSamples;
    buf->applyGain(startSamp, numSamps, gain);
    for (int ch = 0; ch < buf->getNumChannels() && ch < 2; ++ch)
    {
        float* data = buf->getWritePointer(ch, startSamp);
        if (lowPassEnabled)
            lowShelfFilter[ch].processSamples(data, numSamps);
        if (highPassEnabled)
            highShelfFilter[ch].processSamples(data, numSamps);
    }
    if (eqEnabled)
        for (int ch = 0; ch < buf->getNumChannels() && ch < 2; ++ch)
            applyEQToBuffer(buf->getWritePointer(ch, startSamp), numSamps, ch);

    // Apply fade in / fade out / mid-fade as per-sample linear ramps.
    // getCurrentPosition() returns the playhead AFTER the block was consumed,
    // so the block started one block-duration earlier.
    if ((fadeInEnabled || fadeOutEnabled || midFadeEnabled) && currentSampleRate > 0.0)
    {
        double totalLength = transportSource.getLengthInSeconds();
        double blockStartSec = transportSource.getCurrentPosition()
                               - (double)numSamps / currentSampleRate;
        for (int ch = 0; ch < buf->getNumChannels() && ch < 2; ++ch)
        {
            float* data = buf->getWritePointer(ch, startSamp);
            for (int s = 0; s < numSamps; ++s)
            {
                double pos = blockStartSec + (double)s / currentSampleRate;
                float  fadeGain = 1.0f;
                if (fadeInEnabled && fadeInDuration > 0.0)
                    fadeGain *= juce::jlimit(0.0f, 1.0f,
                                            (float)(pos / fadeInDuration));
                if (fadeOutEnabled && fadeOutDuration > 0.0)
                {
                    double fromEnd = totalLength - pos;
                    fadeGain *= juce::jlimit(0.0f, 1.0f,
                                            (float)(fromEnd / fadeOutDuration));
                }
                if (midFadeEnabled && midFadeDuration > 0.0)
                {
                    double halfDip  = midFadeDuration * 0.5;
                    double dipStart = midFadeCenter - halfDip;
                    double dipEnd   = midFadeCenter + halfDip;
                    if (pos >= dipStart && pos < midFadeCenter)
                    {
                        // Ramp down: 1 → 0
                        fadeGain *= juce::jlimit(0.0f, 1.0f,
                                                (float)((midFadeCenter - pos) / halfDip));
                    }
                    else if (pos >= midFadeCenter && pos <= dipEnd)
                    {
                        // Ramp up: 0 → 1
                        fadeGain *= juce::jlimit(0.0f, 1.0f,
                                                (float)((pos - midFadeCenter) / halfDip));
                    }
                }
                data[s] *= fadeGain;
            }
        }
    }
}

void MainComponent::releaseResources()
{
    transportSource.releaseResources();
}

//==============================================================================
// Static helper  used in paint() and announcements
juce::String MainComponent::formatTime(double seconds)
{
    int mins = (int)(seconds / 60);
    int secs = (int)(seconds) % 60;
    return juce::String(mins) + " minutes " + juce::String(secs) + " seconds";
}

//==============================================================================
void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (eqSlidersArea.contains(e.getPosition()))
    {
        int axisW = 28;
        int trackLeft = eqSlidersArea.getX() + axisW;
        int trackAreaWidth = eqSlidersArea.getWidth() - axisW;
        int bandW = trackAreaWidth / kNumEQBands;
        int x = e.getPosition().getX();
        int b = (x - trackLeft) / bandW;
        if (b >= 0 && b < kNumEQBands)
        {
            eqSelectedBand = b;
            eqEditMode = true; // Auto select & enter edit mode
            gainEditMode = fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = false;
            mouseDrag(e); // apply initial gain change
            juce::AccessibilityHandler::postAnnouncement(
                "EQ band " + eqBands[b].name + " selected.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
        }
    }
}

void MainComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (eqSlidersArea.contains(e.getPosition()) || eqEditMode)
    {
        int labelTopH = 18;
        int labelBotH = 16;
        int trackTop = eqSlidersArea.getY() + labelTopH;
        int trackH = eqSlidersArea.getHeight() - labelTopH - labelBotH;
        int y = e.getPosition().getY();
        y = juce::jlimit(trackTop, trackTop + trackH, y);
        float frac = (float)(y - trackTop) / (float)trackH;
        double db = (0.5 - frac) * 24.0;
        if (eqSelectedBand >= 0 && eqSelectedBand < kNumEQBands)
        {
            eqBands[eqSelectedBand].gainDB = juce::jlimit(-12.0, 12.0, db);
            eqBands[eqSelectedBand].enabled = true; // auto enable when touched
            updateEQCoefficients();
            repaint();
        }
    }
}

//==============================================================================
bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    char c = (char)std::tolower(key.getTextCharacter());
    bool ctrlDown = key.getModifiers().isCtrlDown();
    bool altDown = key.getModifiers().isAltDown();
    int kc = key.getKeyCode();
    bool isCommand = false;
    if (kc == 'g' || kc == 'G' || kc == 'e' || kc == 'E' ||
        kc == 'q' || kc == 'Q' || kc == 'w' || kc == 'W' ||
        kc == 'l' || kc == 'L' || kc == 'h' || kc == 'H' ||
        kc == 'm' || kc == 'M' || kc == 's' || kc == 'S')
    {
        isCommand = true;
    }
    if (isCommand && readerSource == nullptr && memorySource == nullptr && transportSource.getLengthInSeconds() <= 0.0)
    {
        juce::AccessibilityHandler::postAnnouncement(
            "No audio file loaded. Press Ctrl + O to open a .wav file.",
            juce::AccessibilityHandler::AnnouncementPriority::high);
        return true;
    }

    // Digit scrub
    if (readerSource != nullptr || transportSource.getLengthInSeconds() > 0.0)
    {
        if (c >= '0' && c <= '9')
        {
            transportSource.setPosition((c - '0') / 10.0 * transportSource.getLengthInSeconds());
            return true;
        }
        else if (c == 'g')
        {
            bool anyOther = fadeInEditMode || fadeOutEditMode ||
                            lowFreqEditMode || highFreqEditMode || eqEditMode;
            if (anyOther)
            {
                fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
                gainEditMode = true;
            }
            else
            {
                gainEditMode = !gainEditMode;
            }
            saveUndoState("Gain edit mode " + juce::String(gainEditMode ? "enabled" : "disabled"));
            logEffect("Gain edit mode " + juce::String(gainEditMode ? "enabled" : "disabled"));
            juce::AccessibilityHandler::postAnnouncement(
                gainEditMode ? "Gain edit mode enabled. Use up and down arrow keys to edit gain." : "Gain edit mode disabled.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        else if (c == 't')
        {
            announceTime();
            return true;
        }
        else if (c == 's' && !ctrlDown && !altDown)
        {
            announceEffectsStatus();
            return true;
        }
        else if (c == 'e' && !ctrlDown && !altDown)
        {
            eqEnabled = !eqEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                eqEnabled ? "EQ on. Press Ctrl + E to enter EQ band editing mode." : "EQ off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        else if (c == 'i')
        {
            cropStart = transportSource.getCurrentPosition();
            juce::String msg = "Crop start set at " + formatTime(cropStart);
            juce::AccessibilityHandler::postAnnouncement(msg, juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        else if (c == 'o' && !ctrlDown)
        {
            cropEnd = transportSource.getCurrentPosition();
            juce::String msg = "Crop end set at " + formatTime(cropEnd);
            juce::AccessibilityHandler::postAnnouncement(msg, juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        else if (c == 'c' && key.getModifiers().isShiftDown())
        {
            applyCrop();
            return true;
        }
        if (c == 'l')
        {
            saveUndoState("Low shelf filter " + juce::String(!lowPassEnabled ? "on" : "off"));
            logEffect("Low shelf filter " + juce::String(!lowPassEnabled ? "on" : "off"));
            lowPassEnabled = !lowPassEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                lowPassEnabled
                ? "Low shelf filter on. " + juce::String((int)lowShelfFreq) + " Hz. Press Ctrl + L to edit frequencies."
                : "Low shelf filter off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            lowShelfFilter[0].reset();
            lowShelfFilter[1].reset();
            repaint();
            return true;
        }
        else if (c == 'h' && !altDown)
        {
            saveUndoState("High shelf filter " + juce::String(!highPassEnabled ? "on" : "off"));
            logEffect("High shelf filter " + juce::String(!highPassEnabled ? "on" : "off"));
            highPassEnabled = !highPassEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                highPassEnabled
                ? "High shelf filter on. " + juce::String((int)highShelfFreq) + " Hz. Press Ctrl + H to edit frequencies."
                : "High shelf filter off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            highShelfFilter[0].reset();
            highShelfFilter[1].reset();
            repaint();
            return true;
        }
        else if (c == 'q')
        {
            saveUndoState("Fade in " + juce::String(!fadeInEnabled ? "on" : "off"));
            logEffect("Fade in " + juce::String(!fadeInEnabled ? "on" : "off"));
            fadeInEnabled = !fadeInEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                fadeInEnabled
                ? "Fade in on. " + juce::String(fadeInDuration, 1) + " seconds. Press Ctrl + Q to edit duration."
                : "Fade in off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        else if (c == 'w')
        {
            saveUndoState("Fade out " + juce::String(!fadeOutEnabled ? "on" : "off"));
            logEffect("Fade out " + juce::String(!fadeOutEnabled ? "on" : "off"));
            fadeOutEnabled = !fadeOutEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                fadeOutEnabled
                ? "Fade out on. " + juce::String(fadeOutDuration, 1) + " seconds. Press Ctrl + W to edit duration."
                : "Fade out off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        else if (c == 'm' && !ctrlDown)
        {
            double pos = transportSource.getCurrentPosition();
            if (!midFadeEnabled)
            {
                midFadeCenter  = pos;
                midFadeEnabled = true;
                saveUndoState("Mid-fade on at " + formatTime(midFadeCenter));
                logEffect("Mid-fade on at " + formatTime(midFadeCenter));
                juce::AccessibilityHandler::postAnnouncement(
                    "Mid-fade enabled at " + formatTime(midFadeCenter) + ". Press Ctrl + M to edit.",
                    juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            else
            {
                midFadeEnabled  = false;
                midFadeEditMode = false;
                saveUndoState("Mid-fade off");
                logEffect("Mid-fade off");
                juce::AccessibilityHandler::postAnnouncement("Mid-fade off.", juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            repaint();
            return true;
        }
    }

    // Space toggles playback
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlayback();
        return true;
    }

    double scrubAmount = ctrlDown ? 10.0 : 2;
    double currentPos = transportSource.getCurrentPosition();
    double length = transportSource.getLengthInSeconds();

    if (ctrlDown)
    {
        if (key.getKeyCode() == 'z' || key.getKeyCode() == 'Z')
        {
            performUndo();
            return true;
        }
        if (key.getKeyCode() == 'y' || key.getKeyCode() == 'Y')
        {
            performRedo();
            return true;
        }
        if (key.getKeyCode() == 'o' || key.getKeyCode() == 'O') {
            importFile();
        }
        if (key.getKeyCode() == 's' || key.getKeyCode() == 'S') {
            exportModifiedFile();
        }
        // Ctrl+L: enter/exit low shelf frequency edit mode
        if (key.getKeyCode() == 'l' || key.getKeyCode() == 'L')
        {
            bool anyOther = gainEditMode || fadeInEditMode || fadeOutEditMode ||
                            highFreqEditMode || eqEditMode;
            bool targetState = anyOther ? true : !lowFreqEditMode;
            if (targetState && !lowPassEnabled)
            {
                juce::AccessibilityHandler::postAnnouncement("Low shelf filter is off. Press L to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
                return true;
            }
            gainEditMode = fadeInEditMode = fadeOutEditMode = highFreqEditMode = eqEditMode = false;
            lowFreqEditMode = targetState;
            juce::AccessibilityHandler::postAnnouncement(
                lowFreqEditMode ? "Low shelf frequency edit mode. Use up and down arrows to change frequency. Currently " + juce::String((int)lowShelfFreq) + " Hz." : "Low shelf frequency edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == 'e' || key.getKeyCode() == 'E')
        {
            bool anyOther = gainEditMode || fadeInEditMode || fadeOutEditMode ||
                            lowFreqEditMode || highFreqEditMode;
            bool targetState = anyOther ? true : !eqEditMode;
            if (targetState && !eqEnabled)
            {
                juce::AccessibilityHandler::postAnnouncement("EQ is off. Press E to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
                return true;
            }
            gainEditMode = fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = false;
            eqEditMode = targetState;
            juce::AccessibilityHandler::postAnnouncement(
                eqEditMode ? "EQ edit mode. Tab to select band. Up and down arrows to adjust gain." : "EQ edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // Ctrl+H: enter/exit high shelf frequency edit mode
        if (key.getKeyCode() == 'h' || key.getKeyCode() == 'H')
        {
            bool anyOther = gainEditMode || fadeInEditMode || fadeOutEditMode ||
                            lowFreqEditMode || eqEditMode;
            bool targetState = anyOther ? true : !highFreqEditMode;
            if (targetState && !highPassEnabled)
            {
                juce::AccessibilityHandler::postAnnouncement("High shelf filter is off. Press H to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
                return true;
            }
            gainEditMode = lowFreqEditMode = fadeInEditMode = fadeOutEditMode = eqEditMode = false;
            highFreqEditMode = targetState;
            juce::AccessibilityHandler::postAnnouncement(
                highFreqEditMode ? "High shelf frequency edit mode. Use up and down arrows to change frequency. Currently " + juce::String((int)highShelfFreq) + " Hz." : "High shelf frequency edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // Ctrl+Q: enter/exit fade-in duration edit mode
        if (key.getKeyCode() == 'q' || key.getKeyCode() == 'Q')
        {
            bool anyOther = gainEditMode || fadeOutEditMode ||
                            lowFreqEditMode || highFreqEditMode || eqEditMode;
            bool targetState = anyOther ? true : !fadeInEditMode;
            if (targetState && !fadeInEnabled)
            {
                juce::AccessibilityHandler::postAnnouncement("Fade in is off. Press Q to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
                return true;
            }
            gainEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
            fadeInEditMode = targetState;
            juce::AccessibilityHandler::postAnnouncement(
                fadeInEditMode ? "Fade in duration edit mode. Use up and down arrows to change duration. Currently " + juce::String(fadeInDuration, 1) + " seconds." : "Fade in duration edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // Ctrl+W: enter/exit fade-out duration edit mode
        if (key.getKeyCode() == 'w' || key.getKeyCode() == 'W')
        {
            bool anyOther = gainEditMode || fadeInEditMode ||
                            lowFreqEditMode || highFreqEditMode || eqEditMode;
            bool targetState = anyOther ? true : !fadeOutEditMode;
            if (targetState && !fadeOutEnabled)
            {
                juce::AccessibilityHandler::postAnnouncement("Fade out is off. Press W to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
                return true;
            }
            gainEditMode = fadeInEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
            fadeOutEditMode = targetState;
            juce::AccessibilityHandler::postAnnouncement(
                fadeOutEditMode ? "Fade out duration edit mode. Use up and down arrows to change duration. Currently " + juce::String(fadeOutDuration, 1) + " seconds." : "Fade out duration edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // Ctrl+M: enter/exit mid-fade edit mode
        if (key.getKeyCode() == 'm' || key.getKeyCode() == 'M')
        {
            if (!midFadeEditMode && !midFadeEnabled)
            {
                juce::AccessibilityHandler::postAnnouncement("Mid-fade is off. Press M at the desired position to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
                return true;
            }
            gainEditMode = fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
            midFadeEditMode = !midFadeEditMode;
            juce::AccessibilityHandler::postAnnouncement(
                midFadeEditMode
                ? "Mid-fade edit mode. Shift + Left and Right arrows adjust position by 0.1 seconds. Ctrl + Shift + Left and Right arrows adjust position by 1 second. Up and Down arrows change duration. Centre at " + formatTime(midFadeCenter) + ", duration " + juce::String(midFadeDuration, 1) + " seconds."
                : "Mid-fade edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::homeKey)
        {
            transportSource.setPosition(0.0);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::endKey)
        {
            double length = transportSource.getLengthInSeconds();
            transportSource.setPosition(length);
            juce::AccessibilityHandler::postAnnouncement("Moved to end, " + formatTime(length), juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
    }

    // Left/Right scrubbing (ignores Shift to allow Shift+Arrow for mid-fade position)
    if (!key.getModifiers().isShiftDown())
    {
        if (key.getKeyCode() == juce::KeyPress::rightKey)
        {
            double newPos = juce::jlimit(0.0, length, currentPos + scrubAmount);
            transportSource.setPosition(newPos);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::leftKey)
        {
            double newPos = juce::jlimit(0.0, length, currentPos - scrubAmount);
            transportSource.setPosition(newPos);
            repaint();
            return true;
        }
    }

    if (eqEditMode)
    {
        if (key.getKeyCode() == juce::KeyPress::tabKey)
        {
            bool shift = key.getModifiers().isShiftDown();
            eqSelectedBand = (eqSelectedBand + (shift ? kNumEQBands - 1 : 1)) % kNumEQBands;
            juce::String msg;
            msg << "Band " << (eqSelectedBand + 1) << ": " << eqBands[eqSelectedBand].name
                << ", " << (eqBands[eqSelectedBand].gainDB >= 0 ? "+" : "")
                << juce::String(eqBands[eqSelectedBand].gainDB, 1) << " dB"
                << (eqBands[eqSelectedBand].enabled ? ", on." : ", off.");
            juce::AccessibilityHandler::postAnnouncement(msg, juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key == juce::KeyPress::spaceKey)
        {
            eqBands[eqSelectedBand].enabled = !eqBands[eqSelectedBand].enabled;
            updateEQCoefficients();
            juce::AccessibilityHandler::postAnnouncement(eqBands[eqSelectedBand].name + (eqBands[eqSelectedBand].enabled ? " on." : " off."), juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey)
        {
            bool   up = (key.getKeyCode() == juce::KeyPress::upKey);
            double step = key.getModifiers().isShiftDown() ? 3.0 : 1.0;
            double& db = eqBands[eqSelectedBand].gainDB;
            db = juce::jlimit(-12.0, 12.0, db + (up ? step : -step));
            eqBands[eqSelectedBand].enabled = true;
            updateEQCoefficients();
            juce::String msg;
            msg << eqBands[eqSelectedBand].name << " " << (db >= 0 ? "+" : "") << juce::String(db, 1) << " dB.";
            juce::AccessibilityHandler::postAnnouncement(msg, juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
    }

    // ── Mid-Fade Edit Mode ─────────────────────────────────────────────────
    if (midFadeEditMode)
    {
        double totalLength = transportSource.getLengthInSeconds();
        bool shiftDown = key.getModifiers().isShiftDown();

        // Up/Down for Duration
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            midFadeDuration = juce::jlimit(0.1, 30.0, midFadeDuration + 0.1);
            juce::AccessibilityHandler::postAnnouncement("Mid-fade duration " + juce::String(midFadeDuration, 1) + " seconds.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            midFadeDuration = juce::jlimit(0.1, 30.0, midFadeDuration - 0.1);
            juce::AccessibilityHandler::postAnnouncement("Mid-fade duration " + juce::String(midFadeDuration, 1) + " seconds.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // Shift + Left/Right for Position
        if (shiftDown && (key.getKeyCode() == juce::KeyPress::rightKey || key.getKeyCode() == juce::KeyPress::leftKey))
        {
            bool isRight = (key.getKeyCode() == juce::KeyPress::rightKey);
            double step = ctrlDown ? 1.0 : 0.1;
            midFadeCenter = juce::jlimit(0.0, juce::jmax(0.0, totalLength),
                                         midFadeCenter + (isRight ? step : -step));
            
            saveUndoState("Mid-fade position " + juce::String(midFadeCenter, 1) + "s");

            juce::String msg = "Mid-fade centre " + juce::String(midFadeCenter, 1) + " seconds. ";
        
            juce::AccessibilityHandler::postAnnouncement(msg, juce::AccessibilityHandler::AnnouncementPriority::high);

            repaint();
            return true;
        }
    }

    // Up/Down arrows: edit modes in priority order — fade in, fade out, filter freqs, gain
    if (fadeInEditMode)
    {
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            fadeInDuration = juce::jlimit(0.1, 30.0, fadeInDuration + 0.1);
            juce::AccessibilityHandler::postAnnouncement("Fade in " + juce::String(fadeInDuration, 1) + " seconds.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            fadeInDuration = juce::jlimit(0.1, 30.0, fadeInDuration - 0.1);
            juce::AccessibilityHandler::postAnnouncement("Fade in " + juce::String(fadeInDuration, 1) + " seconds.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
    }
    else if (fadeOutEditMode)
    {
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            fadeOutDuration = juce::jlimit(0.1, 30.0, fadeOutDuration + 0.1);
            juce::AccessibilityHandler::postAnnouncement("Fade out " + juce::String(fadeOutDuration, 1) + " seconds.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            fadeOutDuration = juce::jlimit(0.1, 30.0, fadeOutDuration - 0.1);
            juce::AccessibilityHandler::postAnnouncement("Fade out " + juce::String(fadeOutDuration, 1) + " seconds.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
    }
    else if (lowFreqEditMode)
    {
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            lowShelfFreq = juce::jlimit(0.0, 2000.0, lowShelfFreq + 50.0);
            updateFilterCoefficients();
            juce::AccessibilityHandler::postAnnouncement("Low shelf " + juce::String((int)lowShelfFreq) + " Hz.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            lowShelfFreq = juce::jlimit(0.0, 2000.0, lowShelfFreq - 50.0);
            updateFilterCoefficients();
            juce::AccessibilityHandler::postAnnouncement("Low shelf " + juce::String((int)lowShelfFreq) + " Hz.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
    }
    else if (highFreqEditMode)
    {
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            highShelfFreq = juce::jlimit(1000.0, 20000.0, highShelfFreq + 200.0);
            updateFilterCoefficients();
            juce::AccessibilityHandler::postAnnouncement("High shelf " + juce::String((int)highShelfFreq) + " Hz.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            highShelfFreq = juce::jlimit(1000.0, 20000.0, highShelfFreq - 200.0);
            updateFilterCoefficients();
            juce::AccessibilityHandler::postAnnouncement("High shelf " + juce::String((int)highShelfFreq) + " Hz.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
    }
    else if (gainEditMode)
    {
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            saveUndoState("Gain increased to " + juce::String(gain + gainStep, 2));
            logEffect("Gain increased to " + juce::String(gain + gainStep, 2));
            gain = juce::jlimit(0.0f, 4.0f, gain + gainStep);
            juce::AccessibilityHandler::postAnnouncement("Gain set to " + juce::String(gain, 1), juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            saveUndoState("Gain decreased to " + juce::String(gain - gainStep, 2));
            logEffect("Gain decreased to " + juce::String(gain - gainStep, 2));
            gain = juce::jlimit(0.0f, 4.0f, gain - gainStep);
            juce::AccessibilityHandler::postAnnouncement("Gain set to " + juce::String(gain, 1), juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
    }

    // Alt+F / Alt+P opens menus
    if (altDown)
    {
        if (c == 'f')
        {
            if (menuBar) menuBar->openMenuByIndex(0);
            return true;
        }
        if (c == 'p')
        {
            if (menuBar) menuBar->openMenuByIndex(1);
            return true;
        }
        if (c == 'e')
        {
            if (menuBar) menuBar->openMenuByIndex(2);
            return true;
        }
        if (c == 'h' && !key.getModifiers().isCtrlDown())
        {
            if (menuBar) menuBar->openMenuByIndex(3);
            return true;
        }
    }
    return false;
}

void MainComponent::announceTime()
{
    double currentPos = transportSource.getCurrentPosition();
    juce::String msg = formatTime(currentPos);
    juce::AccessibilityHandler::postAnnouncement(msg, juce::AccessibilityHandler::AnnouncementPriority::high);
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Playback", "Edit", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex, const juce::String&)
{
    juce::PopupMenu menu;
    bool hasAudio = (readerSource != nullptr || memorySource != nullptr);

    if (menuIndex == 0) // File
    {
        menu.addItem(1, "Open WAV                    Ctrl+O");
        menu.addSeparator();
        menu.addItem(2, "Save Modified WAV           Ctrl+S", hasAudio);
        menu.addSeparator();
        menu.addItem(3, "Quit");
    }
    else if (menuIndex == 1) // Playback
    {
        menu.addItem(4,  transportSource.isPlaying()
                             ? "Stop                              Space"
                             : "Play                              Space", hasAudio);
        menu.addSeparator();
        menu.addItem(11, "Jump to Start         Ctrl+Home / 0", hasAudio);
        menu.addItem(12, "Jump to End               Ctrl+End", hasAudio);
        menu.addSeparator();
        menu.addItem(13, "Scrub Back 2s                    <--", hasAudio);
        menu.addItem(14, "Scrub Forward 2s                 -->", hasAudio);
        menu.addItem(15, "Scrub Back 10s           Ctrl+<--", hasAudio);
        menu.addItem(16, "Scrub Forward 10s        Ctrl+-->", hasAudio);
        menu.addSeparator();
        menu.addItem(17, "Announce Current Time            T", hasAudio);
    }
    else if (menuIndex == 2) // Edit
    {
        // Volume
        menu.addItem(5,  "Toggle Gain Edit Mode            G", hasAudio, gainEditMode);
        menu.addSeparator();
        // Fades
        menu.addItem(30, "Toggle Fade In                   Q", hasAudio, fadeInEnabled);
        menu.addItem(31, "Edit Fade In Duration       Ctrl+Q", hasAudio && fadeInEnabled, fadeInEditMode);
        menu.addItem(32, "Toggle Fade Out                  W", hasAudio, fadeOutEnabled);
        menu.addItem(33, "Edit Fade Out Duration      Ctrl+W", hasAudio && fadeOutEnabled, fadeOutEditMode);
        menu.addItem(34, "Toggle Mid-Fade at Playhead      M", hasAudio, midFadeEnabled);
        menu.addItem(35, "Edit Mid-Fade Position/Duration  Ctrl+M", hasAudio && midFadeEnabled, midFadeEditMode);
        menu.addSeparator();
        // Filters
        menu.addItem(36, "Toggle Low Shelf                 L", hasAudio, lowPassEnabled);
        menu.addItem(37, "Edit Low Shelf Frequency    Ctrl+L", hasAudio && lowPassEnabled, lowFreqEditMode);
        menu.addItem(38, "Toggle High Shelf                H", hasAudio, highPassEnabled);
        menu.addItem(39, "Edit High Shelf Frequency   Ctrl+H", hasAudio && highPassEnabled, highFreqEditMode);
        menu.addSeparator();
        // EQ
        menu.addItem(40, "Toggle EQ                        E", hasAudio, eqEnabled);
        menu.addItem(41, "Edit EQ Bands               Ctrl+E", hasAudio && eqEnabled, eqEditMode);
        menu.addSeparator();
        // Crop
        {
            juce::String inLabel  = cropStart >= 0.0
                ? "Set Crop In Point (now " + formatTime(cropStart) + ")   I"
                : "Set Crop In Point...             I";
            juce::String outLabel = cropEnd >= 0.0
                ? "Set Crop Out Point (now " + formatTime(cropEnd) + ")  O"
                : "Set Crop Out Point...            O";
            menu.addItem(6, inLabel,  hasAudio);
            menu.addItem(7, outLabel, hasAudio);
        }
        menu.addItem(8,  "Apply Crop               Shift+C", cropStart >= 0.0 && cropEnd >= 0.0);
        menu.addSeparator();
        // History
        menu.addItem(9,  "Undo                        Ctrl+Z", !undoStack.empty());
        menu.addItem(10, "Redo                        Ctrl+Y", !redoStack.empty());
    }
    else if (menuIndex == 3) // Help  (IDs 200+ — no overlap with functional menus)
    {
        menu.addItem(200, "Getting Started (Click for Overview)");
        menu.addSeparator();
        juce::PopupMenu menuShortcuts;
        menuShortcuts.addItem(201, "Open File Menu: Alt+F");
        menuShortcuts.addItem(202, "Open Playback Menu: Alt+P");
        menuShortcuts.addItem(203, "Open Edit Menu: Alt+E");
        menuShortcuts.addItem(204, "Open Help Menu: Alt+H");
        menuShortcuts.addItem(205, "Open WAV File: Ctrl+O");
        menuShortcuts.addItem(206, "Save Modified WAV: Ctrl+S");
        menu.addSubMenu("Menu Shortcuts", menuShortcuts);
        juce::PopupMenu playbackShortcuts;
        playbackShortcuts.addItem(210, "Play or Pause: Space");
        playbackShortcuts.addItem(211, "Announce current time: T");
        playbackShortcuts.addItem(212, "Move playhead by 2 seconds: Left or Right Arrow");
        playbackShortcuts.addItem(213, "Move playhead by 10 seconds: Ctrl + Left or Right Arrow");
        playbackShortcuts.addItem(214, "Jump to percentage of track: Number keys 1 to 9");
        playbackShortcuts.addItem(215, "Move playhead to start: Ctrl+Home or 0");
        playbackShortcuts.addItem(216, "Move playhead to end: Ctrl+End");
        menu.addSubMenu("Playback Controls", playbackShortcuts);
        juce::PopupMenu effectsShortcuts;
        effectsShortcuts.addItem(220, "Announce Effects Status: S");
        effectsShortcuts.addItem(221, "Toggle Gain Edit Mode: G");
        effectsShortcuts.addItem(222, "Adjust Gain: Up / Down Arrow");
        effectsShortcuts.addItem(223, "Toggle Low Shelf: L");
        effectsShortcuts.addItem(224, "Edit Low Shelf Frequency: Ctrl+L");
        effectsShortcuts.addItem(225, "Toggle High Shelf: H");
        effectsShortcuts.addItem(226, "Edit High Shelf Frequency: Ctrl+H");
        effectsShortcuts.addItem(227, "Toggle Fade In: Q");
        effectsShortcuts.addItem(228, "Edit Fade In Duration: Ctrl+Q");
        effectsShortcuts.addItem(229, "Toggle Fade Out: W");
        effectsShortcuts.addItem(230, "Edit Fade Out Duration: Ctrl+W");
        effectsShortcuts.addItem(231, "Toggle Mid-Fade at Playhead: M");
        effectsShortcuts.addItem(232, "Edit Mid-Fade: Ctrl+M");
        effectsShortcuts.addItem(233, "Adjust Mid-Fade Position 0.1s: Shift + Left / Right");
        effectsShortcuts.addItem(234, "Adjust Mid-Fade Position 1s: Ctrl+Shift + Left / Right");
        effectsShortcuts.addItem(235, "Adjust Mid-Fade Duration: Up / Down Arrow");
        effectsShortcuts.addItem(236, "Set Crop In Point: I");
        effectsShortcuts.addItem(237, "Set Crop Out Point: O");
        effectsShortcuts.addItem(238, "Apply Crop: Shift+C");
        effectsShortcuts.addItem(239, "Toggle EQ: E");
        effectsShortcuts.addItem(240, "Edit EQ Bands: Ctrl+E");
        effectsShortcuts.addItem(241, "Undo: Ctrl+Z");
        effectsShortcuts.addItem(242, "Redo: Ctrl+Y");
        menu.addSubMenu("Effects and Shortcuts", effectsShortcuts);
        menu.addSeparator();
        juce::PopupMenu historyMenu;
        if (effectHistory.isEmpty())
            historyMenu.addItem(250, "No effects applied yet", false);
        else
            for (int i = 0; i < effectHistory.size(); ++i)
                historyMenu.addItem(300 + i, effectHistory[i], false);
        menu.addSubMenu("Effect History", historyMenu);
    }
    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int)
{
    switch (menuItemID)
    {
        // ── File ──────────────────────────────────────────────────────────────
        case 1: importFile(); break;
        case 2: exportModifiedFile(); break;
        case 3: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;

        // ── Playback ──────────────────────────────────────────────────────────
        case 4: togglePlayback(); break;
        case 11:
            transportSource.setPosition(0.0);
            juce::AccessibilityHandler::postAnnouncement("Moved to start.", juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        case 12:
        {
            double len = transportSource.getLengthInSeconds();
            transportSource.setPosition(len);
            juce::AccessibilityHandler::postAnnouncement("Moved to end, " + formatTime(len), juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 13:
        {
            double p = juce::jlimit(0.0, transportSource.getLengthInSeconds(), transportSource.getCurrentPosition() - 2.0);
            transportSource.setPosition(p);
            juce::AccessibilityHandler::postAnnouncement(formatTime(p), juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 14:
        {
            double p = juce::jlimit(0.0, transportSource.getLengthInSeconds(), transportSource.getCurrentPosition() + 2.0);
            transportSource.setPosition(p);
            juce::AccessibilityHandler::postAnnouncement(formatTime(p), juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 15:
        {
            double p = juce::jlimit(0.0, transportSource.getLengthInSeconds(), transportSource.getCurrentPosition() - 10.0);
            transportSource.setPosition(p);
            juce::AccessibilityHandler::postAnnouncement(formatTime(p), juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 16:
        {
            double p = juce::jlimit(0.0, transportSource.getLengthInSeconds(), transportSource.getCurrentPosition() + 10.0);
            transportSource.setPosition(p);
            juce::AccessibilityHandler::postAnnouncement(formatTime(p), juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 17: announceTime(); break;

        // ── Edit: Gain ────────────────────────────────────────────────────────
        case 5:
        {
            bool anyOther = fadeInEditMode || fadeOutEditMode || lowFreqEditMode || highFreqEditMode || eqEditMode;
            if (anyOther)
            {
                fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
                gainEditMode = true;
            }
            else
                gainEditMode = !gainEditMode;
            juce::AccessibilityHandler::postAnnouncement(
                gainEditMode ? "Gain edit mode enabled. Use up and down arrow keys to adjust gain."
                             : "Gain edit mode disabled.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }

        // ── Edit: Fades ───────────────────────────────────────────────────────
        case 30:
        {
            saveUndoState("Fade in " + juce::String(!fadeInEnabled ? "on" : "off"));
            logEffect   ("Fade in " + juce::String(!fadeInEnabled ? "on" : "off"));
            fadeInEnabled = !fadeInEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                fadeInEnabled ? "Fade in on. " + juce::String(fadeInDuration, 1) + " seconds. Select Edit Fade In Duration to adjust."
                              : "Fade in off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 31:
        {
            gainEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
            fadeInEditMode = !fadeInEditMode;
            juce::AccessibilityHandler::postAnnouncement(
                fadeInEditMode ? "Fade in duration edit mode. Use up and down arrow keys. Currently " + juce::String(fadeInDuration, 1) + " seconds."
                               : "Fade in edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 32:
        {
            saveUndoState("Fade out " + juce::String(!fadeOutEnabled ? "on" : "off"));
            logEffect   ("Fade out " + juce::String(!fadeOutEnabled ? "on" : "off"));
            fadeOutEnabled = !fadeOutEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                fadeOutEnabled ? "Fade out on. " + juce::String(fadeOutDuration, 1) + " seconds. Select Edit Fade Out Duration to adjust."
                               : "Fade out off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 33:
        {
            gainEditMode = fadeInEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
            fadeOutEditMode = !fadeOutEditMode;
            juce::AccessibilityHandler::postAnnouncement(
                fadeOutEditMode ? "Fade out duration edit mode. Use up and down arrow keys. Currently " + juce::String(fadeOutDuration, 1) + " seconds."
                                : "Fade out edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 34:
        {
            double pos = transportSource.getCurrentPosition();
            if (!midFadeEnabled)
            {
                midFadeCenter = pos;  midFadeEnabled = true;
                saveUndoState("Mid-fade on at " + formatTime(midFadeCenter));
                logEffect    ("Mid-fade on at " + formatTime(midFadeCenter));
                juce::AccessibilityHandler::postAnnouncement(
                    "Mid-fade enabled at " + formatTime(midFadeCenter) + ". Select Edit Mid-Fade to adjust.",
                    juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            else
            {
                midFadeEnabled = false;  midFadeEditMode = false;
                saveUndoState("Mid-fade off");
                logEffect    ("Mid-fade off");
                juce::AccessibilityHandler::postAnnouncement("Mid-fade off.", juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            repaint(); break;
        }
        case 35:
        {
            gainEditMode = fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
            midFadeEditMode = !midFadeEditMode;
            juce::AccessibilityHandler::postAnnouncement(
                midFadeEditMode
                    ? "Mid-fade edit mode. Shift+Left/Right adjusts position 0.1s. Ctrl+Shift+Left/Right adjusts 1s. Up/Down changes duration. Centre at " + formatTime(midFadeCenter) + ", duration " + juce::String(midFadeDuration, 1) + "s."
                    : "Mid-fade edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }

        // ── Edit: Filters ─────────────────────────────────────────────────────
        case 36:
        {
            saveUndoState("Low shelf " + juce::String(!lowPassEnabled ? "on" : "off"));
            logEffect    ("Low shelf " + juce::String(!lowPassEnabled ? "on" : "off"));
            lowPassEnabled = !lowPassEnabled;
            lowShelfFilter[0].reset();  lowShelfFilter[1].reset();
            juce::AccessibilityHandler::postAnnouncement(
                lowPassEnabled ? "Low shelf on. " + juce::String((int)lowShelfFreq) + " Hz. Select Edit Low Shelf Frequency to adjust."
                               : "Low shelf off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 37:
        {
            gainEditMode = fadeInEditMode = fadeOutEditMode = highFreqEditMode = eqEditMode = false;
            lowFreqEditMode = !lowFreqEditMode;
            juce::AccessibilityHandler::postAnnouncement(
                lowFreqEditMode ? "Low shelf frequency edit mode. Use up and down arrow keys. Currently " + juce::String((int)lowShelfFreq) + " Hz."
                                : "Low shelf edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 38:
        {
            saveUndoState("High shelf " + juce::String(!highPassEnabled ? "on" : "off"));
            logEffect    ("High shelf " + juce::String(!highPassEnabled ? "on" : "off"));
            highPassEnabled = !highPassEnabled;
            highShelfFilter[0].reset();  highShelfFilter[1].reset();
            juce::AccessibilityHandler::postAnnouncement(
                highPassEnabled ? "High shelf on. " + juce::String((int)highShelfFreq) + " Hz. Select Edit High Shelf Frequency to adjust."
                                : "High shelf off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 39:
        {
            gainEditMode = fadeInEditMode = fadeOutEditMode = lowFreqEditMode = eqEditMode = false;
            highFreqEditMode = !highFreqEditMode;
            juce::AccessibilityHandler::postAnnouncement(
                highFreqEditMode ? "High shelf frequency edit mode. Use up and down arrow keys. Currently " + juce::String((int)highShelfFreq) + " Hz."
                                 : "High shelf edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }

        // ── Edit: EQ ─────────────────────────────────────────────────────────
        case 40:
        {
            eqEnabled = !eqEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                eqEnabled ? "EQ on. Select Edit EQ Bands to adjust." : "EQ off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }
        case 41:
        {
            gainEditMode = fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = false;
            eqEditMode = !eqEditMode;
            juce::AccessibilityHandler::postAnnouncement(
                eqEditMode ? "EQ edit mode. Tab to select band. Up and down arrows to adjust gain."
                           : "EQ edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint(); break;
        }

        // ── Edit: Crop ────────────────────────────────────────────────────────
        case 6: openCropDialog(true);  break;
        case 7: openCropDialog(false); break;
        case 8: applyCrop();           break;

        // ── Edit: History ─────────────────────────────────────────────────────
        case 9:  performUndo(); break;
        case 10: performRedo(); break;

        // ── Help ──────────────────────────────────────────────────────────────
        case 200:
            juce::AccessibilityHandler::postAnnouncement(
                "VytAudio is a keyboard-controlled audio editor. Open WAV files, navigate audio, apply effects, and export your edits using keyboard commands. Press Alt+H at any time to open this Help menu.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            break;

        // All Help reference items (201-242) and effect history (250, 300+) are read-only
        default: break;
    }
}

void MainComponent::logEffect(const juce::String& description)
{
    juce::Time now = juce::Time::getCurrentTime();
    juce::String entry = now.toString(false, true, true, false) + "  —  " + description;
    effectHistory.insert(0, entry);
    if (effectHistory.size() > 50)
        effectHistory.remove(effectHistory.size() - 1);
}

MainComponent::AppState MainComponent::captureCurrentState()
{
    AppState s;
    s.gain = gain;
    s.gainStep = gainStep;
    s.fadeInEnabled = fadeInEnabled;
    s.fadeOutEnabled = fadeOutEnabled;
    s.fadeInDuration = fadeInDuration;
    s.fadeOutDuration = fadeOutDuration;
    s.lowPassEnabled = lowPassEnabled;
    s.highPassEnabled = highPassEnabled;
    s.lowShelfFreq = lowShelfFreq;
    s.highShelfFreq = highShelfFreq;
    s.cropStart = cropStart;
    s.cropEnd = cropEnd;
    s.eqEnabled = eqEnabled;
    s.midFadeEnabled  = midFadeEnabled;
    s.midFadeCenter   = midFadeCenter;
    s.midFadeDuration = midFadeDuration;
    s.fileSampleRate = fileSampleRate;
    s.fileNumChannels = fileNumChannels;
    for (int b = 0; b < kNumEQBands; ++b) {
        s.eqBands[b] = eqBands[b];
    }
    s.sharedBuffer = sharedAudioBuffer;
    return s;
}

void MainComponent::saveUndoState(const juce::String& description)
{
    if ((int)undoStack.size() >= maxUndoLevels)
        undoStack.pop_front();
    AppState s = captureCurrentState();
    s.description = description;
    undoStack.push_back(s);
    redoStack.clear();
}

void MainComponent::restoreState(const AppState& s)
{
    bool wasPlaying = transportSource.isPlaying();
    double currentPos = transportSource.getCurrentPosition();
    gain = s.gain; gainStep = s.gainStep;
    fadeInEnabled = s.fadeInEnabled; fadeOutEnabled = s.fadeOutEnabled;
    fadeInDuration = s.fadeInDuration; fadeOutDuration = s.fadeOutDuration;
    lowPassEnabled = s.lowPassEnabled; highPassEnabled = s.highPassEnabled;
    lowShelfFreq = s.lowShelfFreq; highShelfFreq = s.highShelfFreq;
    cropStart = s.cropStart; cropEnd = s.cropEnd;
    eqEnabled = s.eqEnabled;
    midFadeEnabled  = s.midFadeEnabled;
    midFadeCenter   = s.midFadeCenter;
    midFadeDuration = s.midFadeDuration;
    fileSampleRate = s.fileSampleRate; fileNumChannels = s.fileNumChannels;
    for (int b = 0; b < kNumEQBands; ++b) eqBands[b] = s.eqBands[b];
    sharedAudioBuffer = s.sharedBuffer;

    transportSource.stop();
    transportSource.setSource(nullptr);
    if (sharedAudioBuffer != nullptr && sharedAudioBuffer->getNumSamples() > 0)
    {
        readerSource.reset();
        memorySource = std::make_unique<juce::MemoryAudioSource>(*sharedAudioBuffer, false);
        transportSource.setSource(memorySource.get(), 0, nullptr, fileSampleRate);
        cropStart = -1.0; cropEnd = -1.0;
    }
    else if (currentFile.existsAsFile())
    {
        memorySource.reset();
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(currentFile));
        if (reader)
        {
            auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
            transportSource.setSource(newSource.get(), 0, nullptr, newSource->getAudioFormatReader()->sampleRate);
            readerSource.reset(newSource.release());
        }
    }
    updateFilterCoefficients();
    updateEQCoefficients();
    double newLength = transportSource.getLengthInSeconds();
    if (newLength <= 0.0 && sharedAudioBuffer)
        newLength = sharedAudioBuffer->getNumSamples() / fileSampleRate;
    currentPos = juce::jlimit(0.0, juce::jmax(0.01, newLength), currentPos);
    transportSource.setPosition(currentPos);
    if (wasPlaying) transportSource.start();
    repaint();
}

void MainComponent::performUndo()
{
    if (undoStack.empty())
    {
        juce::AccessibilityHandler::postAnnouncement("Nothing to undo.", juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }
    juce::String whatChanged = undoStack.back().description;
    redoStack.push_back(captureCurrentState());
    restoreState(undoStack.back());
    undoStack.pop_back();
    logEffect("Undo: " + whatChanged);
    juce::AccessibilityHandler::postAnnouncement("Undo: " + whatChanged, juce::AccessibilityHandler::AnnouncementPriority::high);
}

void MainComponent::performRedo()
{
    if (redoStack.empty())
    {
        juce::AccessibilityHandler::postAnnouncement("Nothing to redo.", juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }
    juce::String whatChanged = redoStack.back().description;
    undoStack.push_back(captureCurrentState());
    restoreState(redoStack.back());
    redoStack.pop_back();
    logEffect("Redo: " + whatChanged);
    juce::AccessibilityHandler::postAnnouncement("Redo: " + whatChanged, juce::AccessibilityHandler::AnnouncementPriority::high);
}

//==============================================================================
void MainComponent::openCropDialog(bool isStart)
{
    if (readerSource == nullptr)
    {
        juce::AccessibilityHandler::postAnnouncement("No file loaded. Please open a WAV file first.", juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }
    double currentVal = isStart
        ? (cropStart >= 0.0 ? cropStart : transportSource.getCurrentPosition())
        : (cropEnd >= 0.0 ? cropEnd : transportSource.getCurrentPosition());
    juce::String dialogTitle = isStart ? "Set crop start time" : "Set crop end time";
    auto* dialog = new CropDialog(
        dialogTitle,
        currentVal,
        [this, isStart](double parsedSeconds)
        {
            double length = transportSource.getLengthInSeconds();
            double clamped = juce::jlimit(0.0, length, parsedSeconds);
            if (isStart)
            {
                cropStart = clamped;
                juce::AccessibilityHandler::postAnnouncement("Crop start set to " + formatTime(cropStart), juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            else
            {
                cropEnd = clamped;
                juce::AccessibilityHandler::postAnnouncement("Crop end set to " + formatTime(cropEnd), juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            repaint();
        });
    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(dialog);
    opts.dialogTitle = dialogTitle;
    opts.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
    dialog->grabFocusOnOpen();
}

//==============================================================================
void MainComponent::applyCrop()
{
    if (readerSource == nullptr && memorySource == nullptr)
    {
        juce::AccessibilityHandler::postAnnouncement("No file loaded.", juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }
    double length = transportSource.getLengthInSeconds();
    double startSec = (cropStart >= 0.0) ? cropStart : 0.0;
    double endSec   = (cropEnd >= 0.0)   ? cropEnd   : length;
    if (startSec >= endSec)
    {
        juce::AccessibilityHandler::postAnnouncement("Crop start must be before crop end.", juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }
    logEffect("Crop: " + formatTime(startSec) + " to " + formatTime(endSec));
    saveUndoState("Crop: " + formatTime(startSec) + " to " + formatTime(endSec));
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(currentFile));
    if (!reader)
    {
        juce::AccessibilityHandler::postAnnouncement("Could not read file for cropping.", juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }
    fileSampleRate = reader->sampleRate;
    fileNumChannels = (int)reader->numChannels;
    juce::int64 startSample = (juce::int64)(startSec * fileSampleRate);
    juce::int64 endSample   = (juce::int64)(endSec   * fileSampleRate);
    juce::int64 numSamples  = endSample - startSample;
    if (numSamples <= 0) return;
    sharedAudioBuffer = std::make_shared<juce::AudioBuffer<float>>(fileNumChannels, (int)numSamples);
    reader->read(sharedAudioBuffer.get(), 0, (int)numSamples, startSample, true, true);

    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();
    memorySource = std::make_unique<juce::MemoryAudioSource>(*sharedAudioBuffer, false);
    transportSource.setSource(memorySource.get(), 0, nullptr, fileSampleRate);

    cropStart = -1.0;
    cropEnd = -1.0;
    double newLength = numSamples / fileSampleRate;
    juce::AccessibilityHandler::postAnnouncement("Crop applied. New length is " + formatTime(newLength) + ". Press Space to play.", juce::AccessibilityHandler::AnnouncementPriority::high);
    repaint();
}

void MainComponent::announceEffectsStatus()
{
    juce::StringArray activeEffects;
    if (gain != 1.0f)
        activeEffects.add("Gain is set to " + juce::String(gain, 2) + "x.");
    if (lowPassEnabled)
        activeEffects.add("Low shelf filter is on at " + juce::String((int)lowShelfFreq) + " Hertz.");
    if (highPassEnabled)
        activeEffects.add("High shelf filter is on at " + juce::String((int)highShelfFreq) + " Hertz.");
    if (fadeInEnabled)
        activeEffects.add("Fade in is on for " + juce::String(fadeInDuration, 1) + " seconds.");
    if (fadeOutEnabled)
        activeEffects.add("Fade out is on for " + juce::String(fadeOutDuration, 1) + " seconds.");
    if (midFadeEnabled)
        activeEffects.add("Mid-fade is on, centred at " + formatTime(midFadeCenter) + ", duration " + juce::String(midFadeDuration, 1) + " seconds.");
    if (eqEnabled)
    {
        int activeBands = 0;
        for (auto& b : eqBands) if (b.enabled) activeBands++;
        activeEffects.add("EQ is on with " + juce::String(activeBands) + " active bands.");
    }
    juce::String msg;
    if (activeEffects.isEmpty())
        msg = "No effects are currently applied.";
    else
        msg = "Current status: " + activeEffects.joinIntoString(" ");
    juce::AccessibilityHandler::postAnnouncement(msg, juce::AccessibilityHandler::AnnouncementPriority::high);
}

//==============================================================================
void MainComponent::importFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select a WAV file",
        juce::File{},
        "*.wav");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            juce::File file = fc.getResult();
            if (!file.existsAsFile()) return;
            currentFile = file;
            transportSource.stop();
            transportSource.setSource(nullptr);
            readerSource.reset();
            memorySource.reset();
            sharedAudioBuffer.reset();
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
            if (reader == nullptr) return;
            currentSampleRate = reader->sampleRate;
            fileSampleRate = reader->sampleRate;
            fileNumChannels = (int)reader->numChannels;
            updateFilterCoefficients();
            updateEQCoefficients();
            auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
            transportSource.setSource(newSource.get(), 0, nullptr, newSource->getAudioFormatReader()->sampleRate);
            readerSource.reset(newSource.release());
            juce::AccessibilityHandler::postAnnouncement(currentFile.getFileName() + " loaded successfully.", juce::AccessibilityHandler::AnnouncementPriority::high);
        });
}

//==============================================================================
void MainComponent::togglePlayback()
{
    bool hasSource = (readerSource != nullptr) || (memorySource != nullptr);
    if (!hasSource) return;
    if (transportSource.isPlaying())
        transportSource.stop();
    else
        transportSource.start();
}

void MainComponent::exportModifiedFile()
{
    bool hasMemoryBuffer = (sharedAudioBuffer != nullptr && sharedAudioBuffer->getNumSamples() > 0);
    if (!hasMemoryBuffer && (readerSource == nullptr || !currentFile.existsAsFile()))
        return;
    auto chooser = std::make_shared<juce::FileChooser>(
        "Export modified WAV file",
        currentFile.getSiblingFile(currentFile.getFileNameWithoutExtension() + "_modified"),
        "*.wav");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this, hasMemoryBuffer, chooser](const juce::FileChooser& fc)
        {
            auto outputFile = fc.getResult();
            if (outputFile == juce::File{}) return;
            if (!outputFile.hasFileExtension(".wav"))
                outputFile = outputFile.withFileExtension(".wav");
            std::unique_ptr<juce::FileOutputStream> fileStream(outputFile.createOutputStream());
            if (fileStream == nullptr || !fileStream->openedOk()) return;
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(fileStream.release(), fileSampleRate, (unsigned int)fileNumChannels, 16, {}, 0));
            if (writer == nullptr) return;

            const double exportSR = (fileSampleRate > 0.0) ? fileSampleRate : currentSampleRate;
            juce::IIRFilter exportLow[2], exportHigh[2];
            auto lowCoeffs = juce::IIRCoefficients::makeLowShelf(exportSR, lowShelfFreq, 0.7, 0.25f);
            auto highCoeffs = juce::IIRCoefficients::makeHighShelf(exportSR, highShelfFreq, 0.7, 0.25f);
            juce::IIRFilter exportEQ[kNumEQBands][2];
            if (eqEnabled)
            {
                for (int b = 0; b < kNumEQBands; ++b)
                {
                    float lg = juce::Decibels::decibelsToGain((float)eqBands[b].gainDB);
                    auto c = juce::IIRCoefficients::makePeakFilter(exportSR, eqBands[b].freq, eqBands[b].Q, lg);
                    for (int ch = 0; ch < 2; ++ch)
                        exportEQ[b][ch].setCoefficients(c);
                }
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                exportLow[ch].setCoefficients(lowCoeffs);
                exportHigh[ch].setCoefficients(highCoeffs);
            }

            if (hasMemoryBuffer)
            {
                juce::AudioBuffer<float> copy(*sharedAudioBuffer);
                copy.applyGain(gain);
                double totalLength = (double)copy.getNumSamples() / fileSampleRate;
                for (int ch = 0; ch < copy.getNumChannels() && ch < 2; ++ch)
                {
                    float* data = copy.getWritePointer(ch);
                    if (lowPassEnabled) exportLow[ch].processSamples(data, copy.getNumSamples());
                    if (highPassEnabled) exportHigh[ch].processSamples(data, copy.getNumSamples());
                    if (eqEnabled)
                        for (int b = 0; b < kNumEQBands; ++b)
                            if (eqBands[b].enabled) exportEQ[b][ch].processSamples(data, copy.getNumSamples());
                    if (fadeInEnabled || fadeOutEnabled)
                    {
                        for (int s = 0; s < copy.getNumSamples(); ++s)
                        {
                            double pos = (double)s / fileSampleRate;
                            float  fadeGain = 1.0f;
                            if (fadeInEnabled && fadeInDuration > 0.0)
                                fadeGain *= juce::jlimit(0.0f, 1.0f, (float)(pos / fadeInDuration));
                            if (fadeOutEnabled && fadeOutDuration > 0.0)
                                fadeGain *= juce::jlimit(0.0f, 1.0f, (float)((totalLength - pos) / fadeOutDuration));
                            if (midFadeEnabled && midFadeDuration > 0.0)
                            {
                                double halfDip  = midFadeDuration * 0.5;
                                double dipStart = midFadeCenter - halfDip;
                                double dipEnd   = midFadeCenter + halfDip;
                                if (pos >= dipStart && pos < midFadeCenter)
                                    fadeGain *= juce::jlimit(0.0f, 1.0f, (float)((midFadeCenter - pos) / halfDip));
                                else if (pos >= midFadeCenter && pos <= dipEnd)
                                    fadeGain *= juce::jlimit(0.0f, 1.0f, (float)((pos - midFadeCenter) / halfDip));
                            }
                            data[s] *= fadeGain;
                        }
                    }
                }
                writer->writeFromAudioSampleBuffer(copy, 0, copy.getNumSamples());
            }
            else
            {
                std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(currentFile));
                if (reader == nullptr) return;
                double totalLength = (double)reader->lengthInSamples / exportSR;
                const int bufferSize = 32768;
                juce::AudioBuffer<float> buffer(fileNumChannels, bufferSize);
                auto        totalSamples = reader->lengthInSamples;
                juce::int64 processed = 0;
                while (processed < totalSamples)
                {
                    int thisBlock = (int)juce::jmin((juce::int64)bufferSize, totalSamples - processed);
                    if (!reader->read(&buffer, 0, thisBlock, processed, true, true)) break;
                    buffer.applyGain(0, thisBlock, gain);
                    for (int ch = 0; ch < buffer.getNumChannels() && ch < 2; ++ch)
                    {
                        float* data = buffer.getWritePointer(ch);
                        if (lowPassEnabled) exportLow[ch].processSamples(data, thisBlock);
                        if (highPassEnabled) exportHigh[ch].processSamples(data, thisBlock);
                        if (eqEnabled)
                            for (int b = 0; b < kNumEQBands; ++b)
                                if (eqBands[b].enabled) exportEQ[b][ch].processSamples(data, thisBlock);
                        if (fadeInEnabled || fadeOutEnabled)
                        {
                            for (int s = 0; s < thisBlock; ++s)
                            {
                                double pos = (double)(processed + s) / exportSR;
                                float  fadeGain = 1.0f;
                                if (fadeInEnabled && fadeInDuration > 0.0)
                                    fadeGain *= juce::jlimit(0.0f, 1.0f, (float)(pos / fadeInDuration));
                                if (fadeOutEnabled && fadeOutDuration > 0.0)
                                    fadeGain *= juce::jlimit(0.0f, 1.0f, (float)((totalLength - pos) / fadeOutDuration));
                                if (midFadeEnabled && midFadeDuration > 0.0)
                                {
                                    double halfDip  = midFadeDuration * 0.5;
                                    double dipStart = midFadeCenter - halfDip;
                                    double dipEnd   = midFadeCenter + halfDip;
                                    if (pos >= dipStart && pos < midFadeCenter)
                                        fadeGain *= juce::jlimit(0.0f, 1.0f, (float)((midFadeCenter - pos) / halfDip));
                                    else if (pos >= midFadeCenter && pos <= dipEnd)
                                        fadeGain *= juce::jlimit(0.0f, 1.0f, (float)((pos - midFadeCenter) / halfDip));
                                }
                                data[s] *= fadeGain;
                            }
                        }
                    }
                    writer->writeFromAudioSampleBuffer(buffer, 0, thisBlock);
                    processed += thisBlock;
                }
            }
        });
}
