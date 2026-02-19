#pragma once
#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent,
    public juce::Button::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Audio
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // Button
    void buttonClicked(juce::Button* button) override;

private:
    // Buttons
    juce::TextButton importButton{ "Import WAV" };
    juce::TextButton playButton{ "Play" };

    // Audio
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
