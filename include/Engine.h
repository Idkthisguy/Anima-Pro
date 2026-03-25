#pragma once
#include <string>

// These are "Forward Declarations"
// It's like saying "I'll have a steering wheel later, just know it exists for now."
struct SDL_Window;
typedef void* SDL_GLContext;

namespace Anima {
    class Engine {
    public:
        Engine();
        ~Engine();

        // Starts the engine (Startup)
        bool Startup(const std::string& name, int w, int h);

        // Checks if the car is still driving
        bool IsRunning() const { return m_KeepRunning; }

        void syncInput();      // Grabs the steering wheel and pedal input
        void BeginFrame();     // Starts a new drawing frame
        void drawInterface();  // Draws the ImGui buttons
        void EndFrame();       // Swaps the drawing to the screen
        void shutdown();       // Parks the car and turns off the lights

        // --- Your New 2026 Power-Saving Features ---
        void update(float deltaTime);   // The heartbeat of the app
        void freeUnusedMemory();        // The "Sleep Mode" to save RAM/Disk
        void reloadResources();         // Waking back up when the user moves the mouse

    private:
        bool m_KeepRunning; // Is the engine on?

        // "internalGears" holds the heavy machinery hidden from the user
        struct internalGears;
        internalGears* m_Gears;
    };
}