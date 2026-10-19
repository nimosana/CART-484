#include "MainComponent.h"

//==============================================================================
// Construction / destruction
//==============================================================================
//==============================================================================
MainComponent::MainComponent()
{
	juce::LookAndFeel::setDefaultLookAndFeel(&darkLAF);
	setLookAndFeel(&darkLAF);
	setSize(600, 800);  // Slightly wider for better proportions
	formatManager.registerBasicFormats();

	menuBar = std::make_unique<AccessibleMenuBar>(this, this, &darkLAF);
	addAndMakeVisible(menuBar.get());
	if (menuBar) {
		menuBar->setBounds(0, 0, getWidth(), 30); // Slightly taller for 20pt text
	}
	setWantsKeyboardFocus(true);
	addKeyListener(this);
	juce::MessageManager::callAsync([this]() { grabKeyboardFocus(); });
	setAudioChannels(0, 2);

	// ---- Gamepad mapping ----
	gamepad.onButtonDown = [this](SDL_GameControllerButton b)
		{
			switch (b)
			{
			case SDL_CONTROLLER_BUTTON_A: // Cross
				togglePlayback();
				gamepad.tick();
				break;
			case SDL_CONTROLLER_BUTTON_X: // Square   
				announceTime();
				break;
			case SDL_CONTROLLER_BUTTON_Y: // Triangle        
				announceEffectsStatus();
				break;
			case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: // L1
				performUndo();
				break;
			case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: // R1
				performRedo();
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
			{
				double currentPos = transportSource.getCurrentPosition();
				double newPos = currentPos - 5.0;

				if (newPos < 0.0)
				{
					transportSource.setPosition(0.0);
					gamepad.doubleTick();
				}
				else
				{
					transportSource.setPosition(newPos);
					gamepad.tick();
				}
				break;
			}

			case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
			{
				double currentPos = transportSource.getCurrentPosition();
				double newPos = currentPos + 5.0;
				double length = transportSource.getLengthInSeconds();

				if (length > 0.0 && newPos >= length)
				{
					transportSource.setPosition(length);
					gamepad.doubleTick();
				}
				else
				{
					transportSource.setPosition(newPos);
					gamepad.tick();
				}
				break;
			}
			case SDL_CONTROLLER_BUTTON_START:
				openCropDialog(true);
				break;
			default: break;
			}
		};

	startTimerHz(60);
}

MainComponent::~MainComponent()
{
	gamepad.onButtonDown = nullptr;   // new: no callbacks during teardown

	// 1. Stop the audio first so no callbacks try to repaint while we are dying
	shutdownAudio();

	// 2. IMPORTANT: Explicitly delete the menuBar right now.
	// This forces the OwnedArray of buttons to be destroyed immediately,
	// releasing their references to the LookAndFeel.
	menuBar.reset();

	// 3. Tell the global system to stop using this LAF
	juce::LookAndFeel::setDefaultLookAndFeel(nullptr);

	// 4. Tell all remaining children (Labels, Sliders, etc.) to switch to the default
	sendLookAndFeelChange();

	// 5. Finally, clear the MainComponent's own LAF
	setLookAndFeel(nullptr);
}

//==============================================================================
// Painting
//==============================================================================
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
	// Dynamically divide remaining height equally between the curve and the EQ
	// section, so both resize naturally when the window is resized / full-screened.
	{
		// Fixed-height sections that sit between the curve and the EQ:
		//   gap(14) + timeline(75) + gap(14) + effects-grid(3*72+2*12=240)
		//   + gap(14) + crop(70) + gap(14) + eq-header(30+6+6) = 477
		const int kFixedBetween = 14 + 75 + 14 + (3 * 72 + 2 * 12) + 14 + 70 + 14 + 30 + 12;
		const int kMinCurve = 90;
		const int kMinEQ = 110;
		int totalFlex = juce::jmax(kMinCurve + kMinEQ,
			bounds.getHeight() - kFixedBetween);
		// Store the computed heights so the EQ section can use them later.
		// We write them into local vars; the EQ section below reads eqFlexH.
		eqFlexCurveH = juce::jmax(kMinCurve, totalFlex / 2);
		eqFlexH = juce::jmax(kMinEQ, totalFlex - eqFlexCurveH);
	}
	auto curveBounds = bounds.removeFromTop(eqFlexCurveH);
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
			if (lowPassEnabled) { double ratio = freq / juce::jmax(1.0, lowShelfFreq);  db -= 12.0 / (1.0 + std::pow(ratio, 4.0)); }
			if (highPassEnabled) { double ratio = juce::jmax(1.0, highShelfFreq) / freq; db -= 12.0 / (1.0 + std::pow(ratio, 4.0)); }
			if (eqEnabled)
			{
				for (int b = 0; b < kNumEQBands; ++b)
				{
					if (!eqBands[b].enabled || eqBands[b].freq <= 0.0) continue;
					double ratio = freq / eqBands[b].freq;
					if (ratio <= 0.0) continue;
					double logRatio = std::log(ratio);
					double q = juce::jmax(0.1, eqBands[b].Q);
					// Gaussian approximation of a peaking filter; Q scales width
					db += eqBands[b].gainDB * std::exp(-logRatio * logRatio * q * q * 4.0);
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
		fillPath.lineTo((float)curveBounds.getX(), (float)curveBounds.getBottom());
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
		EffectInfo{ "Gain (G)",       juce::String(gain, 2) + "x" + (gainEditMode ? " | ↑↓" : " | G"),       std::abs(gain - 1.0f) > 0.01f, gainEditMode,       juce::Colour(DarkLookAndFeel::accentOrange) },
		EffectInfo{
			"Mid Fade (M)",
			juce::String(midFadeEnabled ? "ON" : "off") +
			" | Pos: " + juce::String(midFadeCenter, 1) + "s" +
			" | Dur: " + juce::String(midFadeDuration, 1) + "s" +
			(midFadeEditMode ? " (Editing)" : ""),
			midFadeEnabled,
			midFadeEditMode,
			juce::Colour(DarkLookAndFeel::accentPink)
		},
		EffectInfo{ "Fade In (Q)",    juce::String(fadeInEnabled ? "ON" : "off") + "  " + juce::String(fadeInDuration, 1) + "s" + (fadeInEditMode ? " | ↑↓" : " | Ctrl+Q"), fadeInEnabled, fadeInEditMode,  juce::Colour(DarkLookAndFeel::accentYellow) },
		EffectInfo{ "Fade Out (W)",   juce::String(fadeOutEnabled ? "ON" : "off") + "  " + juce::String(fadeOutDuration, 1) + "s" + (fadeOutEditMode ? " | ↑↓" : " | Ctrl+W"), fadeOutEnabled, fadeOutEditMode, juce::Colour(DarkLookAndFeel::accentGold) },
		EffectInfo{ "Low Shelf (L)",  juce::String(lowPassEnabled ? "ON" : "off") + "  " + juce::String((int)lowShelfFreq) + "Hz" + (lowFreqEditMode ? " | ↑↓" : " | Ctrl+L"), lowPassEnabled, lowFreqEditMode,  juce::Colour(DarkLookAndFeel::accentBlue) },
		EffectInfo{ "High Shelf (H)", juce::String(highPassEnabled ? "ON" : "off") + "  " + juce::String((int)highShelfFreq) + "Hz" + (highFreqEditMode ? " | ↑↓" : " | Ctrl+H"), highPassEnabled, highFreqEditMode, juce::Colour(DarkLookAndFeel::accentCyan) }
	};

	for (int row = 0; row < 3; ++row)
	{
		int rowY = gridArea.getY() + row * (boxH + rowGap);
		juce::Rectangle<int> leftBox(gridArea.getX(), rowY, colW, boxH);
		juce::Rectangle<int> rightBox(gridArea.getX() + colW + colGap, rowY, gridArea.getWidth() - colW - colGap, boxH);
		int i0 = row * 2, i1 = row * 2 + 1;
		if (i0 < 6) drawEffectBox(g, leftBox, effects[i0].title, effects[i0].status, effects[i0].active, effects[i0].edit, effects[i0].accent);
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
	juce::String inStr = (cropStart >= 0.0) ? formatTime(cropStart) : "--";
	juce::String outStr = (cropEnd >= 0.0) ? formatTime(cropEnd) : "--";

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
	// eqFlexH was computed above so both the curve and EQ expand proportionally.
	auto eqBounds = bounds.removeFromTop(juce::jmax(110, juce::jmin(bounds.getHeight(), eqFlexH)));
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
	eqStat += eqEditMode
		? ("   Shift+L/R=band  Up/Down=gain  Ctrl+Up/Down=Q  Q:" + juce::String(eqBands[eqSelectedBand].Q, 2))
		: "   Ctrl+E to edit";

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
	repaint(); // Recompute dynamic curve / EQ heights
}
