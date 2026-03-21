/* LowLevelWindow_SDL - SDL3-based window driver. */

#ifndef LOW_LEVEL_WINDOW_SDL_H
#define LOW_LEVEL_WINDOW_SDL_H

#include "RageDisplay.h" // VideoModeParams
#include "LowLevelWindow.h"

#include <SDL3/SDL.h>

#include <vector>
#include <mutex>

class LowLevelWindow_SDL : public LowLevelWindow
{
public:
	LowLevelWindow_SDL();
	~LowLevelWindow_SDL();

	void *GetProcAddress(std::string s);
	std::string TryVideoMode(const VideoModeParams &p, bool &bNewDeviceOut);
	void LogDebugInformation() const;
	bool IsSoftwareRenderer( std::string &sError );
	void SwapBuffers();
	void Update();

	const ActualVideoModeParams GetActualVideoModeParams() const { return m_CurrentParams; }

	void GetDisplaySpecs(DisplaySpecs &out) const;

	bool SupportsRenderToTexture() const;
	RenderTarget *CreateRenderTarget() { return nullptr; }

	bool SupportsFullscreenBorderlessWindow() const { return true; }

	bool SupportsThreadedRendering();
	void BeginConcurrentRenderingMainThread();
	void EndConcurrentRenderingMainThread();
	void BeginConcurrentRendering();
	void EndConcurrentRendering();

	// InputHandler_SDL reads input events from this queue.
	static bool PopInputEvent(SDL_Event &out);

	SDL_Window* GetWindow() const { return m_pWindow; }
	SDL_GLContext GetGLContext() const { return m_GLContext; }

private:
	SDL_Window *m_pWindow;
	SDL_GLContext m_GLContext;
	SDL_GLContext m_GLBackgroundContext;
	ActualVideoModeParams m_CurrentParams;

	// Input event queue shared with InputHandler_SDL
	static std::mutex s_EventQueueMutex;
	static std::vector<SDL_Event> s_InputEventQueue;
};

#ifdef ARCH_LOW_LEVEL_WINDOW
#error "More than one LowLevelWindow selected!"
#endif
#define ARCH_LOW_LEVEL_WINDOW LowLevelWindow_SDL

#endif
