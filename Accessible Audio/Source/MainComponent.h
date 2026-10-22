#pragma once
#include <JuceHeader.h>
#include "UI/CropDialog.h"
#include "LookAndFeel/DarkLookAndFeel.h"
#include "UI/AccessibleMenuBar.h"
#include "GamepadManager.h"
#include <deque>
#include <array>

class MainComponent : public juce::AudioAppComponent,
	public juce::KeyListener,
	public juce::MenuBarModel,
	private juce::Timer
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
	void toggleGainEditMode();
	void toggleLowShelf();
	void toggleLowShelfEdit();
	void toggleHighShelf();
	void toggleFadeIn();
	void toggleFadeOut();
	void toggleMidFade();
	void toggleEqEdit();
	bool checkIfOpenFile(bool, bool);


	// Mouse (EQ dragging) can make vytautas lose keyboard focus
	//void mouseDown(const juce::MouseEvent& e) override;
	//void mouseDrag(const juce::MouseEvent& e) override;

	// Menu
	juce::StringArray getMenuBarNames() override;
	juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String&) override;
	void menuItemSelected(int menuItemID, int) override;

	void timerCallback() override { repaint(); }

private:
	void importFile();
	void togglePlayback();
	void exportModifiedFile();
	void updateFilterCoefficients();
	void applyCrop();
	void openCropDialog(bool isStart);
	void announceTime();
	void announceEffectsStatus();

	DarkLookAndFeel darkLAF;

	std::unique_ptr<juce::MemoryAudioSource> memorySource;
	static juce::String formatTime(double seconds);

	int activeMenuIndex = -1;
	juce::PopupMenu activePopup;
	std::unique_ptr<AccessibleMenuBar> menuBar;

	juce::AudioFormatManager formatManager;
	std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
	juce::AudioTransportSource transportSource;
	juce::File currentFile;
	std::shared_ptr<juce::AudioBuffer<float>> sharedAudioBuffer;

	double fileSampleRate = 44100.0;
	int fileNumChannels = 2;
	double cropStart = -1.0;
	double cropEnd = -1.0;

	float gain = 1.0f;
	float gainStep = 0.1f;
	bool gainEditMode = false;
	bool scrubMode = false;

	bool fadeInEnabled = false, fadeOutEnabled = false;
	bool fadeInEditMode = false, fadeOutEditMode = false;
	double fadeInDuration = 1.0, fadeOutDuration = 1.0;

	bool midFadeEnabled = false, midFadeEditMode = false;
	double midFadeCenter = 0.0, midFadeDuration = 1.0;

	bool lowPassEnabled = false, highPassEnabled = false;
	bool lowFreqEditMode = false, highFreqEditMode = false;
	double lowShelfFreq = 300.0, highShelfFreq = 8000.0;
	double currentSampleRate = 44100.0;

	juce::IIRFilter lowShelfFilter[2], highShelfFilter[2];

	juce::StringArray effectHistory;
	void logEffect(const juce::String& description);

	static constexpr int kNumEQBands = 11;
	struct EQBand {
		juce::String name;
		double freq, gainDB, Q;
		bool enabled;
	};

	struct AppState {
		juce::String description;
		float gain, gainStep;
		bool fadeInEnabled, fadeOutEnabled;
		double fadeInDuration, fadeOutDuration;
		bool lowPassEnabled, highPassEnabled;
		double lowShelfFreq, highShelfFreq;
		double cropStart, cropEnd;
		bool eqEnabled, midFadeEnabled;
		double midFadeCenter, midFadeDuration;
		EQBand eqBands[kNumEQBands];
		std::shared_ptr<juce::AudioBuffer<float>> sharedBuffer;
		double fileSampleRate;
		int fileNumChannels;
	};

	static constexpr int maxUndoLevels = 50;
	std::deque<AppState> undoStack, redoStack;
	void saveUndoState(const juce::String& description);
	void performUndo(), performRedo();
	AppState captureCurrentState();
	void restoreState(const AppState& state);

	EQBand eqBands[kNumEQBands] = {
		{"20 Hz", 20.0, 0.0, 1.0, false}, {"63 Hz", 63.0, 0.0, 1.0, false},
		{"125 Hz", 125.0, 0.0, 1.0, false}, {"250 Hz", 250.0, 0.0, 1.0, false},
		{"500 Hz", 500.0, 0.0, 1.0, false}, {"1k Hz", 1000.0, 0.0, 1.0, false},
		{"2k Hz", 2000.0, 0.0, 1.0, false}, {"4k Hz", 4000.0, 0.0, 1.0, false},
		{"8k Hz", 8000.0, 0.0, 1.0, false}, {"16k Hz", 16000.0, 0.0, 1.0, false},
		{ "20k Hz", 20000.0, 0.0, 1.0, false }
	};

	bool eqEnabled = false, eqEditMode = false;
	int eqSelectedBand = 0;
	juce::Rectangle<int> eqSlidersArea;
	juce::IIRFilter eqFilter[kNumEQBands][2];

	// Dynamic layout: computed each paint() so curve+EQ share remaining height
	int eqFlexCurveH = 140;
	int eqFlexH = 200;

	void updateEQCoefficients();
	void applyEQToBuffer(float* data, int numSamples, int channel);
	void drawEQSliders(juce::Graphics& g, juce::Rectangle<int> area);

	GamepadManager gamepad;   // <-- new, must be inside the class

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};