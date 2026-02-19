#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setSize(600, 400);

    // Register WAV support
    formatManager.registerBasicFormats();

    // Buttons
    addAndMakeVisible(importButton);
    addAndMakeVisible(playButton);

    importButton.addListener(this);
    playButton.addListener(this);

    // Audio device
    setAudioChannels(0, 2); // no input, stereo output
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Accessible Audio Editor Prototype",
        getLocalBounds().reduced(10),
        juce::Justification::topLeft,
        true);
}

void MainComponent::resized()
{
    importButton.setBounds(50, 80, 200, 40);
    playButton.setBounds(50, 140, 200, 40);
}

//==============================================================================
// AUDIO

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource.get() == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transportSource.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    transportSource.releaseResources();
}

//==============================================================================
// BUTTONS

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &importButton)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select a WAV file...",
            juce::File{},
            "*.wav"
        );

        chooser->launchAsync(juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                juce::File file = fc.getResult();   // explicit type avoids template confusion

                if (!file.existsAsFile())
                    return;

                // Stop and clear previous audio safely
                transportSource.stop();
                transportSource.setSource(nullptr);
                readerSource.reset();

                // Create reader
                std::unique_ptr<juce::AudioFormatReader> reader(
                    formatManager.createReaderFor(file)
                );

                if (reader == nullptr)
                    return;

                const double sampleRate = reader->sampleRate;

                // Transfer ownership of reader → AudioFormatReaderSource
                auto newSource = std::make_unique<juce::AudioFormatReaderSource>(
                    reader.release(), true
                );

                transportSource.setSource(newSource.get(), 0, nullptr, sampleRate);

                // Transfer ownership of newSource → readerSource member
                readerSource.reset(newSource.release());
            });
    }


    else if (button == &playButton)
    {
        if (transportSource.isPlaying())
            transportSource.stop();
        else
            transportSource.start();
    }
}
