#pragma once
#include <JuceHeader.h>

// A custom, screen-reader-friendly menu bar. JUCE's built-in MenuBarComponent
// doesn't expose individual items well to accessibility tools, so this
// implements a menu bar out of ordinary TextButtons with proper roles/titles.

class MainComponent; // forward declaration, only used as an owner pointer here

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
            btn->onClick = [this, idx]() { showMenu(idx); };
            btn->addKeyListener(this);

            addAndMakeVisible(btn);
            buttons.add(btn);
        }
        resized();
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component* src) override
    {
        // Only handle Enter/Return to open the focused menu button.
        // Left/Right are intentionally NOT handled here so they always
        // reach MainComponent for timeline scrubbing.
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
        if (model == nullptr || index < 0 || index >= buttons.size()) return;
        auto menu = model->getMenuForIndex(index, {});
        auto* btn = buttons[index];

        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(btn),
            [this, index](int result) {
                if (result != 0 && model != nullptr)
                    model->menuItemSelected(result, index);
            });
    }

    void openMenuByIndex(int index) { showMenu(index); }

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
    juce::OwnedArray<MenuBarButton> buttons;
};
