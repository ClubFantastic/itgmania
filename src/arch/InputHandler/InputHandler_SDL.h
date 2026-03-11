/* InputHandler_SDL - SDL3-based keyboard/mouse input handler. */

#ifndef INPUT_HANDLER_SDL_H
#define INPUT_HANDLER_SDL_H

#include "InputHandler.h"

#include <SDL3/SDL.h>

class InputHandler_SDL: public InputHandler
{
public:
	InputHandler_SDL();
	~InputHandler_SDL();
	void Update();
	void GetDevicesAndDescriptions( std::vector<InputDeviceInfo>& vDevicesOut );

private:
	void HandleKeyEvent(const SDL_KeyboardEvent &key, bool bDown);
	void HandleMouseButtonEvent(const SDL_MouseButtonEvent &button, bool bDown);

	// Offset between SDL tick epoch and RageTimer epoch, in microseconds
	int64_t m_iTimestampOffsetUs;
};

#endif
