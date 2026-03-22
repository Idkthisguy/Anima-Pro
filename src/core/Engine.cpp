// --- LIBRARIES ---
#include "Anima.h"
#include <SDL.h> // helps make the window and handle input
#include <SDL_opengl.h> // renders the window
#include "imgui.h" // the UI library
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
// ---------------------

namespace Anima {
	struct Engine::internalGears {
		SDL_Window* window = nullptr;
		SDL_GLContext glContext = nullptr;
	};

	Engine::Engine() : m_KeepRunning(false) {
		m_Gears = new internalGears();
	}
    Engine::~Engine() {
        shutdown();
        delete m_Gears;
    }

    bool Engine::Startup(const std::string& name, int w, int h) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        m_Gears->window = SDL_CreateWindow(
            name.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            w, h,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        );

        if (!m_Gears->window) return false;

        m_Gears->glContext = SDL_GL_CreateContext(m_Gears->window);
        SDL_GL_MakeCurrent(m_Gears->window, m_Gears->glContext);

        SDL_GL_SetSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplSDL2_InitForOpenGL(m_Gears->window, m_Gears->glContext);
        ImGui_ImplOpenGL3_Init("#version 130");

        m_KeepRunning = true;
        return true;
    }

    void Engine::syncInput() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) m_KeepRunning = false;
        }
    }

    void Engine::BeginFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void Engine::EndFrame() {
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(m_Gears->window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(m_Gears->window);
    }

    void Engine::shutdown() {
        if (!m_Gears) return;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        if (m_Gears->glContext) SDL_GL_DeleteContext(m_Gears->glContext);
        if (m_Gears->window) SDL_DestroyWindow(m_Gears->window);
        SDL_Quit();
        m_KeepRunning = false;
    }


}