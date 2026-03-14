#include "global.h"
#include "ImGuiManager.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#if defined(HAS_GL3)
#include "imgui_impl_opengl3.h"
#else
#include "imgui_impl_opengl2.h"
#endif

#include "RageLog.h"
#include "RageFile.h"
#include "RageFileManager.h"
#include "RageUtil.h"
#include "ThemeManager.h"

namespace ImGuiManager
{
	static bool s_bInitialized = false;

	static void LoadThemeFonts()
	{
		if (!THEME || !THEME->IsThemeLoaded())
		{
			LOG->Info("ImGuiManager: no theme loaded, using default font");
			return;
		}

		std::string sFontDir = THEME->GetCurThemeDir() + "TrueTypeFonts/";

		std::vector<std::string> vsFonts;
		std::vector<std::string> exts = { "ttf", "otf" };
		FILEMAN->GetDirListingWithMultipleExtensions(sFontDir, exts, vsFonts, false, true);

		if (vsFonts.empty())
		{
			LOG->Info("ImGuiManager: no fonts in %s, using default", sFontDir.c_str());
			return;
		}

		ImGuiIO& io = ImGui::GetIO();

		for (const std::string& sPath : vsFonts)
		{
			RageFile f;
			if (!f.Open(sPath))
			{
				LOG->Warn("ImGuiManager: failed to open %s: %s", sPath.c_str(), f.GetError().c_str());
				continue;
			}

			int iSize = f.GetFileSize();
			if (iSize <= 0)
			{
				LOG->Warn("ImGuiManager: empty font file %s", sPath.c_str());
				continue;
			}

			// ImGui takes ownership of this allocation (must be allocated with IM_ALLOC)
			void* pData = IM_ALLOC(iSize);
			if (f.Read(pData, iSize) != iSize)
			{
				LOG->Warn("ImGuiManager: failed to read %s", sPath.c_str());
				IM_FREE(pData);
				continue;
			}

			ImFontConfig config;
			config.FontDataOwnedByAtlas = true;

			// Extract just the filename for the font name
			size_t iSlash = sPath.find_last_of('/');
			std::string sName = (iSlash != std::string::npos) ? std::string(sPath.substr(iSlash + 1)) : sPath;
			snprintf(config.Name, sizeof(config.Name), "%s", sName.c_str());

			ImFont* pFont = io.Fonts->AddFontFromMemoryTTF(pData, iSize, 16.0f, &config);
			if (pFont)
				LOG->Info("ImGuiManager: loaded font '%s'", sName.c_str());
			else
				LOG->Warn("ImGuiManager: failed to add font '%s'", sName.c_str());
		}
	}

	void Initialize(SDL_Window* window, SDL_GLContext gl_context)
	{
		if (s_bInitialized)
			return;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		LoadThemeFonts();

		ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
#if defined(HAS_GL3)
#ifdef EMSCRIPTEN
		ImGui_ImplOpenGL3_Init("#version 300 es");
		LOG->Info("ImGuiManager: initialized (OpenGL ES 3.0 backend)");
#else
		ImGui_ImplOpenGL3_Init("#version 330 core");
		LOG->Info("ImGuiManager: initialized (OpenGL 3.3 backend)");
#endif
#else
		ImGui_ImplOpenGL2_Init();
		LOG->Info("ImGuiManager: initialized (OpenGL 2 backend)");
#endif

		s_bInitialized = true;
	}

	void Shutdown()
	{
		if (!s_bInitialized)
			return;

#if defined(HAS_GL3)
		ImGui_ImplOpenGL3_Shutdown();
#else
		ImGui_ImplOpenGL2_Shutdown();
#endif
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();

		s_bInitialized = false;
		LOG->Info("ImGuiManager: shut down");
	}

	void ProcessEvent(const SDL_Event& event)
	{
		if (!s_bInitialized)
			return;
		ImGui_ImplSDL3_ProcessEvent(&event);
	}

	void NewFrame()
	{
		if (!s_bInitialized)
			return;

#if defined(HAS_GL3)
		ImGui_ImplOpenGL3_NewFrame();
#else
		ImGui_ImplOpenGL2_NewFrame();
#endif
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		// Proof-of-life: show demo window
		ImGui::ShowDemoWindow();
	}

	void Render()
	{
		if (!s_bInitialized)
			return;

		ImGui::Render();
#if defined(HAS_GL3)
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#else
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#endif
	}

	bool WantsKeyboard()
	{
		if (!s_bInitialized)
			return false;
		return ImGui::GetIO().WantCaptureKeyboard;
	}

	bool WantsMouse()
	{
		if (!s_bInitialized)
			return false;
		return ImGui::GetIO().WantCaptureMouse;
	}
}
