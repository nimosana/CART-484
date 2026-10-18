#include "MainComponent.h"

//==============================================================================
// Screen-reader announcements, time formatting, and the effect-history log
// used by the "Effect History" help submenu.
//==============================================================================
//==============================================================================
// Static helper  used in paint() and announcements
juce::String MainComponent::formatTime(double seconds)
{
    int mins = (int)(seconds / 60);
    int secs = (int)(seconds) % 60;
    return juce::String(mins) + " minutes " + juce::String(secs) + " seconds";
}

void MainComponent::announceTime()
{
    double currentPos = transportSource.getCurrentPosition();
    juce::String msg = formatTime(currentPos);
    juce::AccessibilityHandler::postAnnouncement(msg, juce::AccessibilityHandler::AnnouncementPriority::high);
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

void MainComponent::logEffect(const juce::String& description)
{
    juce::Time now = juce::Time::getCurrentTime();
    juce::String entry = now.toString(false, true, true, false) + "  —  " + description;
    effectHistory.insert(0, entry);
    if (effectHistory.size() > 50)
        effectHistory.remove(effectHistory.size() - 1);
}
