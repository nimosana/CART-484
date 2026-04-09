#pragma once
#include <JuceHeader.h>
#include "CropDialog.h"
#include <deque>
#include <array>

class DarkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // ------------------------------------------------------------------
    // Colour constants (accessible as static members)
    // ------------------------------------------------------------------
    static constexpr uint32_t bg0    = 0xff0a0a0a;
    static constexpr uint32_t bg1    = 0xff111111;
    static constexpr uint32_t bg2    = 0xff181818;
    static constexpr uint32_t bg3    = 0xff222222;
    static constexpr uint32_t line1  = 0xff2e2e2e;
    static constexpr uint32_t line2  = 0xff3a3a3a;
    
    // Slightly brighter for low-vision readability
    static constexpr uint32_t txt0   = 0xfff2f2f2;
    static constexpr uint32_t txt1   = 0xffb0b0b0;
    static constexpr uint32_t txt2   = 0xff7a7a7a;

    // Distinct accent colours per effect type
    static constexpr uint32_t accentOrange = 0xffffaa44; // Gain
    static constexpr uint32_t accentPink   = 0xffff80bf; // Mid-Fade
    static constexpr uint32_t accentYellow = 0xffffd966; // Fade In
    static constexpr uint32_t accentGold   = 0xffe6b800; // Fade Out (distinct from yellow)
    static constexpr uint32_t accentBlue   = 0xff4da6ff; // Low Shelf
    static constexpr uint32_t accentCyan   = 0xff00bcd4; // High Shelf
    static constexpr uint32_t accentGreen  = 0xff66d966; // EQ
    static constexpr uint32_t accentPurple = 0xffb38cff; // Crop

    DarkLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId,      juce::Colour(bg0));
        setColour(juce::DocumentWindow::textColourId,             juce::Colour(txt0));

        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(bg2));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(txt0));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(bg3));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(txt0));
        setColour(juce::PopupMenu::headerTextColourId,            juce::Colour(txt0));

        setColour(juce::AlertWindow::backgroundColourId,          juce::Colour(bg2));
        setColour(juce::AlertWindow::textColourId,                juce::Colour(txt0));
        setColour(juce::AlertWindow::outlineColourId,             juce::Colour(line2));

        setColour(juce::TextButton::buttonColourId,               juce::Colour(bg3));
        setColour(juce::TextButton::buttonOnColourId,             juce::Colour(0xff1e3a5a));
        setColour(juce::TextButton::textColourOffId,              juce::Colour(txt1));
        setColour(juce::TextButton::textColourOnId,               juce::Colour(txt0));

        setColour(juce::TextEditor::backgroundColourId,           juce::Colour(bg1));
        setColour(juce::TextEditor::textColourId,                 juce::Colour(txt0));
        setColour(juce::TextEditor::outlineColourId,              juce::Colour(line2));
        setColour(juce::TextEditor::focusedOutlineColourId,       juce::Colour(accentBlue));
        setColour(juce::Label::textColourId,                      juce::Colour(txt0));
        setColour(juce::Label::backgroundColourId,                juce::Colours::transparentBlack);

        setColour(juce::ScrollBar::thumbColourId,                 juce::Colour(line2));

        setColour(juce::ComboBox::backgroundColourId,             juce::Colour(bg2));
        setColour(juce::ComboBox::textColourId,                   juce::Colour(txt0));
        setColour(juce::ComboBox::outlineColourId,                juce::Colour(line1));
        setColour(juce::ComboBox::arrowColourId,                  juce::Colour(txt1));

        setColour(juce::Slider::backgroundColourId,               juce::Colour(bg2));
        setColour(juce::Slider::trackColourId,                    juce::Colour(accentBlue));
        setColour(juce::Slider::thumbColourId,                    juce::Colour(0xffd0e8ff));
    }

    // ------------------------------------------------------------------
    // MenuBar painting
    // ------------------------------------------------------------------
    void drawMenuBarBackground(juce::Graphics & g, int w, int h,
                               bool /*isMouseOverBar*/,
                               juce::MenuBarComponent &) override
    {
        g.fillAll(juce::Colour(bg1));
        g.setColour(juce::Colour(line1));
        g.drawHorizontalLine(h - 1, 0.f, (float)w);
    }

    void drawMenuBarItem(juce::Graphics & g, int w, int h,
                         int /*itemIndex*/, const juce::String & itemText,
                         bool isMouseOverItem, bool isMenuOpen,
                         bool /*isMouseOverBar*/,
                         juce::MenuBarComponent &) override
    {
        if (isMouseOverItem || isMenuOpen)
        {
            g.setColour(juce::Colour(bg3));
            g.fillRoundedRectangle(2.f, 2.f, (float)w - 4.f, (float)h - 4.f, 3.f);
        }
        g.setFont(juce::Font(20.0f, juce::Font::bold)); // Increased for readability
        g.setColour(juce::Colour(isMouseOverItem || isMenuOpen ? txt0 : txt1));
        g.drawFittedText(itemText, 0, 0, w, h, juce::Justification::centred, 1);
    }

    // ------------------------------------------------------------------
    // PopupMenu painting
    // ------------------------------------------------------------------
    void drawPopupMenuBackground(juce::Graphics & g, int w, int h) override
    {
        g.fillAll(juce::Colour(bg2));
        g.setColour(juce::Colour(line2));
        g.drawRect(0, 0, w, h, 1);
    }

    void drawPopupMenuItem(juce::Graphics & g, const juce::Rectangle<int> & area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String & text, const juce::String & shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override
    {
        if (isSeparator)
        {
            g.setColour(juce::Colour(line1));
            g.drawHorizontalLine(area.getCentreY(), (float)area.getX() + 4, (float)area.getRight() - 4);
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(juce::Colour(bg3));
            g.fillRoundedRectangle(area.reduced(2, 1).toFloat(), 3.f);
        }

        auto colour = isActive ? juce::Colour(txt0) : juce::Colour(txt2);
        if (textColourToUse) colour = *textColourToUse;

        g.setFont(juce::Font(18.0f, juce::Font::bold)); // Increased for submenus
        g.setColour(colour);

        auto textArea = area.reduced(10, 0);
        if (isTicked)
        {
            g.setColour(juce::Colour(accentBlue));
            g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x93 "), area.getX(), area.getY(), 20, area.getHeight(), juce::Justification::centred);
        }
        g.drawFittedText(text, textArea.getX() + (isTicked ? 14 : 0), textArea.getY(),
                         textArea.getWidth(), textArea.getHeight(),
                         juce::Justification::centredLeft, 1);

        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour(juce::Colour(txt2));
            g.drawFittedText(shortcutKeyText, textArea.getX(), textArea.getY(),
                             textArea.getWidth(), textArea.getHeight(),
                             juce::Justification::centredRight, 1);
        }
    }
};

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
        // Report as a ComboBox role ó JAWS announces these as
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
    AccessibleMenuBar(juce::MenuBarModel* model, MainComponent* owner, juce::LookAndFeel* laf)
        : model(model), owner(owner), laf(laf)
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
            if (laf) btn->setLookAndFeel(laf);
            
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
    juce::LookAndFeel* laf = nullptr;
    juce::MenuBarModel* model;
    MainComponent* owner;
    juce::OwnedArray<MenuBarButton>    buttons;
};
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

    // Mouse (EQ dragging)
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

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

    static constexpr int kNumEQBands = 9;
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
        {"63 Hz", 63.0, 0.0, 1.0, false}, {"125 Hz", 125.0, 0.0, 1.0, false},
        {"250 Hz", 250.0, 0.0, 1.0, false}, {"500 Hz", 500.0, 0.0, 1.0, false},
        {"1k Hz", 1000.0, 0.0, 1.0, false}, {"2k Hz", 2000.0, 0.0, 1.0, false},
        {"4k Hz", 4000.0, 0.0, 1.0, false}, {"8k Hz", 8000.0, 0.0, 1.0, false},
        {"16k Hz", 16000.0, 0.0, 1.0, false}
    };

    bool eqEnabled = false, eqEditMode = false;
    int eqSelectedBand = 0;
    juce::Rectangle<int> eqSlidersArea;
    juce::IIRFilter eqFilter[kNumEQBands][2];

    void updateEQCoefficients();
    void applyEQToBuffer(float* data, int numSamples, int channel);
    void drawEQSliders(juce::Graphics& g, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
