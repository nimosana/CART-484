#include "MainComponent.h"

//==============================================================================
// Menu bar model: menu contents and dispatch of menu item selections.
//==============================================================================
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
        menu.addItem(4, transportSource.isPlaying()
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
        menu.addItem(5, "Toggle Gain Edit Mode            G", hasAudio, gainEditMode);
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
            juce::String inLabel = cropStart >= 0.0
                ? "Set Crop In Point (now " + formatTime(cropStart) + ")   I"
                : "Set Crop In Point...             I";
            juce::String outLabel = cropEnd >= 0.0
                ? "Set Crop Out Point (now " + formatTime(cropEnd) + ")  O"
                : "Set Crop Out Point...            O";
            menu.addItem(6, inLabel, hasAudio);
            menu.addItem(7, outLabel, hasAudio);
        }
        menu.addItem(8, "Apply Crop               Shift+C", cropStart >= 0.0 && cropEnd >= 0.0);
        menu.addSeparator();
        // History
        menu.addItem(9, "Undo                        Ctrl+Z", !undoStack.empty());
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
        effectsShortcuts.addItem(241, "Switch EQ Band: Shift + Left / Right");
        effectsShortcuts.addItem(242, "Adjust EQ Band Gain: Up / Down Arrow");
        effectsShortcuts.addItem(243, "Undo: Ctrl+Z");
        effectsShortcuts.addItem(244, "Redo: Ctrl+Y");
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
        logEffect("Fade in " + juce::String(!fadeInEnabled ? "on" : "off"));
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
        logEffect("Fade out " + juce::String(!fadeOutEnabled ? "on" : "off"));
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
            logEffect("Mid-fade on at " + formatTime(midFadeCenter));
            juce::AccessibilityHandler::postAnnouncement(
                "Mid-fade enabled at " + formatTime(midFadeCenter) + ". Select Edit Mid-Fade to adjust.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
        }
        else
        {
            midFadeEnabled = false;  midFadeEditMode = false;
            saveUndoState("Mid-fade off");
            logEffect("Mid-fade off");
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
        logEffect("Low shelf " + juce::String(!lowPassEnabled ? "on" : "off"));
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
        logEffect("High shelf " + juce::String(!highPassEnabled ? "on" : "off"));
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
            eqEditMode ? "EQ edit mode. Shift+Left and Right arrows to select band. Up and down arrows to adjust gain."
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

        // All Help reference items (201-244) and effect history (250, 300+) are read-only
    default: break;
    }
}
