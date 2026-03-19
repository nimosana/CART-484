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
    
    if (cropStart >= 0.0)
        info << "   In: " << formatTime(cropStart);
    if (cropEnd >= 0.0)
        info << "   Out: " << formatTime(cropEnd);

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

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    
    if (readerSource == nullptr && memorySource == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transportSource.getNextAudioBlock(bufferToFill);

    if (bufferToFill.buffer != nullptr)
        bufferToFill.buffer->applyGain(bufferToFill.startSample,
                                       bufferToFill.numSamples, gain);
}

void MainComponent::releaseResources()
{
	transportSource.releaseResources();
}


//==============================================================================
// Static helper Ñ used in paint() and announcements
juce::String MainComponent::formatTime(double seconds)
{
    int mins = (int)(seconds / 60);
    int secs = (int)(seconds) % 60;
    return juce::String(mins) + " minutes " + juce::String(secs) + " seconds";
}


//==============================================================================
bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
	char c = (char)std::tolower(key.getTextCharacter());
	bool ctrlDown = key.getModifiers().isCtrlDown();
	bool altDown = key.getModifiers().isAltDown();
	// Digit scrub
	
    if (readerSource != nullptr || transportSource.getLengthInSeconds() > 0.0)
	{
		//auto c = key.getTextCharacter();
		if (c >= '0' && c <= '9')
		{
			transportSource.setPosition((c - '0') / 10.0 * transportSource.getLengthInSeconds());
			return true;
		} else if (c == 'g')
		{
			gainEditMode = !gainEditMode;
			repaint();
			return true;
		}
        
        // announce time
            else if (c == 't')
            {
                announceTime();
                return true;
            }
        
        // I Ñ set crop IN point at playhead
               else if (c == 'i')
               {
                   cropStart = transportSource.getCurrentPosition();
                   juce::String msg = "Crop start set at " + formatTime(cropStart);
                   juce::AccessibilityHandler::postAnnouncement(
                       msg, juce::AccessibilityHandler::AnnouncementPriority::high);
                   repaint();
                   return true;
               }
               // O Ñ set crop OUT point at playhead
               else if (c == 'o' && !ctrlDown)
               {
                   cropEnd = transportSource.getCurrentPosition();
                   juce::String msg = "Crop end set at " + formatTime(cropEnd);
                   juce::AccessibilityHandler::postAnnouncement(
                       msg, juce::AccessibilityHandler::AnnouncementPriority::high);
                   repaint();
                   return true;
               }
               // Shift+C Ñ apply the crop
               else if (c == 'c' && key.getModifiers().isShiftDown())
               {
                   applyCrop();
                   return true;
               }
        
		if (c == 'l')
		{
			lowPassEnabled = !lowPassEnabled;
			return true;
		}
		else if (c == 'h')
		{
			highPassEnabled = !highPassEnabled;
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
	else if (key == juce::KeyPress::tabKey)
	{
		if (activeMenuIndex == -1)
			activeMenuIndex = 0;
		else
			activeMenuIndex = (activeMenuIndex + 1) % getMenuBarNames().size();

		if (true)
			menuBar->showMenu(activeMenuIndex); // this is required; no “highlight only” possible

		return true;
	}
	double scrubAmount = ctrlDown ? 10.0 : 2;
	double currentPos = transportSource.getCurrentPosition();
	double length = transportSource.getLengthInSeconds();
	//int keyCode = key.getKeyCode();


	if (ctrlDown)
	{
		if (key.getKeyCode() == 'o' || key.getKeyCode() == 'O') {
			importFile();
		}
		if (key.getKeyCode() == 's' || key.getKeyCode() == 'S') {
			exportModifiedFile();
		}
		if (key.getKeyCode() == juce::KeyPress::homeKey)
		{
			transportSource.setPosition(0.0); // start of file
			repaint();
			return true;
		}

		if (key.getKeyCode() == juce::KeyPress::endKey)
		{
			double length = transportSource.getLengthInSeconds();
			transportSource.setPosition(length); // end of file
			repaint();
			return true;
		}
	}
	if (key.getKeyCode() == juce::KeyPress::rightKey)
	{
		double newPos = juce::jlimit(0.0, length, currentPos + scrubAmount);
		transportSource.setPosition(newPos);
		repaint();
		return true;
	}

	if (key.getKeyCode() == juce::KeyPress::leftKey)
	{
		double newPos = juce::jlimit(0.0, length, currentPos - scrubAmount);
		transportSource.setPosition(newPos);
		repaint();
		return true;
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
	}

	// Alt+F / Alt+P opens menus
	if (altDown)
	{
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

void MainComponent::announceTime()
{
    double currentPos   = transportSource.getCurrentPosition();
    double totalLength  = transportSource.getLengthInSeconds();
    juce::String msg;
    msg << formatTime(currentPos);
    juce::AccessibilityHandler::postAnnouncement(
        msg, juce::AccessibilityHandler::AnnouncementPriority::high);
}

//==============================================================================
juce::StringArray MainComponent::getMenuBarNames()
{
	return { "File", "Playback", "Edit" };
}

juce::PopupMenu MainComponent::getMenuForIndex(
	int menuIndex,
	const juce::String&)
{
	juce::PopupMenu menu;

	if (menuIndex == 0) // File
	{
		menu.addItem(1, "Open WAV");
		menu.addSeparator();
		menu.addItem(2, "Save Modified WAV");
		menu.addSeparator();
		menu.addItem(3, "Quit");
	}
	else if (menuIndex == 1) // Playback
	{
		menu.addItem(4,
			transportSource.isPlaying() ? "Stop" : "Play");
		//menu.addItem(5, "Precise Scrubbing (arrow keys)", true, scrubMode);
	}
	else if (menuIndex == 2) // Edit
	{
		menu.addItem(5, "Adjust Gain (arrow keys)", true, gainEditMode);
        menu.addSeparator();
        menu.addItem(6, "Set Crop Start (type time)");
        menu.addItem(7, "Set Crop End (type time)");
        menu.addItem(8, "Apply Crop", cropStart >= 0.0 && cropEnd >= 0.0);
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
		gainEditMode = !gainEditMode;
		repaint();
		break;
    case 6: openCropDialog(true);   break;
    case 7: openCropDialog(false);  break;
    case 8: applyCrop();            break;
	}
}


//==============================================================================
void MainComponent::openCropDialog(bool isStart)
{
    if (readerSource == nullptr)
    {
        juce::AccessibilityHandler::postAnnouncement(
            "No file loaded. Please open a WAV file first.",
            juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }

    double currentVal = isStart
        ? (cropStart >= 0.0 ? cropStart : transportSource.getCurrentPosition())
        : (cropEnd   >= 0.0 ? cropEnd   : transportSource.getCurrentPosition());

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
                juce::AccessibilityHandler::postAnnouncement(
                    "Crop start set to " + formatTime(cropStart),
                    juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            else
            {
                cropEnd = clamped;
                juce::AccessibilityHandler::postAnnouncement(
                    "Crop end set to " + formatTime(cropEnd),
                    juce::AccessibilityHandler::AnnouncementPriority::high);
            }
            repaint();
        });

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(dialog);
    opts.dialogTitle                = dialogTitle;
    opts.dialogBackgroundColour     = getLookAndFeel().findColour(
                                        juce::ResizableWindow::backgroundColourId);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar          = true;
    opts.resizable                  = false;
    opts.launchAsync();

    // Give focus to the text editor so screen reader reads the field immediately
    dialog->grabFocusOnOpen();
}

//==============================================================================
void MainComponent::applyCrop()
{
    if (readerSource == nullptr)
    {
        juce::AccessibilityHandler::postAnnouncement(
            "No file loaded.",
            juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }

    double length = transportSource.getLengthInSeconds();

    // Fall back to full file if points aren't set
    double startSec = (cropStart >= 0.0) ? cropStart : 0.0;
    double endSec   = (cropEnd   >= 0.0) ? cropEnd   : length;

    if (startSec >= endSec)
    {
        juce::AccessibilityHandler::postAnnouncement(
            "Crop start must be before crop end. Please reset your crop points.",
            juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }

    // Read the original file into memory
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(currentFile));

    if (reader == nullptr)
    {
        juce::AccessibilityHandler::postAnnouncement(
            "Could not read file for cropping.",
            juce::AccessibilityHandler::AnnouncementPriority::high);
        return;
    }

    fileSampleRate  = reader->sampleRate;
    fileNumChannels = (int)reader->numChannels;

    juce::int64 startSample = (juce::int64)(startSec * fileSampleRate);
    juce::int64 endSample   = (juce::int64)(endSec   * fileSampleRate);
    juce::int64 numSamples  = endSample - startSample;

    // Copy just the crop region into our in-memory buffer
    audioBuffer.setSize(fileNumChannels, (int)numSamples);
    reader->read(&audioBuffer, 0, (int)numSamples, startSample, true, true);

    // Stop transport and swap in the new in-memory source
    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();

    memorySource = std::make_unique<juce::MemoryAudioSource>(audioBuffer, false);
    transportSource.setSource(memorySource.get(), 0, nullptr, fileSampleRate);

    // Reset crop markers now that they are baked in
    cropStart = -1.0;
    cropEnd   = -1.0;

    double newLength = numSamples / fileSampleRate;
    juce::AccessibilityHandler::postAnnouncement(
        "Crop applied. New length is " + formatTime(newLength) +
        ". Press Space to play.",
        juce::AccessibilityHandler::AnnouncementPriority::high);

    repaint();
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
          
            memorySource.reset();  // NEW Ñ clear the old memory source if any
            audioBuffer.setSize(0, 0);  // NEW Ñ clear stale buffer too

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
    if (readerSource == nullptr && transportSource.getLengthInSeconds() <= 0.0)
        return;

    if (transportSource.isPlaying())
        transportSource.stop();
    else
        transportSource.start();
}

void MainComponent::exportModifiedFile()
{
    // Export works from the in-memory buffer if a crop was applied,
    // otherwise falls back to reading from the original file
    bool hasMemoryBuffer = (audioBuffer.getNumSamples() > 0);

    if (!hasMemoryBuffer && (readerSource == nullptr || !currentFile.existsAsFile()))
        return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export modified WAV file",
        currentFile.getSiblingFile(
            currentFile.getFileNameWithoutExtension() + "_modified"),
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

            std::unique_ptr<juce::FileOutputStream> fileStream(
                outputFile.createOutputStream());
            if (fileStream == nullptr || !fileStream->openedOk()) return;

            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(
                    fileStream.release(),
                    fileSampleRate,
                    (unsigned int)fileNumChannels,
                    16, {}, 0));
            if (writer == nullptr) return;

            if (hasMemoryBuffer)
            {
                // Write from in-memory cropped buffer with gain applied
                juce::AudioBuffer<float> copy(audioBuffer);
                copy.applyGain(gain);
                writer->writeFromAudioSampleBuffer(copy, 0, copy.getNumSamples());
            }
            else
            {
                // Write from original file with gain applied
                std::unique_ptr<juce::AudioFormatReader> reader(
                    formatManager.createReaderFor(currentFile));
                if (reader == nullptr) return;

                const int bufferSize = 32768;
                juce::AudioBuffer<float> buffer(fileNumChannels, bufferSize);
                auto totalSamples    = reader->lengthInSamples;
                juce::int64 processed = 0;

                while (processed < totalSamples)
                {
                    int thisBlock = (int)juce::jmin(
                        (juce::int64)bufferSize, totalSamples - processed);
                    if (!reader->read(&buffer, 0, thisBlock, processed, true, true))
                        break;
                    buffer.applyGain(0, thisBlock, gain);
                    writer->writeFromAudioSampleBuffer(buffer, 0, thisBlock);
                    processed += thisBlock;
                }
            }
        });
}
