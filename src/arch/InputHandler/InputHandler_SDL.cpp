#include "global.h"
#include "InputHandler_SDL.h"
#include "RageLog.h"
#include "RageDisplay.h"
#include "InputFilter.h"
#include "arch/LowLevelWindow/LowLevelWindow_SDL.h"

#include <SDL3/SDL.h>

REGISTER_INPUT_HANDLER_CLASS2(SDL3, SDL);

static DeviceButton SDLScancodeToDeviceButton(SDL_Scancode sc)
{
	// Letters
	if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
		return enum_add2(KEY_Ca, (int)(sc - SDL_SCANCODE_A));

	// Number row: SDL_SCANCODE_1..9, then 0
	if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
		return enum_add2(KEY_C1, (int)(sc - SDL_SCANCODE_1));
	if (sc == SDL_SCANCODE_0)
		return KEY_C0;

	// Function keys
	if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12)
		return enum_add2(KEY_F1, (int)(sc - SDL_SCANCODE_F1));
	if (sc == SDL_SCANCODE_F13) return KEY_F13;
	if (sc == SDL_SCANCODE_F14) return KEY_F14;
	if (sc == SDL_SCANCODE_F15) return KEY_F15;

	// Keypad digits
	if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9)
		return enum_add2(KEY_KP_C1, (int)(sc - SDL_SCANCODE_KP_1));
	if (sc == SDL_SCANCODE_KP_0)
		return KEY_KP_C0;

	switch (sc)
	{
	case SDL_SCANCODE_RETURN:       return KEY_ENTER;
	case SDL_SCANCODE_ESCAPE:       return KEY_ESC;
	case SDL_SCANCODE_BACKSPACE:    return KEY_BACK;
	case SDL_SCANCODE_TAB:          return KEY_TAB;
	case SDL_SCANCODE_SPACE:        return KEY_SPACE;
	case SDL_SCANCODE_PAUSE:        return KEY_PAUSE;
	case SDL_SCANCODE_INSERT:       return KEY_INSERT;
	case SDL_SCANCODE_HOME:         return KEY_HOME;
	case SDL_SCANCODE_END:          return KEY_END;
	case SDL_SCANCODE_PAGEUP:       return KEY_PGUP;
	case SDL_SCANCODE_PAGEDOWN:     return KEY_PGDN;
	case SDL_SCANCODE_DELETE:       return KEY_DEL;
	case SDL_SCANCODE_UP:           return KEY_UP;
	case SDL_SCANCODE_DOWN:         return KEY_DOWN;
	case SDL_SCANCODE_LEFT:         return KEY_LEFT;
	case SDL_SCANCODE_RIGHT:        return KEY_RIGHT;
	case SDL_SCANCODE_PRINTSCREEN:  return KEY_PRTSC;

	// Modifiers
	case SDL_SCANCODE_LSHIFT:       return KEY_LSHIFT;
	case SDL_SCANCODE_RSHIFT:       return KEY_RSHIFT;
	case SDL_SCANCODE_LCTRL:        return KEY_LCTRL;
	case SDL_SCANCODE_RCTRL:        return KEY_RCTRL;
	case SDL_SCANCODE_LALT:         return KEY_LALT;
	case SDL_SCANCODE_RALT:         return KEY_RALT;
	case SDL_SCANCODE_LGUI:         return KEY_LSUPER;
	case SDL_SCANCODE_RGUI:         return KEY_RSUPER;
	case SDL_SCANCODE_APPLICATION:  return KEY_MENU;
	case SDL_SCANCODE_NUMLOCKCLEAR: return KEY_NUMLOCK;
	case SDL_SCANCODE_CAPSLOCK:     return KEY_CAPSLOCK;
	case SDL_SCANCODE_SCROLLLOCK:   return KEY_SCRLLOCK;

	// Punctuation
	case SDL_SCANCODE_MINUS:        return KEY_HYPHEN;
	case SDL_SCANCODE_EQUALS:       return KEY_EQUAL;
	case SDL_SCANCODE_LEFTBRACKET:  return KEY_LBRACKET;
	case SDL_SCANCODE_RIGHTBRACKET: return KEY_RBRACKET;
	case SDL_SCANCODE_BACKSLASH:    return KEY_BACKSLASH;
	case SDL_SCANCODE_SEMICOLON:    return KEY_SEMICOLON;
	case SDL_SCANCODE_APOSTROPHE:   return KEY_SQUOTE;
	case SDL_SCANCODE_GRAVE:        return KEY_ACCENT;
	case SDL_SCANCODE_COMMA:        return KEY_COMMA;
	case SDL_SCANCODE_PERIOD:       return KEY_PERIOD;
	case SDL_SCANCODE_SLASH:        return KEY_SLASH;

	// Keypad operators
	case SDL_SCANCODE_KP_DIVIDE:    return KEY_KP_SLASH;
	case SDL_SCANCODE_KP_MULTIPLY:  return KEY_KP_ASTERISK;
	case SDL_SCANCODE_KP_MINUS:     return KEY_KP_HYPHEN;
	case SDL_SCANCODE_KP_PLUS:      return KEY_KP_PLUS;
	case SDL_SCANCODE_KP_ENTER:     return KEY_KP_ENTER;
	case SDL_SCANCODE_KP_PERIOD:    return KEY_KP_PERIOD;
	case SDL_SCANCODE_KP_EQUALS:    return KEY_KP_EQUAL;

	default:
		break;
	}

	return DeviceButton_Invalid;
}

static DeviceButton SDLMouseButtonToDeviceButton(Uint8 button)
{
	switch (button)
	{
	case SDL_BUTTON_LEFT:   return MOUSE_LEFT;
	case SDL_BUTTON_MIDDLE: return MOUSE_MIDDLE;
	case SDL_BUTTON_RIGHT:  return MOUSE_RIGHT;
	default:                return DeviceButton_Invalid;
	}
}

InputHandler_SDL::InputHandler_SDL()
{
	// Compute the offset between SDL's nanosecond tick epoch and RageTimer's epoch.
	// Both are monotonic clocks starting near program init, but may differ slightly.
	uint64_t sdlNs = SDL_GetTicksNS();
	uint64_t rageUs = RageTimer::GetTimeSinceStartMicroseconds();
	uint64_t sdlUs = sdlNs / 1000;
	m_iTimestampOffsetUs = (int64_t)rageUs - (int64_t)sdlUs;

	LOG->Info("InputHandler_SDL: timestamp offset = %lld us", (long long)m_iTimestampOffsetUs);
}

InputHandler_SDL::~InputHandler_SDL()
{
}

void InputHandler_SDL::Update()
{
	SDL_Event event;
	while (LowLevelWindow_SDL::PopInputEvent(event))
	{
		switch (event.type)
		{
		case SDL_EVENT_KEY_DOWN:
			if (!event.key.repeat)
				HandleKeyEvent(event.key, true);
			break;

		case SDL_EVENT_KEY_UP:
			HandleKeyEvent(event.key, false);
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			HandleMouseButtonEvent(event.button, true);
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			HandleMouseButtonEvent(event.button, false);
			break;

		case SDL_EVENT_MOUSE_MOTION:
			INPUTFILTER->UpdateCursorLocation(
				(int)event.motion.x, (int)event.motion.y);
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			if (event.wheel.y > 0)
				ButtonPressed(DeviceInput(DEVICE_MOUSE, MOUSE_WHEELUP, 1));
			else if (event.wheel.y < 0)
				ButtonPressed(DeviceInput(DEVICE_MOUSE, MOUSE_WHEELDOWN, 1));
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			INPUTFILTER->Reset();
			break;

		default:
			break;
		}
	}

	InputHandler::UpdateTimer();
}

void InputHandler_SDL::HandleKeyEvent(const SDL_KeyboardEvent &key, bool bDown)
{
	DeviceButton db = SDLScancodeToDeviceButton(key.scancode);
	if (db == DeviceButton_Invalid)
		return;

	// Convert SDL nanosecond timestamp to RageTimer
	uint64_t sdlUs = key.timestamp / 1000;
	uint64_t rageUs = sdlUs + m_iTimestampOffsetUs;
	RageTimer ts(rageUs / 1000000, rageUs % 1000000);

	DeviceInput di(DEVICE_KEYBOARD, db, bDown ? 1.0f : 0.0f, ts);
	ButtonPressed(di);
}

void InputHandler_SDL::HandleMouseButtonEvent(const SDL_MouseButtonEvent &button, bool bDown)
{
	DeviceButton db = SDLMouseButtonToDeviceButton(button.button);
	if (db == DeviceButton_Invalid)
		return;

	ButtonPressed(DeviceInput(DEVICE_MOUSE, db, bDown ? 1.0f : 0.0f));
}

void InputHandler_SDL::GetDevicesAndDescriptions(std::vector<InputDeviceInfo>& vDevicesOut)
{
	vDevicesOut.push_back(InputDeviceInfo(DEVICE_KEYBOARD, "Keyboard"));
	vDevicesOut.push_back(InputDeviceInfo(DEVICE_MOUSE, "Mouse"));
}
