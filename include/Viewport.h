#pragma once
// Viewport.h — handles pan, zoom, and coordinate conversion
// Ivan: the "viewport" is the window into the canvas.
// Pan = moving the camera. Zoom = scaling it.
// All drawing happens in "canvas space" (pixel coordinates on the canvas).
// The mouse position comes in "screen space" (pixels on your monitor).
// We need to convert between them — that's what this class does.

#include "imgui.h"

namespace Anima {

    class Viewport {
    public:
        Viewport();

        // Call every frame with the panel position/size so the viewport
        // knows where on screen the canvas is being displayed
        void SetPanelBounds(ImVec2 panelPos, ImVec2 panelSize);

        // Handle pan (middle-mouse drag or Space+drag) and zoom (scroll wheel)
        // Pass the ImGuiIO so we can read mouse state
        // Ivan: we call this inside drawInterface, not syncInput,
        // because ImGui needs to have processed the frame first
        void HandleInput(bool allowInput);

        // Convert screen-space mouse position → canvas-space pixel position
        // Example: mouse at screen (600, 400) with zoom=2, pan=(100,50)
        //          → canvas pixel (250, 175)
        ImVec2 ScreenToCanvas(ImVec2 screenPos) const;

        // Convert canvas-space pixel → screen position
        ImVec2 CanvasToScreen(ImVec2 canvasPos) const;

        // The transform values — read by Styles.cpp to position the canvas rect
        float  Zoom()    const { return m_Zoom; }
        ImVec2 Pan()     const { return m_Pan; }

        // Reset zoom and center the canvas in the panel
        void FitToPanel(int canvasW, int canvasH);

        // Zoom toward a specific screen point (e.g. the mouse cursor)
        void ZoomAround(ImVec2 screenPoint, float factor);

    private:
        float  m_Zoom = 1.0f;
        ImVec2 m_Pan = { 0, 0 };      // pan offset in screen pixels

        ImVec2 m_PanelPos = { 0, 0 };
        ImVec2 m_PanelSize = { 0, 0 };

        bool   m_Panning = false;
        ImVec2 m_PanStart = { 0, 0 };     // mouse pos when pan started
        ImVec2 m_PanOrigin = { 0, 0 };    // pan value when pan started
    };

} // namespace Anima