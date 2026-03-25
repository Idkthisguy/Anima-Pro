#include "Engine.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"

namespace Anima {

	Engine::Engine()
		: m_KeepRunning(false), m_Gears(nullptr)
	{
	}

	Engine::~Engine()
	{
		shutdown();
	}

	bool Engine::Startup(const std::string& name, int w, int h)
	{
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
			return false;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		m_Gears = new internalGears();

		m_Gears->window = SDL_CreateWindow(
			name.c_str(),
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			w, h,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
		);

		if (!m_Gears->window)
			return false;

		m_Gears->glContext = SDL_GL_CreateContext(m_Gears->window);
		if (!m_Gears->glContext)
			return false;

		SDL_GL_MakeCurrent(m_Gears->window, m_Gears->glContext);
		SDL_GL_SetSwapInterval(1);

		m_KeepRunning = true;
		return true;
	}

	void Engine::syncInput()
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				m_KeepRunning = false;
		}
	}

	void Engine::BeginFrame()
	{
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void Engine::drawInterface()
	{
	}

	void Engine::EndFrame()
	{
		SDL_GL_SwapWindow(m_Gears->window);
	}

	void Engine::shutdown()
	{
		if (!m_Gears) return;

		if (m_Gears->glContext)
			SDL_GL_DeleteContext(m_Gears->glContext);

		if (m_Gears->window)
			SDL_DestroyWindow(m_Gears->window);

		delete m_Gears;
		m_Gears = nullptr;

		SDL_Quit();
	}

}
