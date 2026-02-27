#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setSize(400, 350);

    formatManager.registerBasicFormats();

    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(menuBar.get());
    if (menuBar) {
        menuBar->setModel(this);
        menuBar->setBounds(0, 0, getWidth(), 25);
    }
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

    g.setColour(juce::Colours::white);
    g.setFont(14.0f);

    juce::String info;
    info << "Gain: " << juce::String(gain, 2) << "x";
    info << "   Step: " << juce::String(gainStep, 2);
    if (gainEditMode)
        info << "   (Use arrow keys to adjust)";

    auto bounds = getLocalBounds().reduced(8, 8);
    g.drawFittedText(info, bounds.removeFromBottom(24),
        juce::Justification::centredLeft, 1);
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

    if (bufferToFill.buffer != nullptr)
        bufferToFill.buffer->applyGain(bufferToFill.startSample, bufferToFill.numSamples, gain);
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
    // Tab cycles through menus
    if (key == juce::KeyPress::tabKey)
    {
        if (activeMenuIndex == -1)
            activeMenuIndex = 0;
        else
            activeMenuIndex = (activeMenuIndex + 1) % getMenuBarNames().size();

        if (true)
            menuBar->showMenu(activeMenuIndex); // this is required; no “highlight only” possible

        return true;
    }
    if (scrubMode)
    {

    }
    // Gain edit mode: arrow keys adjust gain and step
    if (gainEditMode)
    {
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            gain = juce::jlimit(0.0f, 4.0f, gain + gainStep);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            gain = juce::jlimit(0.0f, 4.0f, gain - gainStep);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::rightKey)
        {
            gainStep = juce::jlimit(0.01f, 1.0f, gainStep * 2.0f);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::leftKey)
        {
            gainStep = juce::jlimit(0.01f, 1.0f, gainStep * 0.5f);
            repaint();
            return true;
        }
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
        if (c == 'e')
        {
            if (menuBar) menuBar->showMenu(2);
            return true;
        }
    }

    return false;
}
//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Playback", "Edit"};
}

juce::PopupMenu MainComponent::getMenuForIndex(
    int menuIndex,
    const juce::String&)
{
    juce::PopupMenu menu;

    if (menuIndex == 0) // File
    {
        menu.addItem(1, "Import WAV");
        menu.addSeparator();
        menu.addItem(2, "Export Modified WAV");
        menu.addSeparator();
        menu.addItem(3, "Quit");
    }
    else if (menuIndex == 1) // Playback
    {
        menu.addItem(4,
            transportSource.isPlaying() ? "Stop" : "Play");
        menu.addItem(5, "Precise Scrubbing (arrow keys)", true, scrubMode);
    }
    else if (menuIndex == 2) // Edit
    {
        menu.addItem(6, "Adjust Gain (arrow keys)", true, gainEditMode);
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
        exportModifiedFile();
        break;
    case 3:
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        break;
    case 4:
        togglePlayback();
        break;
    case 5:
        scrubMode = !scrubMode;
        repaint();
        break;
    case 6:
        gainEditMode = !gainEditMode;
        repaint();
        break;
    }
}

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
    if (readerSource == nullptr) return;

    if (transportSource.isPlaying())
        transportSource.stop();
    else
        transportSource.start();
}

void MainComponent::exportModifiedFile()
{
    if (readerSource == nullptr || !currentFile.existsAsFile())
        return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export modified WAV file",
        currentFile.getSiblingFile(currentFile.getFileNameWithoutExtension() + "_modified"),
        "*.wav");

    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode |
        juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto outputFile = fc.getResult();

            if (outputFile == juce::File{})
                return;

            if (!outputFile.hasFileExtension(".wav"))
                outputFile = outputFile.withFileExtension(".wav");

            std::unique_ptr<juce::AudioFormatReader> reader(
                formatManager.createReaderFor(currentFile));

            if (reader == nullptr)
                return;

            std::unique_ptr<juce::FileOutputStream> fileStream(outputFile.createOutputStream());

            if (fileStream == nullptr || !fileStream->openedOk())
                return;

            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(fileStream.release(),
                    reader->sampleRate,
                    static_cast<unsigned int>(reader->numChannels),
                    16,
                    {},
                    0));

            if (writer == nullptr)
                return;

            const int bufferSize = 32768;
            juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), bufferSize);

            auto totalSamples = reader->lengthInSamples;
            juce::int64 samplesProcessed = 0;

            while (samplesProcessed < totalSamples)
            {
                auto remaining64 = totalSamples - samplesProcessed;
                int samplesThisBlock = bufferSize;
                if (remaining64 < bufferSize)
                    samplesThisBlock = static_cast<int>(remaining64);

                if (!reader->read(&buffer, 0, samplesThisBlock, samplesProcessed, true, true))
                    break;

                buffer.applyGain(0, samplesThisBlock, gain);

                writer->writeFromAudioSampleBuffer(buffer, 0, samplesThisBlock);

                samplesProcessed += samplesThisBlock;
            }
        });
}