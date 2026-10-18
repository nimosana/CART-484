#include "MainComponent.h"

//==============================================================================
// File I/O and transport: importing a WAV file, play/pause, and exporting the
// modified audio (gain/fades/EQ/crop baked in) back out to disk.
//==============================================================================
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
            if (!file.existsAsFile()) return;
            currentFile = file;
            transportSource.stop();
            transportSource.setSource(nullptr);
            readerSource.reset();
            memorySource.reset();
            sharedAudioBuffer.reset();
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
            if (reader == nullptr) return;
            currentSampleRate = reader->sampleRate;
            fileSampleRate = reader->sampleRate;
            fileNumChannels = (int)reader->numChannels;
            updateFilterCoefficients();
            updateEQCoefficients();
            auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
            transportSource.setSource(newSource.get(), 0, nullptr, newSource->getAudioFormatReader()->sampleRate);
            readerSource.reset(newSource.release());
            juce::AccessibilityHandler::postAnnouncement(currentFile.getFileName() + " loaded successfully.", juce::AccessibilityHandler::AnnouncementPriority::high);
        });
}

//==============================================================================
void MainComponent::togglePlayback()
{
    bool hasSource = (readerSource != nullptr) || (memorySource != nullptr);
    if (!hasSource) return;
    if (transportSource.isPlaying())
        transportSource.stop();
    else
        transportSource.start();
}

void MainComponent::exportModifiedFile()
{
    bool hasMemoryBuffer = (sharedAudioBuffer != nullptr && sharedAudioBuffer->getNumSamples() > 0);
    if (!hasMemoryBuffer && (readerSource == nullptr || !currentFile.existsAsFile()))
        return;
    auto chooser = std::make_shared<juce::FileChooser>(
        "Export modified WAV file",
        currentFile.getSiblingFile(currentFile.getFileNameWithoutExtension() + "_modified"),
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
            std::unique_ptr<juce::FileOutputStream> fileStream(outputFile.createOutputStream());
            if (fileStream == nullptr || !fileStream->openedOk()) return;
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(fileStream.release(), fileSampleRate, (unsigned int)fileNumChannels, 16, {}, 0));
            if (writer == nullptr) return;

            const double exportSR = (fileSampleRate > 0.0) ? fileSampleRate : currentSampleRate;
            juce::IIRFilter exportLow[2], exportHigh[2];
            auto lowCoeffs = juce::IIRCoefficients::makeLowShelf(exportSR, lowShelfFreq, 0.7, 0.25f);
            auto highCoeffs = juce::IIRCoefficients::makeHighShelf(exportSR, highShelfFreq, 0.7, 0.25f);
            juce::IIRFilter exportEQ[kNumEQBands][2];
            if (eqEnabled)
            {
                for (int b = 0; b < kNumEQBands; ++b)
                {
                    float lg = juce::Decibels::decibelsToGain((float)eqBands[b].gainDB);
                    auto c = juce::IIRCoefficients::makePeakFilter(exportSR, eqBands[b].freq, eqBands[b].Q, lg);
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
                juce::AudioBuffer<float> copy(*sharedAudioBuffer);
                copy.applyGain(gain);
                double totalLength = (double)copy.getNumSamples() / fileSampleRate;
                for (int ch = 0; ch < copy.getNumChannels() && ch < 2; ++ch)
                {
                    float* data = copy.getWritePointer(ch);
                    if (lowPassEnabled) exportLow[ch].processSamples(data, copy.getNumSamples());
                    if (highPassEnabled) exportHigh[ch].processSamples(data, copy.getNumSamples());
                    if (eqEnabled)
                        for (int b = 0; b < kNumEQBands; ++b)
                            if (eqBands[b].enabled) exportEQ[b][ch].processSamples(data, copy.getNumSamples());
                    if (fadeInEnabled || fadeOutEnabled)
                    {
                        for (int s = 0; s < copy.getNumSamples(); ++s)
                        {
                            double pos = (double)s / fileSampleRate;
                            float  fadeGain = 1.0f;
                            if (fadeInEnabled && fadeInDuration > 0.0)
                                fadeGain *= juce::jlimit(0.0f, 1.0f, (float)(pos / fadeInDuration));
                            if (fadeOutEnabled && fadeOutDuration > 0.0)
                                fadeGain *= juce::jlimit(0.0f, 1.0f, (float)((totalLength - pos) / fadeOutDuration));
                            if (midFadeEnabled && midFadeDuration > 0.0)
                            {
                                double halfDip = midFadeDuration * 0.5;
                                double dipStart = midFadeCenter - halfDip;
                                double dipEnd = midFadeCenter + halfDip;
                                if (pos >= dipStart && pos < midFadeCenter)
                                {
                                    double t = (midFadeCenter - pos) / halfDip;
                                    fadeGain *= (float)(0.5 * (1.0 - std::cos(juce::MathConstants<double>::pi * t)));
                                }
                                else if (pos >= midFadeCenter && pos <= dipEnd)
                                {
                                    double t = (pos - midFadeCenter) / halfDip;
                                    fadeGain *= (float)(0.5 * (1.0 - std::cos(juce::MathConstants<double>::pi * t)));
                                }
                            }
                            data[s] *= fadeGain;
                        }
                    }
                }
                writer->writeFromAudioSampleBuffer(copy, 0, copy.getNumSamples());
            }
            else
            {
                std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(currentFile));
                if (reader == nullptr) return;
                double totalLength = (double)reader->lengthInSamples / exportSR;
                const int bufferSize = 32768;
                juce::AudioBuffer<float> buffer(fileNumChannels, bufferSize);
                auto        totalSamples = reader->lengthInSamples;
                juce::int64 processed = 0;
                while (processed < totalSamples)
                {
                    int thisBlock = (int)juce::jmin((juce::int64)bufferSize, totalSamples - processed);
                    if (!reader->read(&buffer, 0, thisBlock, processed, true, true)) break;
                    buffer.applyGain(0, thisBlock, gain);
                    for (int ch = 0; ch < buffer.getNumChannels() && ch < 2; ++ch)
                    {
                        float* data = buffer.getWritePointer(ch);
                        if (lowPassEnabled) exportLow[ch].processSamples(data, thisBlock);
                        if (highPassEnabled) exportHigh[ch].processSamples(data, thisBlock);
                        if (eqEnabled)
                            for (int b = 0; b < kNumEQBands; ++b)
                                if (eqBands[b].enabled) exportEQ[b][ch].processSamples(data, thisBlock);
                        if (fadeInEnabled || fadeOutEnabled)
                        {
                            for (int s = 0; s < thisBlock; ++s)
                            {
                                double pos = (double)(processed + s) / exportSR;
                                float  fadeGain = 1.0f;
                                if (fadeInEnabled && fadeInDuration > 0.0)
                                    fadeGain *= juce::jlimit(0.0f, 1.0f, (float)(pos / fadeInDuration));
                                if (fadeOutEnabled && fadeOutDuration > 0.0)
                                    fadeGain *= juce::jlimit(0.0f, 1.0f, (float)((totalLength - pos) / fadeOutDuration));
                                if (midFadeEnabled && midFadeDuration > 0.0)
                                {
                                    double halfDip = midFadeDuration * 0.5;
                                    double dipStart = midFadeCenter - halfDip;
                                    double dipEnd = midFadeCenter + halfDip;
                                    if (pos >= dipStart && pos < midFadeCenter)
                                    {
                                        double t = (midFadeCenter - pos) / halfDip;
                                        fadeGain *= (float)(0.5 * (1.0 - std::cos(juce::MathConstants<double>::pi * t)));
                                    }
                                    else if (pos >= midFadeCenter && pos <= dipEnd)
                                    {
                                        double t = (pos - midFadeCenter) / halfDip;
                                        fadeGain *= (float)(0.5 * (1.0 - std::cos(juce::MathConstants<double>::pi * t)));
                                    }
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