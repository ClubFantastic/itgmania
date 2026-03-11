#include "global.h"
#include "ImGuiManager.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl2.h"

#include "RageLog.h"

namespace ImGuiManager
{
	static bool s_bInitialized = false;

	void Initialize(SDL_Window* window, SDL_GLContext gl_context)
	{
		if (s_bInitialized)
			return;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
		ImGui_ImplOpenGL2_Init();

		s_bInitialized = true;
		LOG->Info("ImGuiManager: initialized");
	}

	void Shutdown()
	{
		if (!s_bInitialized)
			return;

		ImGui_ImplOpenGL2_Shutdown();
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

		ImGui_ImplOpenGL2_NewFrame();
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
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
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
