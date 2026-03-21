#pragma once
#include "Anima.h"
#include <SDL.h>
#include <SDL_opengl.h>

namespace Anima {
	struct Engine::internalGears {
		SDL_Window* window;
		SDL_GLContext glContext = nullptr;
		
		float deltaTime = 0.0f;
		uint64_t lastFrameTime = 0;
	};
}