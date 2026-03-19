#pragma once
#include <JuceHeader.h>

// A simple dialog that lets the user type in a timestamp (MM:SS or seconds)
// and returns the result in seconds via a callback.
class CropDialog : public juce::Component
{
public:
    // label:    what this dialog is asking for, e.g. "Set crop start time"
    // current:  pre-fills the text box with this value in seconds
    // onConfirm: called with the parsed seconds when user presses Enter/OK
    CropDialog(const juce::String& label,
               double current,
               std::function<void(double)> onConfirm)
        : confirmCallback(std::move(onConfirm))
    {
        setSize(320, 120);

        // Label
        titleLabel.setText(label, juce::dontSendNotification);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        // Pre-fill with current time in MM:SS
        int mins = (int)(current / 60);
        int secs = (int)(current) % 60;
        textEditor.setText(juce::String(mins) + ":" +
                           juce::String(secs).paddedLeft('0', 2));
        textEditor.setSelectAllWhenFocused(true);

        // IMPORTANT for screen readers: give the editor an accessible title
        textEditor.setTitle(label);
        textEditor.setAccessible(true);

        // Announce the dialog opening
        juce::AccessibilityHandler::postAnnouncement(
            label + ". Enter time as minutes colon seconds, or just seconds. Press Enter to confirm.",
            juce::AccessibilityHandler::AnnouncementPriority::high);

        // Confirm on Enter key
        textEditor.onReturnKey = [this]() { confirm(); };

        addAndMakeVisible(textEditor);

        // OK button
        okButton.setButtonText("OK");
        okButton.onClick = [this]() { confirm(); };
        okButton.setAccessible(true);
        addAndMakeVisible(okButton);

        // Cancel button
        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this]()
        {
            juce::AccessibilityHandler::postAnnouncement(
                "Cancelled.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            if (auto* parent = getParentComponent())
                parent->removeChildComponent(this);
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState(0);
        };
        cancelButton.setAccessible(true);
        addAndMakeVisible(cancelButton);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        titleLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(6);
        textEditor.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
        auto buttonRow = area.removeFromTop(30);
        okButton    .setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(4, 0));
        cancelButton.setBounds(buttonRow.reduced(4, 0));
    }

    // Call this after adding to screen so focus lands on the text box,
    // which causes screen readers to read the field label out loud.
    void grabFocusOnOpen()
    {
        textEditor.grabKeyboardFocus();
    }

private:
    void confirm()
    {
        double parsed = parseTime(textEditor.getText());
        if (parsed < 0.0)
        {
            // Invalid input — tell the user
            juce::AccessibilityHandler::postAnnouncement(
                "Invalid time format. Please enter minutes colon seconds, for example 1:30, or total seconds.",
                juce::AccessibilityHandler::AnnouncementPriority::high);
            return;
        }

        if (confirmCallback)
            confirmCallback(parsed);

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    }

    // Accepts "1:30" (1 min 30 sec) or "90" (plain seconds)
    static double parseTime(const juce::String& text)
    {
        auto trimmed = text.trim();
        if (trimmed.contains(":"))
        {
            auto mins = trimmed.upToFirstOccurrenceOf(":", false, false).getDoubleValue();
            auto secs = trimmed.fromFirstOccurrenceOf(":", false, false).getDoubleValue();
            if (mins < 0 || secs < 0 || secs >= 60) return -1.0;
            return mins * 60.0 + secs;
        }
        else
        {
            double val = trimmed.getDoubleValue();
            if (val < 0) return -1.0;
            return val;
        }
    }

    juce::Label      titleLabel;
    juce::TextEditor textEditor;
    juce::TextButton okButton, cancelButton;
    std::function<void(double)> confirmCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CropDialog)
};
