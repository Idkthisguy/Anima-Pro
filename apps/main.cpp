// HANDLED BY IDKTHISGUY

#include "Anima.h"
#include <SDL.h> 
#include <Windows.h>


int main(int argc, char* argv[]) {
	Anima::Engine core;

	SetProcessDPIAware();

	if (!core.Startup("Anima Pro v1.0", 1280, 720)) {
		return -1;
	}
	
	while (core.IsRunning()) {
		core.syncInput();
		core.BeginFrame();
		core.drawInterface();
		core.EndFrame();
	}

	return 0;
}