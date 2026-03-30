#pragma once
#include <JuceHeader.h>
#include "CropDialog.h"

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

	// Shelf filter helpers
	void updateFilterCoefficients();

	void applyCrop();
	void openCropDialog(bool isStart);
	void announceTime();

	std::unique_ptr<juce::MemoryAudioSource> memorySource;

	// Helper: formats seconds as "X minutes Y seconds"
	static juce::String formatTime(double seconds);

	// Track menu navigation
	int activeMenuIndex = -1;
	juce::PopupMenu activePopup;
	std::unique_ptr<juce::MenuBarComponent> menuBar;
	// Audio
	juce::AudioFormatManager formatManager;
	std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
	juce::AudioTransportSource transportSource;

	juce::File currentFile;

	juce::AudioBuffer<float> audioBuffer;
	double fileSampleRate = 44100.0;
	int fileNumChannels = 2;

	double cropStart = -1.0;
	double cropEnd = -1.0;

	float gain = 1.0f;
	float gainStep = 0.1f;
	bool gainEditMode = false;
	bool scrubMode = false;

	// Fades
	// Q / W        : toggle fade in / out on-off
	// Ctrl+Q / W   : enter/exit duration edit mode (arrows change duration)
	bool   fadeInEnabled = false;
	bool   fadeOutEnabled = false;
	bool   fadeInEditMode = false;
	bool   fadeOutEditMode = false;
	double fadeInDuration = 1.0;   // seconds, range 0.1-30, step 0.1
	double fadeOutDuration = 1.0;

	// Filters
	// Low shelf  : -12 dB cut below lowShelfFreq   (toggle: L, freq edit: Ctrl+L)
	// High shelf : -12 dB cut above highShelfFreq  (toggle: H, freq edit: Ctrl+H)
	bool lowPassEnabled = false;
	bool highPassEnabled = false;
	bool lowFreqEditMode = false;  // Ctrl+L enters/exits; arrows change frequency
	bool highFreqEditMode = false;  // Ctrl+H enters/exits; arrows change frequency

	double lowShelfFreq = 300.0;   // Hz, range 20?2000,  step 50
	double highShelfFreq = 8000.0;  // Hz, range 1000?20000, step 200

	double currentSampleRate = 44100.0;

	// One filter instance per channel (stereo = 2)
	juce::IIRFilter lowShelfFilter[2];
	juce::IIRFilter highShelfFilter[2];


	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
