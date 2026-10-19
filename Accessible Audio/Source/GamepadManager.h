#pragma once
#include <JuceHeader.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
//#include <SDL.h>   // SDL2 (add include path + link SDL2.lib / SDL2main not needed)

// Polls an SDL GameController (PS5 DualSense, PS4, Xbox...) on the JUCE message
// thread, so callbacks can safely call UI / transport code directly.
class GamepadManager : private juce::Timer
{
public:
	// Fired once when a button goes down. Use SDL_CONTROLLER_BUTTON_* values.
	std::function<void(SDL_GameControllerButton)> onButtonDown;
	// Fired ~60x/sec while the stick is outside the dead zone. x,y in [-1,1].
	std::function<void(float x, float y)> onLeftStick, onRightStick;
	// Fired ~60x/sec with trigger pressure in [0,1] while pressed.
	std::function<void(float left, float right)> onTriggers;

	GamepadManager()
	{
		// Needed so events keep flowing when the JUCE window has focus but SDL has no window.
		SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
		// Enables full DualSense reports (rumble, LED) over Bluetooth.
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");

		if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
		{
			DBG("SDL init failed: " << SDL_GetError());
			return;
		}
		startTimerHz(60);
	}

	~GamepadManager() override
	{
		*aliveFlag = false;
		stopTimer();
		closePad();
		SDL_Quit();
	}

	bool isConnected() const { return pad != nullptr; }

	// lowFreq / highFreq: 0..1 (big motor / small motor), duration in ms.
	void rumble(float lowFreq, float highFreq, int ms)
	{
		if (pad == nullptr) return;
		SDL_GameControllerRumble(pad,
			(Uint16)(juce::jlimit(0.0f, 1.0f, lowFreq) * 65535.0f),
			(Uint16)(juce::jlimit(0.0f, 1.0f, highFreq) * 65535.0f),
			(Uint32)ms);
	}

	// Handy named patterns that work as non-visual / non-audio feedback.
	void tick() { rumble(1.0f, 1.0f, 75); }   // small click
	void bump() { rumble(1.0f, 1.0f, 200); }   // e.g. "start/end of file"
	void warning() { rumble(1.0f, 1.0f, 300); }   // e.g. "nothing to undo"

	struct Pulse { float low; float high; int ms; int gapAfterMs; };

	// Plays a sequence of pulses, e.g. playPattern({ {0,0.6f,40,60}, {0,0.6f,40,0} });
	void playPattern(std::initializer_list<Pulse> pulses)
	{
		int startAt = 0;
		for (const auto& p : pulses)
		{
			juce::Timer::callAfterDelay(startAt, [this, alive = aliveFlag, p]
				{
					if (*alive)                       // object may have been destroyed
						rumble(p.low, p.high, p.ms);
				});
			startAt += p.ms + p.gapAfterMs;
		}
	}

	// Ready-made patterns
	void doubleTick() { playPattern({ {1.0f, 1.0f, 75, 75}, {1.0f, 1.0f, 75, 0} }); }
	void doubleBump() { playPattern({ {0.7f, 0.3f, 120, 80}, {0.7f, 0.3f, 120, 0} }); }

	void setLightbar(juce::Colour c)
	{
		if (pad != nullptr)
			SDL_GameControllerSetLED(pad, c.getRed(), c.getGreen(), c.getBlue());
	}

private:
	std::shared_ptr<bool> aliveFlag = std::make_shared<bool>(true);
	void timerCallback() override
	{
		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_CONTROLLERDEVICEADDED:
				if (pad == nullptr) openPad(e.cdevice.which);
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				if (pad != nullptr && e.cdevice.which == padInstanceId) closePad();
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				if (onButtonDown)
					onButtonDown((SDL_GameControllerButton)e.cbutton.button);
				break;
			default: break;
			}
		}

		if (pad == nullptr) return;

		auto axis = [this](SDL_GameControllerAxis a) {
			return juce::jlimit(-1.0f, 1.0f, SDL_GameControllerGetAxis(pad, a) / 32767.0f);
			};
		auto dead = [](float v) { return std::abs(v) < 0.2f ? 0.0f : v; };

		float lx = dead(axis(SDL_CONTROLLER_AXIS_LEFTX)), ly = dead(axis(SDL_CONTROLLER_AXIS_LEFTY));
		float rx = dead(axis(SDL_CONTROLLER_AXIS_RIGHTX)), ry = dead(axis(SDL_CONTROLLER_AXIS_RIGHTY));
		float lt = axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT), rt = axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

		if (onLeftStick && (lx != 0.0f || ly != 0.0f)) onLeftStick(lx, ly);
		if (onRightStick && (rx != 0.0f || ry != 0.0f)) onRightStick(rx, ry);
		if (onTriggers && (lt > 0.05f || rt > 0.05f)) onTriggers(lt, rt);
	}

	void openPad(int deviceIndex)
	{
		if (!SDL_IsGameController(deviceIndex)) return;
		pad = SDL_GameControllerOpen(deviceIndex);
		if (pad != nullptr)
		{
			padInstanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
			setLightbar(juce::Colours::blue);
			juce::AccessibilityHandler::postAnnouncement(
				"Controller connected.", juce::AccessibilityHandler::AnnouncementPriority::high);
		}
	}

	void closePad()
	{
		if (pad != nullptr) { SDL_GameControllerClose(pad); pad = nullptr; }
	}

	SDL_GameController* pad = nullptr;
	SDL_JoystickID padInstanceId = -1;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GamepadManager)
};