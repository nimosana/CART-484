#pragma once
#include <JuceHeader.h>

// ============================================================
//  DarkLookAndFeel
//  A dark, modern theme for the accessible audio editor.
//
//  Palette
//  -------
//  bg0  #0a0a0a  – deepest background (window fill)
//  bg1  #111111  – primary surface (panels, rows)
//  bg2  #1a1a1a  – secondary surface (EQ area, freq curve)
//  bg3  #222222  – subtle elevation (hover, selected row bg)
//  line #2e2e2e  – thin divider / border
//  line2 #3a3a3a – slightly brighter border for emphasis
//  txt0 #f0f0f0  – primary text (ON labels, active values)
//  txt1 #909090  – secondary text (disabled labels, hints)
//  txt2 #555555  – very dim text (off states)
//
//  Accent colours (dark-tinted, semi-transparent where used as fills)
//  accent_blue   #1e4a7a / highlight rgba(30,100,200, 0.25)
//  accent_yellow #5a4a00 / highlight rgba(200,160,0,  0.25)
//  accent_green  #1a4a1a / highlight rgba(40,160,40,  0.25)
//  accent_orange #5a2a00 / highlight rgba(200,90,0,   0.25)
//  accent_cyan   #004a5a / stroke  #2dd4e8
// ============================================================

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
    static constexpr uint32_t txt0   = 0xfff0f0f0;
    static constexpr uint32_t txt1   = 0xff909090;
    static constexpr uint32_t txt2   = 0xff505050;

    // Effect-row accent colours  (stroke / ON label)
    static constexpr uint32_t accentBlue   = 0xff4da6ff;   // filters
    static constexpr uint32_t accentYellow = 0xffffd966;   // fades
    static constexpr uint32_t accentGreen  = 0xff66d966;   // EQ
    static constexpr uint32_t accentOrange = 0xffffaa44;   // gain modified

    // Effect-row highlight fills (semi-transparent)
    static juce::Colour hlBlue()   { return juce::Colour(0x1e1e6eff); }  // ~12 % opacity
    static juce::Colour hlYellow() { return juce::Colour(0x1effc80a); }
    static juce::Colour hlGreen()  { return juce::Colour(0x1e28c228); }
    static juce::Colour hlOrange() { return juce::Colour(0x1eff7000); }

    // ------------------------------------------------------------------
    DarkLookAndFeel()
    {
        // Set the colour scheme for standard JUCE components
        setColour(juce::ResizableWindow::backgroundColourId,      juce::Colour(bg0));
        setColour(juce::DocumentWindow::textColourId,             juce::Colour(txt0));

        // MenuBar
        setColour(juce::MenuBarComponent::backgroundColourId,     juce::Colour(bg1));
        setColour(juce::MenuBarComponent::textColourId,           juce::Colour(txt1));
        setColour(juce::MenuBarComponent::highlightedBackgroundColourId, juce::Colour(bg3));
        setColour(juce::MenuBarComponent::highlightedTextColourId, juce::Colour(txt0));

        // PopupMenu
        setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(bg2));
        setColour(juce::PopupMenu::textColourId,                  juce::Colour(txt1));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(bg3));
        setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(txt0));
        setColour(juce::PopupMenu::headerTextColourId,            juce::Colour(txt0));

        // AlertWindow / dialogs
        setColour(juce::AlertWindow::backgroundColourId,          juce::Colour(bg2));
        setColour(juce::AlertWindow::textColourId,                juce::Colour(txt0));
        setColour(juce::AlertWindow::outlineColourId,             juce::Colour(line2));

        // TextButton
        setColour(juce::TextButton::buttonColourId,               juce::Colour(bg3));
        setColour(juce::TextButton::buttonOnColourId,             juce::Colour(0xff1e3a5a));
        setColour(juce::TextButton::textColourOffId,              juce::Colour(txt1));
        setColour(juce::TextButton::textColourOnId,               juce::Colour(txt0));

        // TextEditor / Label
        setColour(juce::TextEditor::backgroundColourId,           juce::Colour(bg1));
        setColour(juce::TextEditor::textColourId,                 juce::Colour(txt0));
        setColour(juce::TextEditor::outlineColourId,              juce::Colour(line2));
        setColour(juce::TextEditor::focusedOutlineColourId,       juce::Colour(accentBlue));
        setColour(juce::Label::textColourId,                      juce::Colour(txt0));
        setColour(juce::Label::backgroundColourId,                juce::Colours::transparentBlack);

        // Scrollbar
        setColour(juce::ScrollBar::thumbColourId,                 juce::Colour(line2));

        // ComboBox
        setColour(juce::ComboBox::backgroundColourId,             juce::Colour(bg2));
        setColour(juce::ComboBox::textColourId,                   juce::Colour(txt0));
        setColour(juce::ComboBox::outlineColourId,                juce::Colour(line1));
        setColour(juce::ComboBox::arrowColourId,                  juce::Colour(txt1));

        // Slider
        setColour(juce::Slider::backgroundColourId,               juce::Colour(bg2));
        setColour(juce::Slider::trackColourId,                    juce::Colour(accentBlue));
        setColour(juce::Slider::thumbColourId,                    juce::Colour(0xffd0e8ff));
    }

    // ------------------------------------------------------------------
    // MenuBar painting
    // ------------------------------------------------------------------
    void drawMenuBarBackground(juce::Graphics& g, int w, int h,
                               bool /*isMouseOverBar*/,
                               juce::MenuBarComponent&) override
    {
        g.fillAll(juce::Colour(bg1));
        // thin bottom separator
        g.setColour(juce::Colour(line1));
        g.drawHorizontalLine(h - 1, 0.f, (float)w);
    }

    void drawMenuBarItem(juce::Graphics& g, int w, int h,
                         int /*itemIndex*/, const juce::String& itemText,
                         bool isMouseOverItem, bool isMenuOpen,
                         bool /*isMouseOverBar*/,
                         juce::MenuBarComponent&) override
    {
        if (isMouseOverItem || isMenuOpen)
        {
            g.setColour(juce::Colour(bg3));
            g.fillRoundedRectangle(2.f, 2.f, (float)w - 4.f, (float)h - 4.f, 3.f);
        }
        g.setFont(juce::Font(13.0f));
        g.setColour(juce::Colour(isMouseOverItem || isMenuOpen ? txt0 : txt1));
        g.drawFittedText(itemText, 0, 0, w, h, juce::Justification::centred, 1);
    }

    // ------------------------------------------------------------------
    // PopupMenu painting
    // ------------------------------------------------------------------
    void drawPopupMenuBackground(juce::Graphics& g, int w, int h) override
    {
        g.fillAll(juce::Colour(bg2));
        g.setColour(juce::Colour(line2));
        g.drawRect(0, 0, w, h, 1);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColourToUse) override
    {
        if (isSeparator)
        {
            g.setColour(juce::Colour(line1));
            g.drawHorizontalLine(area.getCentreY(), (float)area.getX() + 4,
                                 (float)area.getRight() - 4);
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour(juce::Colour(bg3));
            g.fillRoundedRectangle(area.reduced(2, 1).toFloat(), 3.f);
        }

        auto colour = isActive ? juce::Colour(txt0) : juce::Colour(txt2);
        if (textColourToUse) colour = *textColourToUse;

        g.setFont(juce::Font(13.0f));
        g.setColour(colour);

        auto textArea = area.reduced(8, 0);
        if (isTicked)
        {
            g.setColour(juce::Colour(accentBlue));
            g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x93"), area.getX(), area.getY(), 16, area.getHeight(), juce::Justification::centred);
        }
        g.drawFittedText(text, textArea.getX() + (isTicked ? 12 : 0), textArea.getY(),
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