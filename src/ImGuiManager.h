#ifndef IMGUI_MANAGER_H
#define IMGUI_MANAGER_H

#if defined(HAS_SDL3)
#include <SDL3/SDL.h>
#endif

namespace ImGuiManager
{
	void Initialize(SDL_Window* window, SDL_GLContext gl_context);
	void Shutdown();
	void ProcessEvent(const SDL_Event& event);
	void NewFrame();
	void Render();
	bool WantsKeyboard();
	bool WantsMouse();
}

#endif
