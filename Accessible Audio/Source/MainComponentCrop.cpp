#include "MainComponent.h"

//==============================================================================
// Crop tool: the "set start/end time" dialog and applying the crop to the
// in-memory audio buffer.
//==============================================================================
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
    double endSec = (cropEnd >= 0.0) ? cropEnd : length;
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
    juce::int64 endSample = (juce::int64)(endSec * fileSampleRate);
    juce::int64 numSamples = endSample - startSample;
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
