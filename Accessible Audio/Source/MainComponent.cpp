#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    //setSize(400, 350);
    setSize(400, 380);  // base height without EQ sliders open

    formatManager.registerBasicFormats();

    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(menuBar.get());
    if (menuBar) {
        menuBar->setModel(this);
        menuBar->setBounds(0, 0, getWidth(), 25);
    }
    setWantsKeyboardFocus(true);

    addKeyListener(this);

    setAudioChannels(0, 2);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId));

    g.setFont(14.0f);
    auto bounds = getLocalBounds().reduced(8, 8);

    // --- bottom line: gain ---
    juce::String gainInfo;
    gainInfo << "Gain: " << juce::String(gain, 2) << "x";
    gainInfo << "   Step: " << juce::String(gainStep, 2);
    if (gainEditMode)
        gainInfo << "   [arrows adjust]";
    if (cropStart >= 0.0)
        gainInfo << "   In: " << formatTime(cropStart);
    if (cropEnd >= 0.0)
        gainInfo << "   Out: " << formatTime(cropEnd);

    g.setColour(juce::Colours::white);
    g.drawFittedText(gainInfo, bounds.removeFromBottom(24),
        juce::Justification::centredLeft, 1);

    // --- second line: high shelf ---
    {
        juce::String s;
        s << "High Shelf (H): " << (highPassEnabled ? "ON" : "off");
        s << "  @  " << juce::String((int)highShelfFreq) << " Hz";
        if (highFreqEditMode)
            s << "  [Ctrl+H | arrows change freq]";
        else
            s << "  [Ctrl+H to edit freq]";

        g.setColour(highPassEnabled ? juce::Colours::lightblue : juce::Colours::grey);
        g.drawFittedText(s, bounds.removeFromBottom(24),
            juce::Justification::centredLeft, 1);
    }

    // --- third line: low shelf ---
    {
        juce::String s;
        s << "Low Shelf  (L): " << (lowPassEnabled ? "ON" : "off");
        s << "  @  " << juce::String((int)lowShelfFreq) << " Hz";
        if (lowFreqEditMode)
            s << "  [Ctrl+L | arrows change freq]";
        else
            s << "  [Ctrl+L to edit freq]";

        g.setColour(lowPassEnabled ? juce::Colours::lightblue : juce::Colours::grey);
        g.drawFittedText(s, bounds.removeFromBottom(24),
            juce::Justification::centredLeft, 1);
    }

    // --- fourth line: fade out ---
    {
        juce::String s;
        s << "Fade Out (W): " << (fadeOutEnabled ? "ON" : "off");
        s << "   " << juce::String(fadeOutDuration, 1) << " s";
        if (fadeOutEditMode)
            s << "  [Ctrl+W | arrows change duration]";
        else
            s << "  [Ctrl+W to edit duration]";

        g.setColour(fadeOutEnabled ? juce::Colours::lightyellow : juce::Colours::grey);
        g.drawFittedText(s, bounds.removeFromBottom(24),
            juce::Justification::centredLeft, 1);
    }

    // --- fifth line: fade in ---
    {
        juce::String s;
        s << "Fade In  (Q): " << (fadeInEnabled ? "ON" : "off");
        s << "   " << juce::String(fadeInDuration, 1) << " s";
        if (fadeInEditMode)
            s << "  [Ctrl+Q | arrows change duration]";
        else
            s << "  [Ctrl+Q to edit duration]";

        g.setColour(fadeInEnabled ? juce::Colours::lightyellow : juce::Colours::grey);
        g.drawFittedText(s, bounds.removeFromBottom(24),
            juce::Justification::centredLeft, 1);
    }
    // --- sixth line: EQ ---
// --- sixth line: EQ ---
// --- EQ status line + sliders ---
    {
        // Always show the status line
        juce::String s;
        s << "EQ           (E): " << (eqEnabled ? "ON" : "off");
        int active = 0;
        for (auto& b : eqBands) if (b.enabled) ++active;
        s << "   " << active << " band(s) active";
        if (eqEditMode)
            s << "   [Tab=select band  Up/Down=gain  Space=toggle]";
        else
            s << "   [Ctrl+E to edit]";

        g.setColour(eqEnabled ? juce::Colours::lightgreen : juce::Colours::grey);
        g.drawFittedText(s, bounds.removeFromBottom(24),
            juce::Justification::centredLeft, 1);

        // Slider area — only visible in edit mode
        if (eqEditMode)
        {
            auto sliderArea = bounds.removeFromBottom(120);
            sliderArea = sliderArea.reduced(4, 4);
            g.setColour(juce::Colour(0xff111122));
            g.fillRect(sliderArea);
            drawEQSliders(g, sliderArea);
        }
    }
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

    // ── Layout constants ────────────────────────────────────────────────────
    const int labelTopH = 18;   // dB value label at top of each column
    const int labelBotH = 16;   // frequency name at bottom
    const int axisW = 28;   // left axis column width
    const int sliderW = area.getWidth() / (kNumEQBands + 1); // +1 so axis fits
    const int trackPad = 4;    // horizontal inset so track is centred in column

    // Slider track lives between the two labels
    juce::Rectangle<int> trackArea(
        area.getX() + axisW,
        area.getY() + labelTopH,
        area.getWidth() - axisW,
        area.getHeight() - labelTopH - labelBotH);

    const int trackLeft = trackArea.getX();
    const int trackTop = trackArea.getY();
    const int trackH = trackArea.getHeight();
    const int bandW = trackArea.getWidth() / kNumEQBands;

    // ── Background ──────────────────────────────────────────────────────────
    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillRect(area);

    // ── dB axis labels + horizontal grid lines ───────────────────────────────
    // Grid at +12, +6, 0, -6, -12 dB
    const int gridDBs[] = { 12, 6, 0, -6, -12 };
    for (int db : gridDBs)
    {
        float frac = 0.5f - (float)db / (2.0f * kDbMax);
        int   lineY = trackTop + (int)(frac * trackH);

        // Axis label
        g.setFont(juce::Font(9.0f));
        g.setColour(juce::Colour(0xff888899));
        juce::String lbl = (db > 0 ? "+" : "") + juce::String(db);
        g.drawText(lbl,
            area.getX(), lineY - 7, axisW - 4, 14,
            juce::Justification::centredRight, false);

        // Grid line
        g.setColour(db == 0 ? juce::Colour(0xff444466)
            : juce::Colour(0xff252535));
        g.drawHorizontalLine(lineY, (float)trackLeft, (float)trackArea.getRight());
    }

    // Thin border around the whole slider track area
    g.setColour(juce::Colour(0xff2a2a40));
    g.drawRect(trackArea);

    // ── Per-band rendering ───────────────────────────────────────────────────
    for (int b = 0; b < kNumEQBands; ++b)
    {
        bool   sel = eqEditMode && (b == eqSelectedBand);
        bool   on = eqBands[b].enabled;
        double db = eqBands[b].gainDB;

        // Column rectangle within the track area
        juce::Rectangle<int> col(trackLeft + b * bandW, trackTop, bandW, trackH);

        // Selection highlight
        if (sel)
        {
            g.setColour(juce::Colour(0x1800c8ff));
            g.fillRect(col);
        }

        // Vertical divider between bands (skip after last)
        if (b < kNumEQBands - 1)
        {
            g.setColour(juce::Colour(0xff252535));
            g.drawVerticalLine(col.getRight(), (float)trackTop, (float)(trackTop + trackH));
        }

        // ── Thin centre track line ────────────────────────────────────────
        int trackCX = col.getCentreX();
        g.setColour(juce::Colour(0xff383850));
        g.fillRect(trackCX - 1, col.getY() + 4, 2, col.getHeight() - 8);

        // ── Thumb position ───────────────────────────────────────────────
        float thumbFrac = 0.5f - (float)(db / (2.0 * kDbMax));
        int   thumbCY = col.getY() + (int)(thumbFrac * col.getHeight());

        // Thumb rectangle (horizontal grip, like image 1)
        const int thumbW = bandW - trackPad * 2 - 2;
        const int thumbH = 10;
        juce::Rectangle<int> thumb(
            trackCX - thumbW / 2,
            thumbCY - thumbH / 2,
            thumbW, thumbH);

        // Thumb body
        juce::Colour thumbFace = !on ? juce::Colour(0xff3a3a50)
            : sel ? juce::Colour(0xffd0eeff)
            : juce::Colour(0xffc0c8d8);
        g.setColour(thumbFace);
        g.fillRoundedRectangle(thumb.toFloat(), 2.0f);

        // Thumb border
        g.setColour(sel ? juce::Colour(0xff00c8ff)
            : (on ? juce::Colour(0xff7788aa) : juce::Colour(0xff444455)));
        g.drawRoundedRectangle(thumb.toFloat(), 2.0f, 0.8f);

        // Three notch lines on the thumb (like a real fader grip)
        if (on || sel)
        {
            juce::Colour notchCol = sel ? juce::Colour(0xff006090)
                : juce::Colour(0xff8899aa);
            g.setColour(notchCol);
            for (int n = -1; n <= 1; ++n)
            {
                int nx = trackCX + n * 4;
                g.fillRect(nx, thumb.getCentreY() - 3, 1, 6);
            }
        }

        // ── dB value label (above slider area, top of column) ───────────
        g.setFont(juce::Font(9.5f, sel ? juce::Font::bold : juce::Font::plain));
        g.setColour(sel ? juce::Colours::white
            : (on ? juce::Colour(0xffaabbcc) : juce::Colour(0xff555566)));
        juce::String dbStr = (db >= 0 ? "+" : "") + juce::String((int)std::round(db));
        g.drawText(dbStr,
            col.getX(), area.getY(), bandW, labelTopH,
            juce::Justification::centred, false);

        // ── Frequency label (below slider area) ──────────────────────────
        g.setFont(9.0f);
        g.setColour(sel ? juce::Colour(0xff00c8ff)
            : (on ? juce::Colour(0xff7788aa) : juce::Colour(0xff444455)));
        g.drawText(eqBands[b].name,
            col.getX(), area.getBottom() - labelBotH, bandW, labelBotH,
            juce::Justification::centred, false);
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

    // Apply fade in / fade out as per-sample linear ramps.
    // getCurrentPosition() returns the playhead AFTER the block was consumed,
    // so the block started one block-duration earlier.
    if ((fadeInEnabled || fadeOutEnabled) && currentSampleRate > 0.0)
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
// Static helper � used in paint() and announcements
juce::String MainComponent::formatTime(double seconds)
{
    int mins = (int)(seconds / 60);
    int secs = (int)(seconds) % 60;
    return juce::String(mins) + " minutes " + juce::String(secs) + " seconds";
}


//==============================================================================
bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    char c = (char)std::tolower(key.getTextCharacter());
    bool ctrlDown = key.getModifiers().isCtrlDown();
    bool altDown = key.getModifiers().isAltDown();
    // Digit scrub

    if (readerSource != nullptr || transportSource.getLengthInSeconds() > 0.0)
    {
        //auto c = key.getTextCharacter();
        if (c >= '0' && c <= '9')
        {
            transportSource.setPosition((c - '0') / 10.0 * transportSource.getLengthInSeconds());
            return true;
        }
        else if (c == 'g')
        {
            gainEditMode = !gainEditMode;
            lowFreqEditMode = false;
            highFreqEditMode = false;

            juce::AccessibilityHandler::postAnnouncement(
                gainEditMode ? "Gain edit mode enabled." : "Gain edit mode disabled.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }

        // announce time
        else if (c == 't')
        {
            announceTime();
            return true;
        }
        else if (c == 'e' && !ctrlDown && !altDown)
        {
            eqEnabled = !eqEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                eqEnabled ? "EQ on." : "EQ off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // I � set crop IN point at playhead
        else if (c == 'i')
        {
            cropStart = transportSource.getCurrentPosition();
            juce::String msg = "Crop start set at " + formatTime(cropStart);
            juce::AccessibilityHandler::postAnnouncement(
                msg, juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // O � set crop OUT point at playhead
        else if (c == 'o' && !ctrlDown)
        {
            cropEnd = transportSource.getCurrentPosition();
            juce::String msg = "Crop end set at " + formatTime(cropEnd);
            juce::AccessibilityHandler::postAnnouncement(
                msg, juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // Shift+C � apply the crop
        else if (c == 'c' && key.getModifiers().isShiftDown())
        {
            applyCrop();
            return true;
        }

        if (c == 'l')
        {
            lowPassEnabled = !lowPassEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                lowPassEnabled
                ? "Low shelf filter on. " + juce::String((int)lowShelfFreq) + " Hz."
                : "Low shelf filter off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            lowShelfFilter[0].reset();
            lowShelfFilter[1].reset();
            repaint();
            return true;
        }
        else if (c == 'h')
        {
            highPassEnabled = !highPassEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                highPassEnabled
                ? "High shelf filter on. " + juce::String((int)highShelfFreq) + " Hz."
                : "High shelf filter off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            highShelfFilter[0].reset();
            highShelfFilter[1].reset();
            repaint();
            return true;
        }
        else if (c == 'q')
        {
            fadeInEnabled = !fadeInEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                fadeInEnabled
                ? "Fade in on. " + juce::String(fadeInDuration, 1) + " seconds."
                : "Fade in off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        else if (c == 'w')
        {
            fadeOutEnabled = !fadeOutEnabled;
            juce::AccessibilityHandler::postAnnouncement(
                fadeOutEnabled
                ? "Fade out on. " + juce::String(fadeOutDuration, 1) + " seconds."
                : "Fade out off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
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
    // Tab cycles through menus
    //else if (key == juce::KeyPress::tabKey)
    //{
    //    if (activeMenuIndex == -1)
    //        activeMenuIndex = 0;
    //    else
    //        activeMenuIndex = (activeMenuIndex + 1) % getMenuBarNames().size();

    //    if (true)
    //        menuBar->showMenu(activeMenuIndex); // this is required; no �highlight only� possible

    //    return true;
    //}
    double scrubAmount = ctrlDown ? 10.0 : 2;
    double currentPos = transportSource.getCurrentPosition();
    double length = transportSource.getLengthInSeconds();
    //int keyCode = key.getKeyCode();


    if (ctrlDown)
    {
        if (key.getKeyCode() == 'o' || key.getKeyCode() == 'O') {
            importFile();
        }
        if (key.getKeyCode() == 's' || key.getKeyCode() == 'S') {
            exportModifiedFile();
        }
        // Ctrl+L: enter/exit low shelf frequency edit mode
        if (key.getKeyCode() == 'l' || key.getKeyCode() == 'L')
        {
            lowFreqEditMode = !lowFreqEditMode;
            gainEditMode = false;
            highFreqEditMode = false;
            fadeInEditMode = false;
            fadeOutEditMode = false;
            juce::AccessibilityHandler::postAnnouncement(
                lowFreqEditMode
                ? "Low shelf frequency edit mode. Use up and down arrows to change frequency. Currently " + juce::String((int)lowShelfFreq) + " Hz."
                : "Low shelf frequency edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == 'e' || key.getKeyCode() == 'E')
        {
            eqEditMode = !eqEditMode;
            // Resize the window to make room for (or hide) the slider panel
            setSize(getWidth(), eqEditMode ? 500 : 380);
            juce::AccessibilityHandler::postAnnouncement(
                eqEditMode
                ? "EQ edit mode. Tab to select band. Up and down arrows to adjust gain. Space to toggle band."
                : "EQ edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // Ctrl+H: enter/exit high shelf frequency edit mode
        if (key.getKeyCode() == 'h' || key.getKeyCode() == 'H')
        {
            highFreqEditMode = !highFreqEditMode;
            gainEditMode = false;
            lowFreqEditMode = false;
            fadeInEditMode = false;
            fadeOutEditMode = false;
            juce::AccessibilityHandler::postAnnouncement(
                highFreqEditMode
                ? "High shelf frequency edit mode. Use up and down arrows to change frequency. Currently " + juce::String((int)highShelfFreq) + " Hz."
                : "High shelf frequency edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // Ctrl+Q: enter/exit fade-in duration edit mode
        if (key.getKeyCode() == 'q' || key.getKeyCode() == 'Q')
        {
            fadeInEditMode = !fadeInEditMode;
            fadeOutEditMode = false;
            gainEditMode = false;
            lowFreqEditMode = false;
            highFreqEditMode = false;
            juce::AccessibilityHandler::postAnnouncement(
                fadeInEditMode
                ? "Fade in duration edit mode. Use up and down arrows to change duration. Currently " + juce::String(fadeInDuration, 1) + " seconds."
                : "Fade in duration edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        // Ctrl+W: enter/exit fade-out duration edit mode
        if (key.getKeyCode() == 'w' || key.getKeyCode() == 'W')
        {
            fadeOutEditMode = !fadeOutEditMode;
            fadeInEditMode = false;
            gainEditMode = false;
            lowFreqEditMode = false;
            highFreqEditMode = false;
            juce::AccessibilityHandler::postAnnouncement(
                fadeOutEditMode
                ? "Fade out duration edit mode. Use up and down arrows to change duration. Currently " + juce::String(fadeOutDuration, 1) + " seconds."
                : "Fade out duration edit mode off.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::homeKey)
        {
            transportSource.setPosition(0.0); // start of file
            repaint();
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::endKey)
        {
            double length = transportSource.getLengthInSeconds();
            transportSource.setPosition(length); // end of file
            repaint();
            return true;
        }
    }
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
            juce::AccessibilityHandler::postAnnouncement(
                msg, juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key == juce::KeyPress::spaceKey)
        {
            eqBands[eqSelectedBand].enabled = !eqBands[eqSelectedBand].enabled;
            updateEQCoefficients();
            juce::AccessibilityHandler::postAnnouncement(
                eqBands[eqSelectedBand].name +
                (eqBands[eqSelectedBand].enabled ? " on." : " off."),
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::upKey ||
            key.getKeyCode() == juce::KeyPress::downKey)
        {
            bool   up = (key.getKeyCode() == juce::KeyPress::upKey);
            double step = key.getModifiers().isShiftDown() ? 3.0 : 1.0;
            double& db = eqBands[eqSelectedBand].gainDB;
            db = juce::jlimit(-12.0, 12.0, db + (up ? step : -step));
            eqBands[eqSelectedBand].enabled = true;
            updateEQCoefficients();
            juce::String msg;
            msg << eqBands[eqSelectedBand].name << " "
                << (db >= 0 ? "+" : "") << juce::String(db, 1) << " dB.";
            juce::AccessibilityHandler::postAnnouncement(
                msg, juce::AccessibilityHandler::AnnouncementPriority::high);
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
            juce::AccessibilityHandler::postAnnouncement(
                "Fade in " + juce::String(fadeInDuration, 1) + " seconds.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            fadeInDuration = juce::jlimit(0.1, 30.0, fadeInDuration - 0.1);
            juce::AccessibilityHandler::postAnnouncement(
                "Fade in " + juce::String(fadeInDuration, 1) + " seconds.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
    }
    else if (fadeOutEditMode)
    {
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            fadeOutDuration = juce::jlimit(0.1, 30.0, fadeOutDuration + 0.1);
            juce::AccessibilityHandler::postAnnouncement(
                "Fade out " + juce::String(fadeOutDuration, 1) + " seconds.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            fadeOutDuration = juce::jlimit(0.1, 30.0, fadeOutDuration - 0.1);
            juce::AccessibilityHandler::postAnnouncement(
                "Fade out " + juce::String(fadeOutDuration, 1) + " seconds.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
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
            juce::AccessibilityHandler::postAnnouncement(
                "Low shelf " + juce::String((int)lowShelfFreq) + " Hz.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            lowShelfFreq = juce::jlimit(0.0, 2000.0, lowShelfFreq - 50.0);
            updateFilterCoefficients();
            juce::AccessibilityHandler::postAnnouncement(
                "Low shelf " + juce::String((int)lowShelfFreq) + " Hz.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
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
            juce::AccessibilityHandler::postAnnouncement(
                "High shelf " + juce::String((int)highShelfFreq) + " Hz.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            highShelfFreq = juce::jlimit(1000.0, 20000.0, highShelfFreq - 200.0);
            updateFilterCoefficients();
            juce::AccessibilityHandler::postAnnouncement(
                "High shelf " + juce::String((int)highShelfFreq) + " Hz.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            repaint();
            return true;
        }
    }
    else if (gainEditMode)
    {
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            gain = juce::jlimit(0.0f, 4.0f, gain + gainStep);
            juce::AccessibilityHandler::postAnnouncement(
                "Gain set to " + juce::String(gain, 1),
                juce::AccessibilityHandler::AnnouncementPriority::high
            );
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            gain = juce::jlimit(0.0f, 4.0f, gain - gainStep);
            juce::AccessibilityHandler::postAnnouncement(
                "Gain set to " + juce::String(gain, 1),
                juce::AccessibilityHandler::AnnouncementPriority::high
            );
            repaint();
            return true;
        }
    }

    // Alt+F / Alt+P opens menus
    if (altDown)
    {
        if (c == 'f')
        {
            if (menuBar) menuBar->showMenu(0);
            return true;
        }
        if (c == 'p')
        {
            if (menuBar) menuBar->showMenu(1);
            return true;
        }
        if (c == 'e')
        {
            if (menuBar) menuBar->showMenu(2);
            return true;
        }
    }
    return false;
}

void MainComponent::announceTime()
{
    double currentPos = transportSource.getCurrentPosition();
    double totalLength = transportSource.getLengthInSeconds();
    juce::String msg;
    msg << formatTime(currentPos);
    juce::AccessibilityHandler::postAnnouncement(
        msg, juce::AccessibilityHandler::AnnouncementPriority::high);
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Playback", "Edit" };
}

juce::PopupMenu MainComponent::getMenuForIndex(
    int menuIndex,
    const juce::String&)
{
    juce::PopupMenu menu;

    if (menuIndex == 0) // File
    {
        menu.addItem(1, "Open WAV");
        menu.addSeparator();
        menu.addItem(2, "Save Modified WAV");
        menu.addSeparator();
        menu.addItem(3, "Quit");
    }
    else if (menuIndex == 1) // Playback
    {
        menu.addItem(4,
            transportSource.isPlaying() ? "Stop" : "Play");
        //menu.addItem(5, "Precise Scrubbing (arrow keys)", true, scrubMode);
    }
    else if (menuIndex == 2) // Edit
    {
        menu.addItem(5, "Adjust Gain (arrow keys)", true, gainEditMode);
        menu.addSeparator();
        menu.addItem(6, "Set Crop Start (type time)");
        menu.addItem(7, "Set Crop End (type time)");
        menu.addItem(8, "Apply Crop", cropStart >= 0.0 && cropEnd >= 0.0);
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int)
{
    switch (menuItemID)
    {
    case 1:
        importFile();
        break;
    case 2:
        exportModifiedFile();
        break;
    case 3:
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        break;
    case 4:
        togglePlayback();
        break;
    case 5:
        gainEditMode = !gainEditMode;
        repaint();
        break;
    case 6: openCropDialog(true);   break;
    case 7: openCropDialog(false);  break;
    case 8: applyCrop();            break;
    }
}


//==============================================================================
void MainComponent::openCropDialog(bool isStart)
{
    if (readerSource == nullptr)
    {
        juce::AccessibilityHandler::postAnnouncement(
            "No file loaded. Please open a WAV file first.",
            juce::AccessibilityHandler::AnnouncementPriority::high);
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
                juce::AccessibilityHandler::postAnnouncement(
                    "Crop start set to " + formatTime(cropStart),
                    juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            else
            {
                cropEnd = clamped;
                juce::AccessibilityHandler::postAnnouncement(
                    "Crop end set to " + formatTime(cropEnd),
                    juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            repaint();
        });

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(dialog);
    opts.dialogTitle = dialogTitle;
    opts.dialogBackgroundColour = getLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();

    // Give focus to the text editor so screen reader reads the field immediately
    dialog->grabFocusOnOpen();
}


//==============================================================================
void MainComponent::applyCrop()
{
    if (readerSource == nullptr)
    {
        juce::AccessibilityHandler::postAnnouncement(
            "No file loaded.",
            juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }

    double length = transportSource.getLengthInSeconds();

    // Fall back to full file if points aren't set
    double startSec = (cropStart >= 0.0) ? cropStart : 0.0;
    double endSec = (cropEnd >= 0.0) ? cropEnd : length;

    if (startSec >= endSec)
    {
        juce::AccessibilityHandler::postAnnouncement(
            "Crop start must be before crop end. Please reset your crop points.",
            juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }

    // Read the original file into memory
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(currentFile));

    if (reader == nullptr)
    {
        juce::AccessibilityHandler::postAnnouncement(
            "Could not read file for cropping.",
            juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }

    fileSampleRate = reader->sampleRate;
    fileNumChannels = (int)reader->numChannels;

    juce::int64 startSample = (juce::int64)(startSec * fileSampleRate);
    juce::int64 endSample = (juce::int64)(endSec * fileSampleRate);
    juce::int64 numSamples = endSample - startSample;

    // Copy just the crop region into our in-memory buffer
    audioBuffer.setSize(fileNumChannels, (int)numSamples);
    reader->read(&audioBuffer, 0, (int)numSamples, startSample, true, true);

    // Stop transport and swap in the new in-memory source
    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();

    memorySource = std::make_unique<juce::MemoryAudioSource>(audioBuffer, false);
    transportSource.setSource(memorySource.get(), 0, nullptr, fileSampleRate);

    // Reset crop markers now that they are baked in
    cropStart = -1.0;
    cropEnd = -1.0;

    double newLength = numSamples / fileSampleRate;
    juce::AccessibilityHandler::postAnnouncement(
        "Crop applied. New length is " + formatTime(newLength) +
        ". Press Space to play.",
        juce::AccessibilityHandler::AnnouncementPriority::high);

    repaint();
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

            if (!file.existsAsFile())
                return;

            currentFile = file;

            transportSource.stop();
            transportSource.setSource(nullptr);
            readerSource.reset();

            memorySource.reset();  // NEW � clear the old memory source if any
            audioBuffer.setSize(0, 0);  // NEW � clear stale buffer too

            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(file));

            if (reader == nullptr)
                return;

            // Update filter coefficients to match the file's sample rate
            /*   currentSampleRate = reader->sampleRate;
            updateFilterCoefficients();*/
            // Update filter coefficients to match the file's sample rate
            currentSampleRate = reader->sampleRate;
            fileSampleRate = reader->sampleRate;       // FIX: was never set on import
            fileNumChannels = (int)reader->numChannels; // FIX: was never set on import
            updateFilterCoefficients();
            updateEQCoefficients();
            auto newSource =
                std::make_unique<juce::AudioFormatReaderSource>(
                    reader.release(), true);

            transportSource.setSource(
                newSource.get(),
                0,
                nullptr,
                newSource->getAudioFormatReader()->sampleRate);

            readerSource.reset(newSource.release());
        });
}

//==============================================================================
void MainComponent::togglePlayback()
{
    if (readerSource == nullptr && transportSource.getLengthInSeconds() <= 0.0)
        return;

    if (transportSource.isPlaying())
        transportSource.stop();
    else
        transportSource.start();
}

void MainComponent::exportModifiedFile()
{
    // Export works from the in-memory buffer if a crop was applied,
    // otherwise falls back to reading from the original file
    bool hasMemoryBuffer = (audioBuffer.getNumSamples() > 0);

    if (!hasMemoryBuffer && (readerSource == nullptr || !currentFile.existsAsFile()))
        return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export modified WAV file",
        currentFile.getSiblingFile(
            currentFile.getFileNameWithoutExtension() + "_modified"),
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

            std::unique_ptr<juce::FileOutputStream> fileStream(
                outputFile.createOutputStream());
            if (fileStream == nullptr || !fileStream->openedOk()) return;

            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(
                    fileStream.release(),
                    fileSampleRate,
                    (unsigned int)fileNumChannels,
                    16, {}, 0));
            if (writer == nullptr) return;

            // Build offline filter chain matching live playback
            // Guard: fileSampleRate must have been set by importFile or applyCrop.
            // If it's still the default 44100 but the transport knows better, trust the transport.
            const double exportSR = (fileSampleRate > 0.0) ? fileSampleRate : currentSampleRate;
            juce::IIRFilter exportLow[2], exportHigh[2];
            auto lowCoeffs = juce::IIRCoefficients::makeLowShelf(exportSR, lowShelfFreq, 0.7, 0.25f);
            auto highCoeffs = juce::IIRCoefficients::makeHighShelf(exportSR, highShelfFreq, 0.7, 0.25f);



            // EQ filters for export
            juce::IIRFilter exportEQ[kNumEQBands][2];
            if (eqEnabled)
            {
                for (int b = 0; b < kNumEQBands; ++b)
                {
                    float lg = juce::Decibels::decibelsToGain((float)eqBands[b].gainDB);
                    auto c = juce::IIRCoefficients::makePeakFilter(
                        exportSR, eqBands[b].freq, eqBands[b].Q, lg);
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
                // Write from in-memory cropped buffer with gain + filters + fades applied
                juce::AudioBuffer<float> copy(audioBuffer);
                copy.applyGain(gain);

                double totalLength = (double)copy.getNumSamples() / fileSampleRate;

                for (int ch = 0; ch < copy.getNumChannels() && ch < 2; ++ch)
                {
                    float* data = copy.getWritePointer(ch);

                    if (lowPassEnabled)
                        exportLow[ch].processSamples(data, copy.getNumSamples());
                    if (highPassEnabled)
                        exportHigh[ch].processSamples(data, copy.getNumSamples());
                    if (eqEnabled)
                        for (int b = 0; b < kNumEQBands; ++b)
                            if (eqBands[b].enabled)
                                exportEQ[b][ch].processSamples(data, copy.getNumSamples());
                    if (fadeInEnabled || fadeOutEnabled)
                    {
                        for (int s = 0; s < copy.getNumSamples(); ++s)
                        {
                            double pos = (double)s / fileSampleRate;
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

                            data[s] *= fadeGain;
                        }
                    }

                }
                writer->writeFromAudioSampleBuffer(copy, 0, copy.getNumSamples());
            }
            else
            {
                // Write from original file with gain + filters + fades applied
                std::unique_ptr<juce::AudioFormatReader> reader(
                    formatManager.createReaderFor(currentFile));
                if (reader == nullptr) return;

                double totalLength = (double)reader->lengthInSamples / exportSR;

                const int bufferSize = 32768;
                juce::AudioBuffer<float> buffer(fileNumChannels, bufferSize);
                auto        totalSamples = reader->lengthInSamples;
                juce::int64 processed = 0;


                while (processed < totalSamples)
                {
                    int thisBlock = (int)juce::jmin(
                        (juce::int64)bufferSize, totalSamples - processed);
                    if (!reader->read(&buffer, 0, thisBlock, processed, true, true))
                        break;

                    buffer.applyGain(0, thisBlock, gain);

                    for (int ch = 0; ch < buffer.getNumChannels() && ch < 2; ++ch)
                    {
                        float* data = buffer.getWritePointer(ch);

                        if (lowPassEnabled)
                            exportLow[ch].processSamples(data, thisBlock);
                        if (highPassEnabled)
                            exportHigh[ch].processSamples(data, thisBlock);
                        if (eqEnabled)
                            for (int b = 0; b < kNumEQBands; ++b)
                                if (eqBands[b].enabled)
                                    exportEQ[b][ch].processSamples(data,thisBlock);
                        if (fadeInEnabled || fadeOutEnabled)
                        {
                            for (int s = 0; s < thisBlock; ++s)
                            {
                                double pos = (double)(processed + s) / exportSR;
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