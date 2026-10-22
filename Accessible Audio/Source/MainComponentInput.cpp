#include "MainComponent.h"
//using namespace MainComponent;

//==============================================================================
// Keyboard input: this app is keyboard/screen-reader driven, so keyPressed()
// is the primary interaction surface. Mouse-drag EQ editing was prototyped
// but is currently disabled in favour of the keyboard workflow (left commented
// out below rather than deleted, in case it is revisited).
//==============================================================================
//==============================================================================
//void MainComponent::mouseDown(const juce::MouseEvent& e)
//{
//    //grabKeyboardFocus();
//    //if (eqSlidersArea.contains(e.getPosition()))
//    //{
//    //    int axisW = 28;
//    //    int trackLeft = eqSlidersArea.getX() + axisW;
//    //    int trackAreaWidth = eqSlidersArea.getWidth() - axisW;
//    //    int bandW = trackAreaWidth / kNumEQBands;
//    //    int x = e.getPosition().getX();
//    //    int b = (x - trackLeft) / bandW;
//    //    if (b >= 0 && b < kNumEQBands)
//    //    {
//    //        eqSelectedBand = b;
//    //        eqEditMode = true; // Auto select & enter edit mode
//    //        gainEditMode = fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = false;
//    //        mouseDrag(e); // apply initial gain change
//    //        juce::AccessibilityHandler::postAnnouncement(
//    //            "EQ band " + eqBands[b].name + " selected.",
//    //            juce::AccessibilityHandler::AnnouncementPriority::high);
//    //    }
//    //}
//}

//void MainComponent::mouseDrag(const juce::MouseEvent& e)
//{
//    if (eqSlidersArea.contains(e.getPosition()) || eqEditMode)
//    {
//        int labelTopH = 18;
//        int labelBotH = 16;
//        int trackTop = eqSlidersArea.getY() + labelTopH;
//        int trackH = eqSlidersArea.getHeight() - labelTopH - labelBotH;
//        int y = e.getPosition().getY();
//        y = juce::jlimit(trackTop, trackTop + trackH, y);
//        float frac = (float)(y - trackTop) / (float)trackH;
//        double db = (0.5 - frac) * 24.0;
//        if (eqSelectedBand >= 0 && eqSelectedBand < kNumEQBands)
//        {
//            eqBands[eqSelectedBand].gainDB = juce::jlimit(-12.0, 12.0, db);
//            eqBands[eqSelectedBand].enabled = true; // auto enable when touched
//            updateEQCoefficients();
//            repaint();
//        }
//    }
//}

//==============================================================================
bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
	char c = (char)std::tolower(key.getTextCharacter());
	bool ctrlDown = key.getModifiers().isCtrlDown();
	bool altDown = key.getModifiers().isAltDown();
	bool shiftDown = key.getModifiers().isShiftDown();
	int kc = key.getKeyCode();
	bool isCommand = false;
	if (kc == 'g' || kc == 'G' || kc == 'e' || kc == 'E' ||
		kc == 'q' || kc == 'Q' || kc == 'w' || kc == 'W' ||
		kc == 'l' || kc == 'L' || kc == 'h' || kc == 'H' ||
		kc == 'm' || kc == 'M' || kc == 's' || kc == 'S')
	{
		isCommand = true;
	}
	if (checkIfOpenFile(isCommand, altDown)) return true;


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
			toggleGainEditMode();
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
		else if (c == 'c' && shiftDown)
		{
			applyCrop();
			return true;
		}
		if (c == 'l')
		{
			toggleLowShelf();
			return true;
		}
		else if (c == 'h' && !altDown)
		{
			toggleHighShelf();
			return true;
		}
		else if (c == 'q')
		{
			toggleFadeIn();
			return true;
		}
		else if (c == 'w')
		{
			toggleFadeOut();
			return true;
		}
		else if (c == 'm' && !ctrlDown)
		{
			toggleMidFade();
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
			toggleLowShelfEdit();
			return true;
		}
		if (key.getKeyCode() == 'e' || key.getKeyCode() == 'E')
		{
			toggleEqEdit();
			return true;
		}
		// Ctrl+H: enter/exit high shelf frequency edit mode
		if (key.getKeyCode() == 'h' || key.getKeyCode() == 'H')
		{
			toggleHighShelfEdit();
			return true;
		}
		// Ctrl+Q: enter/exit fade-in duration edit mode
		if (key.getKeyCode() == 'q' || key.getKeyCode() == 'Q')
		{
			toggleFadeInEdit();
			return true;
		}
		// Ctrl+W: enter/exit fade-out duration edit mode
		if (key.getKeyCode() == 'w' || key.getKeyCode() == 'W')
		{
			toggleFadeOutEdit();
			return true;
		}
		// Ctrl+M: enter/exit mid-fade edit mode
		if (key.getKeyCode() == 'm' || key.getKeyCode() == 'M')
		{
			toggleMidFadeEdit();
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

	// ── EQ Edit Mode: Shift+Left/Right switches bands, Up/Down adjusts gain ──
	// These are consumed first so they don't fall through to scrub or mid-fade.
	if (eqEditMode)
	{
		if (shiftDown && !ctrlDown &&
			(key.getKeyCode() == juce::KeyPress::rightKey || key.getKeyCode() == juce::KeyPress::leftKey))
		{
			bool isRight = (key.getKeyCode() == juce::KeyPress::rightKey);
			eqSelectedBand = (eqSelectedBand + (isRight ? 1 : kNumEQBands - 1)) % kNumEQBands;
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

			// Ctrl+Up/Down → adjust Q (bandwidth); plain Up/Down → adjust gain
			if (ctrlDown)
			{
				double step = shiftDown ? 0.5 : 0.1;
				double& q = eqBands[eqSelectedBand].Q;
				q = juce::jlimit(0.3, 10.0, q + (up ? step : -step));
				updateEQCoefficients();
				juce::String msg;
				msg << eqBands[eqSelectedBand].name
					<< " Q: " << juce::String(q, 2)
					<< (q < 0.7 ? " (wide)" : q > 2.0 ? " (narrow)" : "");
				juce::AccessibilityHandler::postAnnouncement(msg, juce::AccessibilityHandler::AnnouncementPriority::high);
				repaint();
				return true;
			}

			double step = shiftDown ? 3.0 : 1.0;
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

	// Left/Right scrubbing — only when Shift is NOT held (to avoid conflicting with edit modes above)
	if (!shiftDown)
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

void MainComponent::toggleGainEditMode()
{
	bool anyOther = fadeInEditMode || fadeOutEditMode || lowFreqEditMode || highFreqEditMode || eqEditMode;
	if (anyOther)
	{
		fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
		gainEditMode = true;
	}
	else {
		gainEditMode = !gainEditMode;
	}
	saveUndoState("Gain edit mode " + juce::String(gainEditMode ? "enabled" : "disabled"));
	logEffect("Gain edit mode " + juce::String(gainEditMode ? "enabled" : "disabled"));
	juce::AccessibilityHandler::postAnnouncement(
		gainEditMode ? "Gain edit mode enabled. Use up and down arrow keys to edit gain." : "Gain edit mode disabled.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	repaint();
}

void MainComponent::toggleLowShelf() {
	saveUndoState("Low shelf filter " + juce::String(!lowPassEnabled ? "on" : "off"));
	logEffect("Low shelf filter " + juce::String(!lowPassEnabled ? "on" : "off"));
	lowPassEnabled = !lowPassEnabled;
	juce::AccessibilityHandler::postAnnouncement(lowPassEnabled
		? "Low shelf filter on. " + juce::String((int)lowShelfFreq) + " Hz. Press Ctrl + L to edit frequencies."
		: "Low shelf filter off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	lowShelfFilter[0].reset();
	lowShelfFilter[1].reset();
	repaint();
}
void MainComponent::toggleLowShelfEdit() {
	bool anyOther = gainEditMode || fadeInEditMode || fadeOutEditMode ||
		highFreqEditMode || eqEditMode;
	bool targetState = anyOther ? true : !lowFreqEditMode;
	if (targetState && !lowPassEnabled)
	{
		juce::AccessibilityHandler::postAnnouncement("Low shelf filter is off. Press L to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
		return;
	}
	gainEditMode = fadeInEditMode = fadeOutEditMode = highFreqEditMode = eqEditMode = false;
	lowFreqEditMode = targetState;
	juce::AccessibilityHandler::postAnnouncement(
		lowFreqEditMode ? "Low shelf frequency edit mode. Use up and down arrows to change frequency. Currently " + juce::String((int)lowShelfFreq) + " Hz." : "Low shelf frequency edit mode off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	repaint();
}
void MainComponent::toggleHighShelf() {
	saveUndoState("High shelf filter " + juce::String(!highPassEnabled ? "on" : "off"));
	logEffect("High shelf filter " + juce::String(!highPassEnabled ? "on" : "off"));
	highPassEnabled = !highPassEnabled;
	juce::AccessibilityHandler::postAnnouncement(highPassEnabled
		? "High shelf filter on. " + juce::String((int)highShelfFreq) + " Hz. Press Ctrl + H to edit frequencies."
		: "High shelf filter off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	highShelfFilter[0].reset();
	highShelfFilter[1].reset();
	repaint();
}
void MainComponent::toggleHighShelfEdit() {
	bool anyOther = gainEditMode || fadeInEditMode || fadeOutEditMode ||
		lowFreqEditMode || eqEditMode;
	bool targetState = anyOther ? true : !highFreqEditMode;
	if (targetState && !highPassEnabled)
	{
		juce::AccessibilityHandler::postAnnouncement("High shelf filter is off. Press H to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
		return;
	}
	gainEditMode = lowFreqEditMode = fadeInEditMode = fadeOutEditMode = eqEditMode = false;
	highFreqEditMode = targetState;
	juce::AccessibilityHandler::postAnnouncement(
		highFreqEditMode ? "High shelf frequency edit mode. Use up and down arrows to change frequency. Currently " + juce::String((int)highShelfFreq) + " Hz." : "High shelf frequency edit mode off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	repaint();
}
void MainComponent::toggleFadeIn() {
	saveUndoState("Fade in " + juce::String(!fadeInEnabled ? "on" : "off"));
	logEffect("Fade in " + juce::String(!fadeInEnabled ? "on" : "off"));
	fadeInEnabled = !fadeInEnabled;
	juce::AccessibilityHandler::postAnnouncement(
		fadeInEnabled
		? "Fade in on. " + juce::String(fadeInDuration, 1) + " seconds. Press Ctrl + Q to edit duration."
		: "Fade in off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	repaint();
}
void MainComponent::toggleFadeInEdit() {
	bool anyOther = gainEditMode || fadeOutEditMode ||
		lowFreqEditMode || highFreqEditMode || eqEditMode;
	bool targetState = anyOther ? true : !fadeInEditMode;
	if (targetState && !fadeInEnabled)
	{
		juce::AccessibilityHandler::postAnnouncement("Fade in is off. Press Q to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
		return;
	}
	gainEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
	fadeInEditMode = targetState;
	juce::AccessibilityHandler::postAnnouncement(
		fadeInEditMode ? "Fade in duration edit mode. Use up and down arrows to change duration. Currently " + juce::String(fadeInDuration, 1) + " seconds." : "Fade in duration edit mode off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	repaint();
}

void MainComponent::toggleFadeOut() {
	saveUndoState("Fade out " + juce::String(!fadeOutEnabled ? "on" : "off"));
	logEffect("Fade out " + juce::String(!fadeOutEnabled ? "on" : "off"));
	fadeOutEnabled = !fadeOutEnabled;
	juce::AccessibilityHandler::postAnnouncement(
		fadeOutEnabled
		? "Fade out on. " + juce::String(fadeOutDuration, 1) + " seconds. Press Ctrl + W to edit duration."
		: "Fade out off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	repaint();
}
void MainComponent::toggleFadeOutEdit() {
	bool anyOther = gainEditMode || fadeInEditMode ||
		lowFreqEditMode || highFreqEditMode || eqEditMode;
	bool targetState = anyOther ? true : !fadeOutEditMode;
	if (targetState && !fadeOutEnabled)
	{
		juce::AccessibilityHandler::postAnnouncement("Fade out is off. Press W to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
		return;
	}
	gainEditMode = fadeInEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
	fadeOutEditMode = targetState;
	juce::AccessibilityHandler::postAnnouncement(
		fadeOutEditMode ? "Fade out duration edit mode. Use up and down arrows to change duration. Currently " + juce::String(fadeOutDuration, 1) + " seconds." : "Fade out duration edit mode off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	repaint();
}
void MainComponent::toggleMidFade() {
	double pos = transportSource.getCurrentPosition();
	if (!midFadeEnabled)
	{
		midFadeCenter = pos;
		midFadeEnabled = true;
		saveUndoState("Mid-fade on at " + formatTime(midFadeCenter));
		logEffect("Mid-fade on at " + formatTime(midFadeCenter));
		juce::AccessibilityHandler::postAnnouncement(
			"Mid-fade enabled at " + formatTime(midFadeCenter) + ". Press Ctrl + M to edit.",
			juce::AccessibilityHandler::AnnouncementPriority::high);
	}
	else
	{
		midFadeEnabled = false;
		midFadeEditMode = false;
		saveUndoState("Mid-fade off");
		logEffect("Mid-fade off");
		juce::AccessibilityHandler::postAnnouncement("Mid-fade off.", juce::AccessibilityHandler::AnnouncementPriority::high);
	}
	repaint();
}
void MainComponent::toggleMidFadeEdit() {
	if (!midFadeEditMode && !midFadeEnabled)
	{
		juce::AccessibilityHandler::postAnnouncement("Mid-fade is off. Press M at the desired position to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
		return;
	}
	gainEditMode = fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = eqEditMode = false;
	midFadeEditMode = !midFadeEditMode;
	juce::AccessibilityHandler::postAnnouncement(
		midFadeEditMode
		? "Mid-fade edit mode. Shift + Left and Right arrows adjust position by 0.1 seconds. Ctrl + Shift + Left and Right arrows adjust position by 1 second. Up and Down arrows change duration. Centre at " + formatTime(midFadeCenter) + ", duration " + juce::String(midFadeDuration, 1) + " seconds."
		: "Mid-fade edit mode off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	repaint();
}
void MainComponent::toggleEqEdit() {
	bool anyOther = gainEditMode || fadeInEditMode || fadeOutEditMode ||
		lowFreqEditMode || highFreqEditMode;
	bool targetState = anyOther ? true : !eqEditMode;
	if (targetState && !eqEnabled)
	{
		juce::AccessibilityHandler::postAnnouncement("EQ is off. Press E to enable it first.", juce::AccessibilityHandler::AnnouncementPriority::high);
		return;
	}
	gainEditMode = fadeInEditMode = fadeOutEditMode = lowFreqEditMode = highFreqEditMode = false;
	eqEditMode = targetState;
	juce::AccessibilityHandler::postAnnouncement(
		eqEditMode ? "EQ edit mode. Shift+Left and Right arrows to select band. Up and down arrows to adjust gain." : "EQ edit mode off.",
		juce::AccessibilityHandler::AnnouncementPriority::high);
	repaint();
}

bool MainComponent::checkIfOpenFile(bool isCommand, bool altDown) {
	if (isCommand && !altDown && readerSource == nullptr && memorySource == nullptr && transportSource.getLengthInSeconds() <= 0.0)
	{
		juce::AccessibilityHandler::postAnnouncement(
			"No audio file loaded. Press Ctrl + O to open a .wav file.",
			juce::AccessibilityHandler::AnnouncementPriority::high);
		return true;
	}
	else return false;
}