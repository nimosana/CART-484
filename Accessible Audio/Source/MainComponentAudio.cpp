#include "MainComponent.h"

//==============================================================================
// Playback engine: filter coefficient updates and the AudioAppComponent
// callbacks that stream audio, apply gain/fades/filters, and mix in the EQ.
//==============================================================================
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

    // Apply fade in / fade out / mid-fade as per-sample linear ramps.
    // getCurrentPosition() returns the playhead AFTER the block was consumed,
    // so the block started one block-duration earlier.
    if ((fadeInEnabled || fadeOutEnabled || midFadeEnabled) && currentSampleRate > 0.0)
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
                if (midFadeEnabled && midFadeDuration > 0.0)
                {
                    double halfDip = midFadeDuration * 0.5;
                    double dipStart = midFadeCenter - halfDip;
                    double dipEnd = midFadeCenter + halfDip;
                    if (pos >= dipStart && pos < midFadeCenter)
                    {
                        // Fade-out: raised-cosine from 1→0.  Stays near silence
                        // longer than a linear ramp, giving a more "dead" centre.
                        double t = (midFadeCenter - pos) / halfDip; // 1→0 as pos→centre
                        double sinFade = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::pi * t));
                        fadeGain *= (float)sinFade;
                    }
                    else if (pos >= midFadeCenter && pos <= dipEnd)
                    {
                        // Fade-in: raised-cosine from 0→1
                        double t = (pos - midFadeCenter) / halfDip; // 0→1
                        double sinFade = 0.5 * (1.0 - std::cos(juce::MathConstants<double>::pi * t));
                        fadeGain *= (float)sinFade;
                    }
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
