#include "MainComponent.h"

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
void MainComponent::toggleEq() {
	eqEnabled = !eqEnabled;
	juce::AccessibilityHandler::postAnnouncement(
		eqEnabled ? "EQ on. Press Ctrl + E to enter EQ band editing mode." : "EQ off.",
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