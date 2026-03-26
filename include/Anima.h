#pragma once
#include <string>

struct SDL_Window;
typedef void* SDL_GLContext;

namespace Anima {
	class Engine {
	public:
		Engine();

		~Engine();


		bool Startup(const std::string& name, int w, int h);
		bool IsRunning() const { return m_KeepRunning; }

		void syncInput();
		void BeginFrame();
		void drawInterface();
		void EndFrame();
		void shutdown();

	private:
		bool m_KeepRunning;

		struct internalGears;
		internalGears* m_Gears;
	};
}