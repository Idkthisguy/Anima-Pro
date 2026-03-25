// --- LIBRARIES ---
#include <SDL.h> // helps make the window and handle input
#include <SDL_opengl.h> // renders the window
#include "imgui.h" // the UI library
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "Engine.h"
// ---------------------

namespace Anima {
	struct Engine::internalGears {
		SDL_Window* window = nullptr;
		SDL_GLContext glContext = nullptr;

        float idleTime = 0.0f;
        bool highMemoryMode = true;
        bool isPowerSaving = false;
        unsigned int canvasTextureID = 0;
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

    void Engine::freeUnusedMemory() {
        if (m_Gears->isPowerSaving) return;

        // saveToTempCache("temp_session.bak"); // (You'll implement this later)

        // The Swap Trick to instantly delete memory
        // std::vector<std::vector<Pixel>>().swap(undoStack);
        // std::vector<Pixel>().swap(currentCanvasPixels);

        if (m_Gears->canvasTextureID != 0) {
            glDeleteTextures(1, &m_Gears->canvasTextureID);
            m_Gears->canvasTextureID = 0;
        }

        m_Gears->isPowerSaving = true;
        printf("Anima: Memory Released. Entering Sleep Mode.\n");
    }

    void Engine::reloadResources() {
        if (!m_Gears->isPowerSaving) return;

        // currentCanvasPixels.clear();
        // loadFromTempCacheDirectlyInto(currentCanvasPixels);
        // m_Gears->canvasTextureID = createTextureFromPixels(currentCanvasPixels);

        m_Gears->isPowerSaving = false;
        printf("Anima: Waking up! Reload complete.\n");
    }

    void Engine::update(float deltaTime) {
        ImGuiIO& io = ImGui::GetIO();

        bool userActive = (io.MouseDelta.x != 0 || io.MouseDelta.y != 0 || io.InputQueueCharacters.Size > 0 || io.MouseDown[0]);

        if (userActive) {
            m_Gears->idleTime = 0.0f;
            if (!m_Gears->highMemoryMode) {
                m_Gears->highMemoryMode = true;
                reloadResources();
                SDL_GL_SetSwapInterval(1); // Turn VSync back on
            }
        }
        else {
            m_Gears->idleTime += deltaTime;
        }

        if (m_Gears->idleTime > 5.0f && m_Gears->highMemoryMode) {
            m_Gears->highMemoryMode = false;
            freeUnusedMemory();
            SDL_GL_SetSwapInterval(0); // Drop framerate to save laptop battery
        }

        if (!m_Gears->highMemoryMode) {
            SDL_Delay(100); // Sleep for 100ms so the CPU rests
        }
    }
}