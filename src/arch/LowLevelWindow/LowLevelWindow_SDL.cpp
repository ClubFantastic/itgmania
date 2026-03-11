#include "global.h"
#include "LowLevelWindow_SDL.h"
#include "RageLog.h"
#include "RageException.h"
#include "DisplaySpec.h"
#include "RageDisplay_OGL_Helpers.h"
#include "arch/ArchHooks/ArchHooks.h"

#include <GL/glew.h>
#include <SDL3/SDL.h>

// Static members for input event queue
std::mutex LowLevelWindow_SDL::s_EventQueueMutex;
std::vector<SDL_Event> LowLevelWindow_SDL::s_InputEventQueue;

LowLevelWindow_SDL::LowLevelWindow_SDL()
	: m_pWindow(nullptr),
	  m_GLContext(nullptr),
	  m_GLBackgroundContext(nullptr)
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		RageException::Throw("SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
	}

	LOG->Info("SDL video initialized (driver: %s)", SDL_GetCurrentVideoDriver());
}

LowLevelWindow_SDL::~LowLevelWindow_SDL()
{
	if (m_GLBackgroundContext)
		SDL_GL_DestroyContext(m_GLBackgroundContext);
	if (m_GLContext)
		SDL_GL_DestroyContext(m_GLContext);
	if (m_pWindow)
		SDL_DestroyWindow(m_pWindow);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void *LowLevelWindow_SDL::GetProcAddress(RString s)
{
	return (void *)SDL_GL_GetProcAddress(s.c_str());
}

static SDL_DisplayID FindSDLDisplay(const RString &sDisplayId)
{
	if (sDisplayId.empty())
		return SDL_GetPrimaryDisplay();

	int count = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&count);
	if (!displays)
		return SDL_GetPrimaryDisplay();

	SDL_DisplayID found = SDL_GetPrimaryDisplay();
	for (int i = 0; i < count; i++)
	{
		const char *name = SDL_GetDisplayName(displays[i]);
		if (name && sDisplayId == name)
		{
			found = displays[i];
			break;
		}
		// Also try matching by stringified ID
		if (ssprintf("%d", (int)displays[i]) == sDisplayId)
		{
			found = displays[i];
			break;
		}
	}
	SDL_free(displays);
	return found;
}

RString LowLevelWindow_SDL::TryVideoMode(const VideoModeParams &p, bool &bNewDeviceOut)
{
	bNewDeviceOut = false;

	// Determine if we need a new window/context
	bool bNeedNewWindow = (m_pWindow == nullptr);

	if (bNeedNewWindow)
	{
		bNewDeviceOut = true;

		// Set GL attributes before window creation
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		// Enable context sharing for threaded rendering
		SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

		SDL_PropertiesID props = SDL_CreateProperties();
		SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, p.sWindowTitle.c_str());
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, p.width);
		SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, p.height);
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, false);
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, false);

		// Position on the target display
		SDL_DisplayID targetDisplay = FindSDLDisplay(p.sDisplayId);
		if (targetDisplay)
		{
			SDL_Rect bounds;
			if (SDL_GetDisplayBounds(targetDisplay, &bounds))
			{
				SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER,
					bounds.x + (bounds.w - p.width) / 2);
				SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER,
					bounds.y + (bounds.h - p.height) / 2);
			}
		}

		m_pWindow = SDL_CreateWindowWithProperties(props);
		SDL_DestroyProperties(props);

		if (!m_pWindow)
			return ssprintf("SDL_CreateWindow failed: %s", SDL_GetError());

		m_GLContext = SDL_GL_CreateContext(m_pWindow);
		if (!m_GLContext)
		{
			SDL_DestroyWindow(m_pWindow);
			m_pWindow = nullptr;
			return ssprintf("SDL_GL_CreateContext failed: %s", SDL_GetError());
		}

		// Create shared background context for threaded rendering
		m_GLBackgroundContext = SDL_GL_CreateContext(m_pWindow);
		if (m_GLBackgroundContext)
		{
			// Switch back to the main context
			SDL_GL_MakeCurrent(m_pWindow, m_GLContext);
		}

		// Initialize GLEW
		GLenum err = glewInit();
		if (GLEW_OK != err)
			return ssprintf("glewInit failed: %s", glewGetErrorString(err));
	}
	else
	{
		// Reuse existing window, just resize
		SDL_SetWindowSize(m_pWindow, p.width, p.height);
	}

	// Handle fullscreen mode
	if (!p.windowed)
	{
		if (p.bWindowIsFullscreenBorderless)
		{
			// Desktop fullscreen (borderless) — pass NULL mode
			SDL_SetWindowFullscreenMode(m_pWindow, nullptr);
		}
		else
		{
			// Exclusive fullscreen — find closest matching mode
			SDL_DisplayID targetDisplay = FindSDLDisplay(p.sDisplayId);
			SDL_DisplayMode closest;
			bool found = SDL_GetClosestFullscreenDisplayMode(
				targetDisplay, p.width, p.height,
				p.rate > 0 ? (float)p.rate : 0.0f, false, &closest);

			if (found)
				SDL_SetWindowFullscreenMode(m_pWindow, &closest);
			else
				SDL_SetWindowFullscreenMode(m_pWindow, nullptr); // fallback to desktop
		}
		SDL_SetWindowFullscreen(m_pWindow, true);
	}
	else
	{
		SDL_SetWindowFullscreen(m_pWindow, false);
	}

	// VSync
	SDL_GL_SetSwapInterval(p.vsync ? 1 : 0);

	// Screensaver
	SDL_DisableScreenSaver();

	// Update current params.
	// Use pixel dimensions (not logical/screen coordinates) since these feed
	// directly into glViewport. On HiDPI/scaled displays, SDL_GetWindowSize
	// returns logical coords (e.g. 1536x864 at 2.5x scale) while
	// SDL_GetWindowSizeInPixels returns the actual framebuffer size (3840x2160).
	m_CurrentParams = ActualVideoModeParams(p);
	int pixW, pixH;
	SDL_GetWindowSizeInPixels(m_pWindow, &pixW, &pixH);
	m_CurrentParams.windowWidth = pixW;
	m_CurrentParams.windowHeight = pixH;

	// For fullscreen borderless, the render size should match the pixel size
	if (p.bWindowIsFullscreenBorderless)
	{
		m_CurrentParams.width = pixW;
		m_CurrentParams.height = pixH;
	}

	return RString(); // success
}

void LowLevelWindow_SDL::GetDisplaySpecs(DisplaySpecs &out) const
{
	out.clear();

	int displayCount = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&displayCount);
	if (!displays)
		return;

	for (int i = 0; i < displayCount; i++)
	{
		SDL_DisplayID displayID = displays[i];
		const char *name = SDL_GetDisplayName(displayID);
		std::string displayName = name ? std::string(name) : std::string(ssprintf("Display %d", i));
		std::string displayIdStr = ssprintf("%d", (int)displayID);

		// Get supported modes
		std::set<DisplayMode> modes;
		int modeCount = 0;
		SDL_DisplayMode **sdlModes = SDL_GetFullscreenDisplayModes(displayID, &modeCount);
		if (sdlModes)
		{
			for (int m = 0; m < modeCount; m++)
			{
				DisplayMode mode;
				mode.width = sdlModes[m]->w;
				mode.height = sdlModes[m]->h;
				mode.refreshRate = sdlModes[m]->refresh_rate;
				modes.insert(mode);
			}
			SDL_free(sdlModes);
		}

		// Get current mode
		const SDL_DisplayMode *curSDLMode = SDL_GetCurrentDisplayMode(displayID);

		// Get display bounds
		SDL_Rect bounds;
		SDL_GetDisplayBounds(displayID, &bounds);
		RectI curBounds(bounds.x, bounds.y, bounds.x + bounds.w, bounds.y + bounds.h);

		if (curSDLMode && !modes.empty())
		{
			DisplayMode curMode;
			curMode.width = curSDLMode->w;
			curMode.height = curSDLMode->h;
			curMode.refreshRate = curSDLMode->refresh_rate;

			// Ensure current mode is in the set
			modes.insert(curMode);

			out.insert(DisplaySpec(displayIdStr, displayName, modes, curMode, curBounds));
		}
		else if (!modes.empty())
		{
			out.insert(DisplaySpec(displayIdStr, displayName, modes));
		}
	}

	SDL_free(displays);
}

void LowLevelWindow_SDL::SwapBuffers()
{
	SDL_GL_SwapWindow(m_pWindow);
}

void LowLevelWindow_SDL::Update()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_EVENT_QUIT:
			ArchHooks::SetUserQuit();
			break;

		// Route input events to the shared queue for InputHandler_SDL
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_WHEEL:
		case SDL_EVENT_WINDOW_FOCUS_LOST:
		{
			std::lock_guard<std::mutex> lock(s_EventQueueMutex);
			s_InputEventQueue.push_back(event);
			break;
		}

		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		{
			m_CurrentParams.windowWidth = event.window.data1;
			m_CurrentParams.windowHeight = event.window.data2;
			break;
		}

		default:
			break;
		}
	}
}

bool LowLevelWindow_SDL::PopInputEvent(SDL_Event &out)
{
	std::lock_guard<std::mutex> lock(s_EventQueueMutex);
	if (s_InputEventQueue.empty())
		return false;
	out = s_InputEventQueue.front();
	s_InputEventQueue.erase(s_InputEventQueue.begin());
	return true;
}

void LowLevelWindow_SDL::LogDebugInformation() const
{
	LOG->Info("SDL Video Driver: %s", SDL_GetCurrentVideoDriver());
	LOG->Info("GL Vendor: %s", glGetString(GL_VENDOR));
	LOG->Info("GL Renderer: %s", glGetString(GL_RENDERER));
	LOG->Info("GL Version: %s", glGetString(GL_VERSION));
}

bool LowLevelWindow_SDL::IsSoftwareRenderer(RString &sError)
{
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	if (renderer)
	{
		RString sRenderer(renderer);
		sRenderer.MakeLower();
		if (sRenderer.find("software") != std::string::npos ||
			sRenderer.find("llvmpipe") != std::string::npos ||
			sRenderer.find("swrast") != std::string::npos)
		{
			sError = ssprintf("Software renderer detected: %s", renderer);
			return true;
		}
	}
	return false;
}

bool LowLevelWindow_SDL::SupportsRenderToTexture() const
{
	// Let RageDisplay_OGL use FBOs (GLEW_EXT_framebuffer_object).
	// We don't provide platform-specific pbuffer render targets.
	return false;
}

bool LowLevelWindow_SDL::SupportsThreadedRendering()
{
	return m_GLBackgroundContext != nullptr;
}

void LowLevelWindow_SDL::BeginConcurrentRenderingMainThread()
{
	// Release the main GL context so the render thread can use it
	SDL_GL_MakeCurrent(m_pWindow, m_GLBackgroundContext);
}

void LowLevelWindow_SDL::EndConcurrentRenderingMainThread()
{
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);
}

void LowLevelWindow_SDL::BeginConcurrentRendering()
{
	SDL_GL_MakeCurrent(m_pWindow, m_GLContext);
}

void LowLevelWindow_SDL::EndConcurrentRendering()
{
	SDL_GL_MakeCurrent(m_pWindow, nullptr);
}
