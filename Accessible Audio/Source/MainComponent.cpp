#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setSize(400, 350);

    formatManager.registerBasicFormats();

    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(menuBar.get());
    if (menuBar)
        menuBar->setBounds(0, 0, getWidth(), 25);
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    setAudioChannels(0, 2);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    if (menuBar)
        menuBar->setBounds(0, 0, getWidth(), 25);
}

//==============================================================================
void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(
    const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (readerSource == nullptr)
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
bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    // Digit scrub
    if (readerSource != nullptr)
    {
        auto c = key.getTextCharacter();
        if (c >= '0' && c <= '9')
        {
            transportSource.setPosition((c - '0') / 10.0 * transportSource.getLengthInSeconds());
            return true;
        }
    }

    // Space toggles playback
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlayback();
        return true;
    }

    // Alt+F / Alt+P opens menus
    if (key.getModifiers().isAltDown())
    {
        char c = (char)std::tolower(key.getTextCharacter());
        if (c == 'f')
        {
            if (menuBar) menuBar->showMenu(0);
            return true;
        }
        if (c == 'p')
        {
            if (menuBar) menuBar->showMenu(1);
            return true;
        }
    }

    return false;
}
//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Playback" };
}

juce::PopupMenu MainComponent::getMenuForIndex(
    int menuIndex,
    const juce::String&)
{
    juce::PopupMenu menu;

    if (menuIndex == 0) // File
    {
        menu.addItem(1, "Import WAV...");
        menu.addSeparator();
        menu.addItem(2, "Quit");
    }
    else if (menuIndex == 1) // Playback
    {
        menu.addItem(3,
            transportSource.isPlaying() ? "Stop" : "Play");
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int)
{
    switch (menuItemID)
    {
    case 1:
        importFile();
        break;

    case 2:
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        break;

    case 3:
        togglePlayback();
        break;
    }
}

//==============================================================================
void MainComponent::importFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select a WAV file...",
        juce::File{},
        "*.wav");

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            juce::File file = fc.getResult();

            if (!file.existsAsFile())
                return;

            currentFile = file;

            transportSource.stop();
            transportSource.setSource(nullptr);
            readerSource.reset();

            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(file));

            if (reader == nullptr)
                return;

            auto newSource =
                std::make_unique<juce::AudioFormatReaderSource>(
                    reader.release(), true);

            transportSource.setSource(  
                newSource.get(),
                0,
                nullptr,
                newSource->getAudioFormatReader()->sampleRate);

            readerSource.reset(newSource.release());
        });
}

//==============================================================================
void MainComponent::togglePlayback()
{
    if (readerSource == nullptr)
        return;

    if (transportSource.isPlaying())
        transportSource.stop();
    else
        transportSource.start();
}