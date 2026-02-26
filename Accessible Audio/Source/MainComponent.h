#pragma once
#include <JuceHeader.h>

class MainComponent : public juce::AudioAppComponent,
    public juce::KeyListener,
    public juce::MenuBarModel
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

    // Keyboard
    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;

    // Menu
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String&) override;
    void menuItemSelected(int menuItemID, int) override;

private:

    void importFile();
    void togglePlayback();
    void exportModifiedFile();

    // Track menu navigation
    int activeMenuIndex = -1;
    juce::PopupMenu activePopup;
    std::unique_ptr<juce::MenuBarComponent> menuBar;
    // Audio
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    juce::File currentFile;

    float gain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};