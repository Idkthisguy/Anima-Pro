#pragma once
#include "Anima.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include "imgui.h"

namespace Anima {
	struct Engine::internalGears {
		SDL_Window* window;
		SDL_GLContext glContext = nullptr;
		
		float deltaTime = 0.0f;
		uint64_t lastFrameTime = 0;
	};

	void Engine::freeUnusedMemory() {
		if (gears->isPowerSaving) return;

		saveToTempCache("temp_session.bak");

		undoStack.clear();
		undoStack.shrink_to_fit();

		if (gears->canvasTextureID != 0) {
			glDeleteTextures(1, &gears->canvasTextureID);
			gears->canvasTextureID = 0;
		}

		gears->isPowerSaving = true;
		printf("Anima: Memory Released. Entering Sleep Mode.\n");
	}

	void Engine::reloadResources() {
		if (!gears->isPowerSaving) return;

		loadFromTempCache("temp_session.bak");

		gears->canvasTextureID = createTextureFromPixels(currentCanvasPixels);

		gears->isPowerSaving = false;
		printf("Anima: Resources Reloaded. Waking up!\n");
	}

	float idleTime = 0.0f; 
	bool highMemoryMode = true;

	void Engine::update(float deltaTime) {
		ImGuiIO& io = ImGui::GetIO();

		bool userActive = (io.MouseDelta.x != 0 || io.MouseDelta.y != 0 || io.InputQueueCharacters.Size > 0 || io.MouseDown[0]);

		if (userActive) {
			idleTime = 0.0f;
			if (!highMemoryMode) {
				highMemoryMode = true;
				reloadResources();
				SDL_GL_SetSwapInterval(1);
			}
		}
		else {
			idleTime += deltaTime;
		}

		if (idleTime > 5.0f && highMemoryMode) {
			highMemoryMode = false;
			freeUnusedMemory();

			SDL_GL_SetSwapInterval(0);
		}

		if (!highMemoryMode) {
			SDL_Delay(100);
		}
	}
}