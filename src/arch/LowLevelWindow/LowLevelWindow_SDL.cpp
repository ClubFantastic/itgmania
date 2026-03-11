#include "global.h"
#include "LowLevelWindow_SDL.h"
#include "RageLog.h"
#include "RageException.h"
#include "DisplaySpec.h"
#include "RageDisplay_OGL_Helpers.h"
#include "RageSurface.h"
#include "RageSurface_Load.h"
#include "arch/ArchHooks/ArchHooks.h"
#include "ImGuiManager.h"

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
	// Set app metadata before SDL_Init so the Wayland compositor can match
	// the window to the .desktop entry for the correct icon and taskbar entry.
	SDL_SetAppMetadata("ITGmania", nullptr, "itgmania");

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

void *LowLevelWindow_SDL::GetProcAddress(std::string s)
{
	return (void *)SDL_GL_GetProcAddress(s.c_str());
}

static SDL_DisplayID FindSDLDisplay(const std::string &sDisplayId)
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

std::string LowLevelWindow_SDL::TryVideoMode(const VideoModeParams &p, bool &bNewDeviceOut)
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
#if defined(HAS_GL3)
		// Request GL 3.3 core profile for the GL3 renderer
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

		SDL_PropertiesID props = SDL_CreateProperties();
		SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, p.sWindowTitle.c_str());
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, false);
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);

		if (p.bWindowIsFullscreenBorderless)
		{
			// For borderless fullscreen, create at a small default size and
			// set fullscreen immediately — SDL/compositor will resize to fill
			// the display. Creating at the native res would be interpreted as
			// logical coords which may be huge on scaled displays.
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 640);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 480);
			SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
		}
		else if (!p.windowed)
		{
			// Exclusive fullscreen — also start fullscreen
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, p.width);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, p.height);
			SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
		}
		else
		{
			// Windowed mode — position on the target display
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, p.width);
			SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, p.height);

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

		// Ensure context is current before GLEW init
		if (!SDL_GL_MakeCurrent(m_pWindow, m_GLContext))
			LOG->Warn("SDL_GL_MakeCurrent before glewInit: %s", SDL_GetError());

		// Initialize GLEW. glewExperimental is needed because glewInit()
		// calls glGetString(GL_EXTENSIONS) which returns NULL on core
		// profile contexts (GL 3.2+), causing a spurious error.
		glewExperimental = GL_TRUE;
		GLenum err = glewInit();
		if (GLEW_OK != err)
		{
			// GLEW may fail on EGL/Wayland contexts (error 4 =
			// GLEW_ERROR_GLX_VERSION_11_ONLY) because it probes GLX
			// which doesn't exist. If GL functions work, continue.
			if (glGetString(GL_VERSION) != nullptr)
			{
				LOG->Info("glewInit returned error %d (%s) but GL context is functional; continuing",
					(int)err, glewGetErrorString(err));
			}
			else
			{
				return ssprintf("glewInit failed: %s", glewGetErrorString(err));
			}
		}
		// glewInit may set GL_INVALID_ENUM with glewExperimental; clear it.
		glGetError();
	}
	else
	{
		// Reuse existing window, just resize
		SDL_SetWindowSize(m_pWindow, p.width, p.height);
	}

	// Handle fullscreen mode.
	// In ITGmania, "fullscreen borderless" uses windowed=true with
	// bWindowIsFullscreenBorderless=true. SDL3 handles this as desktop
	// fullscreen (NULL mode = use desktop resolution, no mode switch).
	if (p.bWindowIsFullscreenBorderless)
	{
		SDL_SetWindowFullscreenMode(m_pWindow, nullptr);
		SDL_SetWindowFullscreen(m_pWindow, true);
	}
	else if (!p.windowed)
	{
		// Exclusive fullscreen — find closest matching display mode
		SDL_DisplayID targetDisplay = FindSDLDisplay(p.sDisplayId);
		SDL_DisplayMode closest;
		bool found = SDL_GetClosestFullscreenDisplayMode(
			targetDisplay, p.width, p.height,
			p.rate > 0 ? (float)p.rate : 0.0f, false, &closest);

		if (found)
			SDL_SetWindowFullscreenMode(m_pWindow, &closest);
		else
			SDL_SetWindowFullscreenMode(m_pWindow, nullptr);

		SDL_SetWindowFullscreen(m_pWindow, true);
	}
	else
	{
		SDL_SetWindowFullscreen(m_pWindow, false);
	}

	// Window icon
	if (!p.sIconFile.empty())
	{
		std::string sError;
		RageSurface *pIcon = RageSurfaceUtils::LoadFile(p.sIconFile, sError);
		if (pIcon)
		{
			SDL_Surface *pSDLIcon = SDL_CreateSurfaceFrom(
				pIcon->w, pIcon->h,
				SDL_GetPixelFormatForMasks(
					pIcon->fmt.BitsPerPixel,
					pIcon->fmt.Rmask, pIcon->fmt.Gmask,
					pIcon->fmt.Bmask, pIcon->fmt.Amask),
				pIcon->pixels, pIcon->pitch);
			if (pSDLIcon)
			{
				SDL_SetWindowIcon(m_pWindow, pSDLIcon);
				SDL_DestroySurface(pSDLIcon);
			}
			delete pIcon;
		}
	}

	// VSync
	SDL_GL_SetSwapInterval(p.vsync ? 1 : 0);

	// Screensaver
	SDL_DisableScreenSaver();

	// Gather all the size information SDL gives us.
	m_CurrentParams = ActualVideoModeParams(p);
	int logW, logH, pixW, pixH;
	SDL_GetWindowSize(m_pWindow, &logW, &logH);
	SDL_GetWindowSizeInPixels(m_pWindow, &pixW, &pixH);
	float scale = SDL_GetWindowDisplayScale(m_pWindow);

	LOG->Info("SDL TryVideoMode: logical %dx%d, pixels %dx%d, scale %.2f",
		logW, logH, pixW, pixH, scale);

	// Use the actual pixel dimensions for the GL viewport.
	m_CurrentParams.windowWidth = pixW;
	m_CurrentParams.windowHeight = pixH;

	// For fullscreen borderless, the render size should match the pixel size
	if (p.bWindowIsFullscreenBorderless)
	{
		m_CurrentParams.width = pixW;
		m_CurrentParams.height = pixH;
	}

	return std::string(); // success
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
		ImGuiManager::ProcessEvent(event);

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

bool LowLevelWindow_SDL::IsSoftwareRenderer(std::string &sError)
{
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	if (renderer)
	{
		std::string sRenderer(renderer);
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
