#include "MainComponent.h"

//==============================================================================
// Undo/redo: snapshotting and restoring the full editable state of the app.
//==============================================================================
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
    s.midFadeEnabled = midFadeEnabled;
    s.midFadeCenter = midFadeCenter;
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
    midFadeEnabled = s.midFadeEnabled;
    midFadeCenter = s.midFadeCenter;
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
