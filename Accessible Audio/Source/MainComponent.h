#pragma once
#include <JuceHeader.h>
#include "CropDialog.h"

#include <memory>
#include <deque>
class MainComponent;

class MenuBarButton : public juce::TextButton
{
public:
    MenuBarButton(const juce::String& name) : juce::TextButton(name)
    {
        setTitle(name);
        setDescription(name + " menu");
        setWantsKeyboardFocus(true);
        setAccessible(true);
    }

protected:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        // Report as a ComboBox role — JAWS announces these as
        // "press Enter to open" instead of "press Space"
        class MenuButtonAccessHandler : public juce::AccessibilityHandler
        {
        public:
            MenuButtonAccessHandler(juce::TextButton& btn)
                : juce::AccessibilityHandler(
                    btn,
                    juce::AccessibilityRole::menuItem,
                    juce::AccessibilityActions()
                    .addAction(juce::AccessibilityActionType::press,
                        [&btn] { btn.triggerClick(); }))
            {
            }

            juce::String getTitle() const override
            {
                return getComponent().getTitle();
            }

            juce::String getHelp() const override
            {
                return "Press Enter to open menu";
            }
        };

        return std::make_unique<MenuButtonAccessHandler>(*this);
    }
};

class AccessibleMenuBar : public juce::Component,
    public juce::KeyListener
{
public:
    AccessibleMenuBar(juce::MenuBarModel* model, MainComponent* owner)
        : model(model), owner(owner)
    {
        rebuild();
    }

    void rebuild()
    {
        buttons.clear();
        removeAllChildren();
        if (model == nullptr) return;

        auto names = model->getMenuBarNames();
        for (int i = 0; i < names.size(); ++i)
        {
            auto* btn = new MenuBarButton(names[i]);

            int idx = i;
            btn->onClick = [this, idx]() {
                showMenu(idx);
                };

            // Also trigger on Enter key specifically
            btn->addKeyListener(this);

            addAndMakeVisible(btn);
            buttons.add(btn);
        }
        resized();
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component* src) override
    {
        // Make Enter open the menu, same as click
        if (key == juce::KeyPress::returnKey)
        {
            for (int i = 0; i < buttons.size(); ++i)
            {
                if (src == buttons[i])
                {
                    showMenu(i);
                    return true;
                }
            }
        }
        return false;
    }

    void showMenu(int index)
    {
        if (model == nullptr || index >= buttons.size()) return;
        auto menu = model->getMenuForIndex(index, {});
        auto* btn = buttons[index];

        menu.showMenuAsync(
            juce::PopupMenu::Options()
            .withTargetComponent(btn),
            [this, index](int result) {
                if (result != 0 && model != nullptr)
                    model->menuItemSelected(result, index);
            });
    }

    void openMenuByIndex(int index)
    {
        showMenu(index);
    }

    void resized() override
    {
        if (buttons.isEmpty()) return;
        int x = 4;
        int btnH = getHeight();
        for (auto* btn : buttons)
        {
            int w = juce::Font(14.0f).getStringWidth(btn->getButtonText()) + 20;
            btn->setBounds(x, 0, w, btnH);
            x += w + 2;
        }
    }

private:
    juce::MenuBarModel* model;
    MainComponent* owner;
    juce::OwnedArray<MenuBarButton>    buttons;
};
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

	// Mouse (EQ dragging)
	void mouseDown(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;

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
    
    void announceEffectsStatus();

	std::unique_ptr<juce::MemoryAudioSource> memorySource;

	// Helper: formats seconds as "X minutes Y seconds"
	static juce::String formatTime(double seconds);

	// Track menu navigation
	int activeMenuIndex = -1;
	juce::PopupMenu activePopup;
    std::unique_ptr<AccessibleMenuBar> menuBar;;
	// Audio
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
    
    
    // Effect history
    juce::StringArray effectHistory;
    void logEffect(const juce::String& description);


	// === Parametric EQ ===
	// 9 bands matching standard graphic EQ: 63 125 250 500 1k 2k 4k 8k 16k
	// === Parametric EQ ===
	static constexpr int kNumEQBands = 9;

	struct EQBand {
		juce::String name;
		double       freq;
		double       gainDB;
		double       Q;
		bool         enabled;
	};
    
    // Undo / Redo
    struct AppState
    {
        juce::String description;
        
        // Parameters
        float gain;
        float gainStep;
        bool fadeInEnabled;
        bool fadeOutEnabled;
        double fadeInDuration;
        double fadeOutDuration;
        bool lowPassEnabled;
        bool highPassEnabled;
        double lowShelfFreq;
        double highShelfFreq;
        double cropStart;
        double cropEnd;
        bool eqEnabled;

        EQBand eqBands[kNumEQBands];

        // Audio buffer snapshot pointer
        std::shared_ptr<juce::AudioBuffer<float>> sharedBuffer;
        double fileSampleRate;
        int fileNumChannels;
    };

    static constexpr int maxUndoLevels = 50;
    std::deque<AppState> undoStack;
    std::deque<AppState> redoStack;

    void saveUndoState(const juce::String& description);

    void performUndo();
    void performRedo();
    AppState captureCurrentState();
    void restoreState(const AppState& state);

	EQBand eqBands[kNumEQBands] = {
		{ "63 Hz",   63.0,    0.0, 1.0, false },
		{ "125 Hz",  125.0,   0.0, 1.0, false },
		{ "250 Hz",  250.0,   0.0, 1.0, false },
		{ "500 Hz",  500.0,   0.0, 1.0, false },
		{ "1k Hz",   1000.0,  0.0, 1.0, false },
		{ "2k Hz",   2000.0,  0.0, 1.0, false },
		{ "4k Hz",   4000.0,  0.0, 1.0, false },
		{ "8k Hz",   8000.0,  0.0, 1.0, false },
		{ "16k Hz",  16000.0, 0.0, 1.0, false },
	};

	bool eqEnabled = false;
	bool eqEditMode = false;   // Ctrl+E enters/exits; Tab cycles bands; Up/Down adjusts
	int  eqSelectedBand = 0;
	juce::Rectangle<int> eqSlidersArea;

	juce::IIRFilter eqFilter[kNumEQBands][2];

	void updateEQCoefficients();
	void applyEQToBuffer(float* data, int numSamples, int channel);
	void drawEQSliders(juce::Graphics& g, juce::Rectangle<int> area);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
