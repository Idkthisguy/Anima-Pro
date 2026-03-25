// InputHandler.cpp — keyboard shortcut processing
// Ivan: we use ImGuiKey_ values instead of SDL scancodes
// because ImGui handles keyboard layout differences automatically

#include "InputHandler.h"
#include <cstring>

namespace Anima {

    InputHandler::InputHandler() {}

    bool InputHandler::JustPressed(ImGuiKey key) const {
        return ImGui::IsKeyPressed(key, false); // false = no repeat
    }

    void InputHandler::Fire(Action a) {
        m_FiredActions |= (1ULL << (int)a);
    }

    bool InputHandler::Fired(Action a) const {
        return (m_FiredActions >> (int)a) & 1;
    }

    void InputHandler::Process() {
        // Clear all actions from last frame
        m_FiredActions = 0;

        // Read modifier state
        m_Ctrl = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
        m_Shift = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
        m_Alt = ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);
		m_Esc = ImGui::IsKeyDown(ImGuiKey_Escape);

        // Don't fire shortcuts when typing in an input field
        // ImGui::GetIO().WantCaptureKeyboard is true when text input is focused
        if (ImGui::GetIO().WantCaptureKeyboard) return;

        // ── Tool shortcuts ──
        if (JustPressed(ImGuiKey_B)) Fire(Action::SelectBrush);
        if (JustPressed(ImGuiKey_E)) Fire(Action::SelectEraser);
        if (JustPressed(ImGuiKey_G)) Fire(Action::SelectBucket);
        if (JustPressed(ImGuiKey_I)) Fire(Action::SelectEyedropper);

        // ── Edit shortcuts ──
        if (m_Ctrl && JustPressed(ImGuiKey_Z) && !m_Shift) Fire(Action::Undo);
        if (m_Ctrl && JustPressed(ImGuiKey_Y))              Fire(Action::Redo);
        if (m_Ctrl && JustPressed(ImGuiKey_Z) && m_Shift) Fire(Action::Redo); // Ctrl+Shift+Z also = redo
        if (m_Ctrl && JustPressed(ImGuiKey_C))              Fire(Action::CopyFrame);
        if (m_Ctrl && JustPressed(ImGuiKey_V))              Fire(Action::PasteFrame);
        if (JustPressed(ImGuiKey_Delete))                   Fire(Action::ClearFrame);

        // ── Playback shortcuts ──
        if (JustPressed(ImGuiKey_Space))      Fire(Action::TogglePlay);
        if (JustPressed(ImGuiKey_RightArrow)) Fire(Action::NextFrame);
        if (JustPressed(ImGuiKey_LeftArrow))  Fire(Action::PrevFrame);
        if (JustPressed(ImGuiKey_Period) && m_Ctrl) Fire(Action::NextFrame);  // Ctrl+. = next
        if (JustPressed(ImGuiKey_Comma) && m_Ctrl) Fire(Action::PrevFrame);  // Ctrl+, = prev

        // ── View shortcuts ──
        if (m_Ctrl && JustPressed(ImGuiKey_Equal))    Fire(Action::ZoomIn);   // Ctrl++
        if (m_Ctrl && JustPressed(ImGuiKey_Minus))    Fire(Action::ZoomOut);  // Ctrl+-
        if (m_Ctrl && JustPressed(ImGuiKey_0))        Fire(Action::FitCanvas);

        // ── File shortcuts ──
        if (m_Ctrl && JustPressed(ImGuiKey_N))              Fire(Action::NewProject);
        if (m_Ctrl && JustPressed(ImGuiKey_O))              Fire(Action::OpenProject);
        if (m_Ctrl && JustPressed(ImGuiKey_S) && !m_Shift) Fire(Action::Save);
        if (m_Ctrl && JustPressed(ImGuiKey_S) && m_Shift) Fire(Action::SaveAs);

        if (m_Esc && JustPressed(ImGuiKey_K)) Fire(Action::KillProcess);
    }

} // namespace Anima