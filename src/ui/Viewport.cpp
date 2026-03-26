// Viewport.cpp — pan and zoom logic
// Ivan: this is pure math. No OpenGL, no drawing.
// Just tracking where the camera is and converting coordinates.

#include "Viewport.h"
#include <algorithm>

namespace Anima {

    Viewport::Viewport() {}

    void Viewport::SetPanelBounds(ImVec2 panelPos, ImVec2 panelSize) {
        m_PanelPos = panelPos;
        m_PanelSize = panelSize;
    }

    void Viewport::FitToPanel(int canvasW, int canvasH) {
        // Scale the canvas to fit inside the panel with a small margin
        float margin = 40.0f;
        float scaleX = (m_PanelSize.x - margin) / canvasW;
        float scaleY = (m_PanelSize.y - margin) / canvasH;
        m_Zoom = std::min(scaleX, scaleY);     // use the tighter fit
        m_Zoom = std::max(0.1f, std::min(m_Zoom, 8.0f)); // clamp

        // Center the canvas in the panel
        // Pan is the offset from the panel's top-left to the canvas's top-left
        m_Pan.x = (m_PanelSize.x - canvasW * m_Zoom) * 0.5f;
        m_Pan.y = (m_PanelSize.y - canvasH * m_Zoom) * 0.5f;
    }

    void Viewport::ZoomAround(ImVec2 screenPoint, float factor) {
        // We want the canvas pixel under the cursor to stay in the same
        // screen position after zooming. The math:
        // 1. Find the canvas pixel under cursor BEFORE zoom
        // 2. Apply new zoom
        // 3. Adjust pan so that same canvas pixel lands at same screen pos

        // Canvas pos under cursor before zoom
        ImVec2 canvasPt = ScreenToCanvas(screenPoint);

        float newZoom = std::max(0.1f, std::min(m_Zoom * factor, 32.0f));
        m_Zoom = newZoom;

        // What screen position would that canvas pixel land at now?
        // screenPos = panelPos + pan + canvasPos * zoom
        // We want screenPos == screenPoint, so:
        // pan = screenPoint - panelPos - canvasPos * zoom
        m_Pan.x = (screenPoint.x - m_PanelPos.x) - canvasPt.x * m_Zoom;
        m_Pan.y = (screenPoint.y - m_PanelPos.y) - canvasPt.y * m_Zoom;
    }

    ImVec2 Viewport::ScreenToCanvas(ImVec2 screenPos) const {
        // Undo the pan and zoom transforms
        // screenPos = panelPos + pan + canvasPos * zoom
        // → canvasPos = (screenPos - panelPos - pan) / zoom
        return {
            (screenPos.x - m_PanelPos.x - m_Pan.x) / m_Zoom,
            (screenPos.y - m_PanelPos.y - m_Pan.y) / m_Zoom
        };
    }

    ImVec2 Viewport::CanvasToScreen(ImVec2 canvasPos) const {
        return {
            m_PanelPos.x + m_Pan.x + canvasPos.x * m_Zoom,
            m_PanelPos.y + m_Pan.y + canvasPos.y * m_Zoom
        };
    }

    void Viewport::HandleInput(bool allowInput) {
        if (!allowInput) return;

        ImGuiIO& io = ImGui::GetIO();

        // ── ZOOM with scroll wheel ──
        // io.MouseWheel is positive when scrolling up (zoom in)
        if (io.MouseWheel != 0.0f) {
            float factor = (io.MouseWheel > 0) ? 1.12f : (1.0f / 1.12f);
            ZoomAround(io.MousePos, factor);
        }

        // ── PAN with middle mouse button OR Space + left mouse ──
        bool spaceHeld = ImGui::IsKeyDown(ImGuiKey_Space);
        bool middleMouse = io.MouseDown[2]; // button 2 = middle mouse
        bool panTrigger = middleMouse || (spaceHeld && io.MouseDown[0]);

        if (panTrigger && !m_Panning) {
            // Started panning — record the starting positions
            m_Panning = true;
            m_PanStart = io.MousePos;
            m_PanOrigin = m_Pan;
        }

        if (m_Panning) {
            if (panTrigger) {
                // Delta = how far mouse moved from where pan started
                m_Pan.x = m_PanOrigin.x + (io.MousePos.x - m_PanStart.x);
                m_Pan.y = m_PanOrigin.y + (io.MousePos.y - m_PanStart.y);
            }
            else {
                // Mouse released — stop panning
                m_Panning = false;
            }
        }
    }

} // namespace Anima