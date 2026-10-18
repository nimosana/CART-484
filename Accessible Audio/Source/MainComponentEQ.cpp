#include "MainComponent.h"

//==============================================================================
// Parametric EQ: coefficient updates, slider UI drawing, and per-sample
// application of the active bands.
//==============================================================================
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
    const int axisW = 32;
    const int trackPad = 6;

    juce::Rectangle<int> trackArea(
        area.getX() + axisW,
        area.getY() + labelTopH,
        juce::jmax(0, area.getWidth() - axisW),
        juce::jmax(0, area.getHeight() - labelTopH - labelBotH));

    const int trackLeft = trackArea.getX();
    const int trackTop = trackArea.getY();
    const int trackH = trackArea.getHeight();
    const int bandW = juce::jmax(1, trackArea.getWidth() / kNumEQBands);

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
        bool   on = eqBands[b].enabled;
        double db = eqBands[b].gainDB;
        juce::Rectangle<int> col(trackLeft + b * bandW, trackTop, bandW, trackH);

        if (sel) { g.setColour(juce::Colour(0x221e6eff)); g.fillRect(col); }
        if (b < kNumEQBands - 1) { g.setColour(juce::Colour(DarkLookAndFeel::line1)); g.drawVerticalLine(col.getRight(), (float)trackTop, (float)(trackTop + trackH)); }

        int trackCX = col.getCentreX();
        g.setColour(juce::Colour(DarkLookAndFeel::line2));
        g.fillRect(trackCX - 1, col.getY() + 6, 2, col.getHeight() - 12);

        float thumbFrac = 0.5f - (float)(db / (2.0 * kDbMax));
        int   thumbCY = col.getY() + (int)(thumbFrac * col.getHeight());
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
        g.setColour(sel ? juce::Colour(DarkLookAndFeel::txt0) : (on ? juce::Colour(DarkLookAndFeel::txt1) : juce::Colour(DarkLookAndFeel::txt2)));
        juce::String dbStr = (db >= 0 ? "+" : "") + juce::String((int)std::round(db));
        g.drawText(dbStr, juce::Rectangle<int>(col.getX(), area.getY(), bandW, labelTopH), juce::Justification::centred);

        g.setFont(10.0f);
        g.setColour(sel ? juce::Colour(DarkLookAndFeel::accentGreen) : (on ? juce::Colour(DarkLookAndFeel::txt1) : juce::Colour(DarkLookAndFeel::txt2)));
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
