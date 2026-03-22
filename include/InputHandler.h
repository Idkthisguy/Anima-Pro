#pragma once
// InputHandler.h — keyboard shortcuts and input state
// Ivan: all keybind logic lives here so it's not scattered across files.
// The engine calls InputHandler::Process() once per frame.
// Other systems (Canvas, Viewport) read the resulting actions.

#include "imgui.h"
#include <cstdint> 

namespace Anima {

    // Every action the user can trigger — tools, timeline, etc.
    // Using an enum makes it easy to check "did this action fire this frame"
    enum class Action {
        None,
        // Tools
        SelectBrush,
        SelectEraser,
        SelectBucket,
        SelectEyedropper,
        // Edit
        Undo,
        Redo,
        CopyFrame,
        PasteFrame,
        ClearFrame,
        // Playback
        TogglePlay,
        NextFrame,
        PrevFrame,
        // View
        ZoomIn,
        ZoomOut,
        FitCanvas,
        // File
        NewProject,
        OpenProject,
        Save,
        SaveAs,
    };

    class InputHandler {
    public:
        InputHandler();

        // Call once per frame — reads ImGui keyboard state and fires actions
        void Process();

        // Check if an action was triggered this frame
        // Ivan: use this instead of checking keys directly in other files
        // e.g.  if (input.Fired(Action::Undo)) { ... }
        bool Fired(Action a) const;

        // Is a modifier key currently held?
        bool Ctrl()  const { return m_Ctrl; }
        bool Shift() const { return m_Shift; }
        bool Alt()   const { return m_Alt; }

        // Is the mouse cursor over the canvas viewport panel?
        // Drawing should only happen when this is true
        bool MouseOverCanvas() const { return m_MouseOverCanvas; }
        void SetMouseOverCanvas(bool v) { m_MouseOverCanvas = v; }

    private:
        // Bitmask of fired actions this frame
        // 64 bits covers all our Action enum values
        uint64_t m_FiredActions = 0;

        bool m_Ctrl = false;
        bool m_Shift = false;
        bool m_Alt = false;
        bool m_MouseOverCanvas = false;

        void Fire(Action a);

        // True if key was just pressed this frame (not held)
        // Ivan: ImGui tracks this as "key pressed", not "key down"
        // "Down" = held. "Pressed" = just this frame. We want Pressed for shortcuts.
        bool JustPressed(ImGuiKey key) const;
    };

} // namespace Anima