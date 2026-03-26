#pragma once

#include "imgui.h"
#include "Anima.h"
#include <cstdint> 

namespace Anima {
    enum class Action {
        None,
        SelectBrush,
        SelectEraser,
        SelectBucket,
        SelectEyedropper,
        Undo,
        Redo,
        CopyFrame,
        PasteFrame,
        ClearFrame,
        TogglePlay,
        NextFrame,
        PrevFrame,
        ZoomIn,
        ZoomOut,
        FitCanvas,
        NewProject,
        OpenProject,
        Save,
        SaveAs,
		KillProcess,
    };

    class InputHandler {
    public:
        InputHandler();

        void Process();
        bool Fired(Action a) const;

        bool Ctrl()  const { return m_Ctrl; }
        bool Shift() const { return m_Shift; }
        bool Alt()   const { return m_Alt; }
        bool Esc()   const { return m_Esc; }
        bool MouseOverCanvas() const { return m_MouseOverCanvas; }
        void SetMouseOverCanvas(bool v) { m_MouseOverCanvas = v; }

    private:
        uint64_t m_FiredActions = 0;

        bool m_Ctrl = false;
        bool m_Shift = false;
        bool m_Alt = false;
		bool m_Esc = false;
        bool m_MouseOverCanvas = false;

        void Fire(Action a);
        bool JustPressed(ImGuiKey key) const;
    };

}