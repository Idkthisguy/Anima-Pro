// ALL COMMENTS ARE FOR IVAN — LEARNING C++ WITH ME
// Styles.cpp — UI layout + drawing input
// This file calls Canvas and Viewport to do actual work.
// It just handles the "what did the user do with the mouse" part.

#include "Anima.h"
#include "imgui.h"
#include "Canvas.h"
#include "Viewport.h"
#include "InputHandler.h"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;

namespace Anima {

    // The canvas — our drawing surface
    static Canvas    g_Canvas;
    static bool      g_CanvasInited = false;

    // Viewport — handles pan/zoom math
    static Viewport  g_Viewport;
    static bool      g_ViewportFitted = false; // fit once on startup

    // Input handler — keyboard shortcuts
    static InputHandler g_Input;

    // ── Tool state ──
    static int       g_SelectedTool = 0;  // 0=brush 1=eraser 2=bucket 3=eyedropper
    static float     g_BrushSize = 8.0f;
    static float     g_Opacity = 1.0f;
    static float     g_Smoothing = 0.5f;
    static ImVec4    g_DrawColor = { 0.06f, 0.06f, 0.06f, 1.0f };

    // ── Stroke state ──
    // We track whether the user is currently dragging to draw
    // so we can interpolate between mouse positions (no gaps in strokes)
    static bool      g_IsDrawing = false;
    static ImVec2    g_LastDrawPos = { -1, -1 };  // last canvas pos we painted

    // Dirty rect — the region of the canvas modified since last upload
    // We only upload that region to the GPU, not the whole canvas
    static int g_DirtyX0 = 0, g_DirtyY0 = 0;
    static int g_DirtyX1 = 0, g_DirtyY1 = 0;
    static bool g_HasDirty = false;

    // ── Undo stack ──
    // Each entry is a full copy of the canvas pixels
    // this is simple but memory-hungry for large canvases.
    static vector<vector<Pixel>> g_UndoStack;
    static vector<vector<Pixel>> g_RedoStack;
    static const int MAX_UNDO = 30;

    // ── Timeline state ──
    static int       g_TotalFrames = 60;
    static int       g_CurrentFrame = 0;
    static int       g_FPS = 12;
    static bool      g_Playing = false;
    static bool      g_Looping = true;
    static float     g_PlayTimer = 0.0f;  // seconds since last frame advance

    // ── Layer list ──
    static int       g_SelectedLayer = 0;
    static vector<string> g_Layers = { "Layer 1", "Background" };

    // ─────────────────────────────────────────────
    // HELPERS
    // ─────────────────────────────────────────────

    static void PushUndoState() {
        // Save current canvas pixels as an undo checkpoint
        g_UndoStack.push_back(g_Canvas.Pixels());
        if ((int)g_UndoStack.size() > MAX_UNDO)
            g_UndoStack.erase(g_UndoStack.begin()); // drop oldest
        g_RedoStack.clear(); // any new action clears the redo stack
    }

    static void DoUndo() {
        if (g_UndoStack.empty()) return;
        g_RedoStack.push_back(g_Canvas.Pixels()); // save for redo
        g_Canvas.Pixels() = g_UndoStack.back();
        g_UndoStack.pop_back();
        g_Canvas.UploadTexture();
    }

    static void DoRedo() {
        if (g_RedoStack.empty()) return;
        g_UndoStack.push_back(g_Canvas.Pixels());
        g_Canvas.Pixels() = g_RedoStack.back();
        g_RedoStack.pop_back();
        g_Canvas.UploadTexture();
    }

    // Expand the dirty rect to include a circle at (cx,cy) radius r
    static void ExpandDirty(float cx, float cy, float r) {
        int x0 = (int)(cx - r) - 1;
        int y0 = (int)(cy - r) - 1;
        int x1 = (int)(cx + r) + 1;
        int y1 = (int)(cy + r) + 1;

        if (!g_HasDirty) {
            g_DirtyX0 = x0; g_DirtyY0 = y0;
            g_DirtyX1 = x1; g_DirtyY1 = y1;
            g_HasDirty = true;
        }
        else {
            g_DirtyX0 = min(g_DirtyX0, x0);
            g_DirtyY0 = min(g_DirtyY0, y0);
            g_DirtyX1 = max(g_DirtyX1, x1);
            g_DirtyY1 = max(g_DirtyY1, y1);
        }
    }

    // Toolbar button: ghost by default, accent when active
    static bool ToolButton(const char* label, ImVec2 size, bool isActive, ImVec4 accent) {
        ImGui::PushStyleColor(ImGuiCol_Button,
            isActive ? accent : ImVec4(0.13f, 0.13f, 0.14f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            isActive ? accent : ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(accent.x * 0.8f, accent.y * 0.8f, accent.z * 0.8f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        bool clicked = ImGui::Button(label, size);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        return clicked;
    }

    // Section header label
    static void SectionHeader(const char* label) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.50f, 1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.22f, 0.22f, 0.25f, 1.0f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // ─────────────────────────────────────────────
    // DRAWING LOGIC
    // Called when mouse is pressed/dragged over the canvas
    // ─────────────────────────────────────────────

    static void HandleDrawInput(ImVec2 panelPos, ImVec2 panelSize, bool isPanning) {
        ImGuiIO& io = ImGui::GetIO();

        // Only draw with left mouse button, not while panning
        bool leftDown = io.MouseDown[0];

        // Is the mouse inside the viewport panel?
        bool inPanel = (io.MousePos.x >= panelPos.x && io.MousePos.x < panelPos.x + panelSize.x &&
            io.MousePos.y >= panelPos.y && io.MousePos.y < panelPos.y + panelSize.y);

        if (!inPanel || isPanning) {
            // End any active stroke if we left the panel
            if (g_IsDrawing) {
                // Upload dirty region to GPU when stroke ends
                if (g_HasDirty) {
                    // Clamp dirty rect to canvas bounds
                    int cx0 = max(0, g_DirtyX0);
                    int cy0 = max(0, g_DirtyY0);
                    int cx1 = min(g_Canvas.Width() - 1, g_DirtyX1);
                    int cy1 = min(g_Canvas.Height() - 1, g_DirtyY1);
                    if (cx1 > cx0 && cy1 > cy0)
                        g_Canvas.UploadRegion(cx0, cy0, cx1 - cx0, cy1 - cy0);
                    g_HasDirty = false;
                }
                g_IsDrawing = false;
                g_LastDrawPos = { -1, -1 };
            }
            return;
        }

        // Convert mouse from screen space to canvas space
        ImVec2 canvasPos = g_Viewport.ScreenToCanvas(io.MousePos);

        // ── Eyedropper: pick color on click ──
        if (g_SelectedTool == 3 && ImGui::IsMouseClicked(0)) {
            int px = (int)canvasPos.x;
            int py = (int)canvasPos.y;
            Pixel p = g_Canvas.GetPixel(px, py);
            g_DrawColor = { p.r / 255.0f, p.g / 255.0f, p.b / 255.0f, 1.0f };
            return;
        }

        // ── Bucket fill: flood fill on click ──
        if (g_SelectedTool == 2 && ImGui::IsMouseClicked(0)) {
            PushUndoState();
            g_Canvas.FloodFill(
                (int)canvasPos.x, (int)canvasPos.y,
                (uint8_t)(g_DrawColor.x * 255),
                (uint8_t)(g_DrawColor.y * 255),
                (uint8_t)(g_DrawColor.z * 255)
            );
            g_Canvas.UploadTexture();
            return;
        }

        // ── Brush / Eraser: paint while held ──
        bool isBrushTool = (g_SelectedTool == 0 || g_SelectedTool == 1);
        if (!isBrushTool) return;

        if (ImGui::IsMouseClicked(0)) {
            // Start of stroke — save undo state
            PushUndoState();
            g_IsDrawing = true;
            g_LastDrawPos = canvasPos;
        }

        if (g_IsDrawing && leftDown) {
            // Interpolate between last position and current position
            // This fills in the gaps when the mouse moves fast
            // For Ivan: without interpolation, fast strokes look like dotted lines
            float dx = canvasPos.x - g_LastDrawPos.x;
            float dy = canvasPos.y - g_LastDrawPos.y;
            float dist = sqrtf(dx * dx + dy * dy);

            // Step size = half the brush radius (so circles overlap)
            float stepSize = max(1.0f, g_BrushSize * 0.4f);
            int   steps = max(1, (int)(dist / stepSize));

            for (int i = 0; i <= steps; i++) {
                float t = (steps > 0) ? (float)i / steps : 0.0f;
                float px = g_LastDrawPos.x + dx * t;
                float py = g_LastDrawPos.y + dy * t;

                if (g_SelectedTool == 0) {
                    g_Canvas.PaintCircle(px, py, g_BrushSize,
                        g_Opacity,
                        (uint8_t)(g_DrawColor.x * 255),
                        (uint8_t)(g_DrawColor.y * 255),
                        (uint8_t)(g_DrawColor.z * 255));
                }
                else {
                    g_Canvas.EraseCircle(px, py, g_BrushSize, g_Opacity);
                }
                ExpandDirty(px, py, g_BrushSize);
            }
            g_LastDrawPos = canvasPos;

            // Upload dirty region incrementally while drawing
            // so the stroke appears on screen in real time
            if (g_HasDirty) {
                int cx0 = max(0, g_DirtyX0);
                int cy0 = max(0, g_DirtyY0);
                int cx1 = min(g_Canvas.Width() - 1, g_DirtyX1);
                int cy1 = min(g_Canvas.Height() - 1, g_DirtyY1);
                if (cx1 > cx0 && cy1 > cy0)
                    g_Canvas.UploadRegion(cx0, cy0, cx1 - cx0, cy1 - cy0);
                g_HasDirty = false;
            }
        }

        if (!leftDown && g_IsDrawing) {
            g_IsDrawing = false;
            g_LastDrawPos = { -1, -1 };
        }
    }

    // ─────────────────────────────────────────────
    // MAIN INTERFACE FUNCTION
    // ─────────────────────────────────────────────

    void Engine::drawInterface() {

        // ── One-time init ──
        if (!g_CanvasInited) {
            g_Canvas.Init(960, 540);
            g_CanvasInited = true;
        }

        // ── Process keyboard shortcuts ──
        g_Input.Process();

        // ── Handle shortcut actions ──
        if (g_Input.Fired(Action::SelectBrush))      g_SelectedTool = 0;
        if (g_Input.Fired(Action::SelectEraser))     g_SelectedTool = 1;
        if (g_Input.Fired(Action::SelectBucket))     g_SelectedTool = 2;
        if (g_Input.Fired(Action::SelectEyedropper)) g_SelectedTool = 3;
        if (g_Input.Fired(Action::Undo))             DoUndo();
        if (g_Input.Fired(Action::Redo))             DoRedo();
        if (g_Input.Fired(Action::ClearFrame)) { PushUndoState(); g_Canvas.Clear(); g_Canvas.UploadTexture(); }
        if (g_Input.Fired(Action::TogglePlay))       g_Playing = !g_Playing;
        if (g_Input.Fired(Action::NextFrame)) { g_Playing = false; g_CurrentFrame = min(g_CurrentFrame + 1, g_TotalFrames - 1); }
        if (g_Input.Fired(Action::PrevFrame)) { g_Playing = false; g_CurrentFrame = max(g_CurrentFrame - 1, 0); }
        if (g_Input.Fired(Action::ZoomIn))  g_Viewport.ZoomAround(ImGui::GetIO().MousePos, 1.25f);
        if (g_Input.Fired(Action::ZoomOut)) g_Viewport.ZoomAround(ImGui::GetIO().MousePos, 0.8f);

        // ── Playback timer ──
        // io.DeltaTime = seconds since last frame (varies with frame rate)
        // We accumulate it and advance the animation frame when it exceeds 1/fps
        if (g_Playing) {
            g_PlayTimer += ImGui::GetIO().DeltaTime;
            float frameDuration = 1.0f / (float)g_FPS;
            if (g_PlayTimer >= frameDuration) {
                g_PlayTimer -= frameDuration;
                g_CurrentFrame++;
                if (g_CurrentFrame >= g_TotalFrames) {
                    if (g_Looping) g_CurrentFrame = 0;
                    else { g_CurrentFrame = g_TotalFrames - 1; g_Playing = false; }
                }
            }
        }

        // ─────────────────────────────────────────
        // STYLE
        // ─────────────────────────────────────────
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 3.0f;
        style.WindowBorderSize = 0.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(6.0f, 5.0f);
        style.FramePadding = ImVec2(8.0f, 4.0f);
        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.ScrollbarSize = 10.0f;

        ImVec4 accent = ImVec4(0.18f, 0.52f, 0.90f, 1.0f);
        ImVec4 accentDim = ImVec4(0.18f, 0.52f, 0.90f, 0.35f);
        ImVec4 bg0 = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
        ImVec4 bg1 = ImVec4(0.15f, 0.15f, 0.16f, 1.0f);
        ImVec4 bg2 = ImVec4(0.20f, 0.20f, 0.21f, 1.0f);
        ImVec4 bg3 = ImVec4(0.26f, 0.26f, 0.28f, 1.0f);
        ImVec4 textDim = ImVec4(0.50f, 0.50f, 0.54f, 1.0f);
        ImVec4 border = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);

        ImVec4* c = style.Colors;
        c[ImGuiCol_WindowBg] = bg1;
        c[ImGuiCol_ChildBg] = bg0;
        c[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.14f, 0.97f);
        c[ImGuiCol_Border] = border;
        c[ImGuiCol_FrameBg] = bg2;
        c[ImGuiCol_FrameBgHovered] = bg3;
        c[ImGuiCol_FrameBgActive] = bg3;
        c[ImGuiCol_TitleBg] = bg0;
        c[ImGuiCol_TitleBgActive] = bg0;
        c[ImGuiCol_MenuBarBg] = ImVec4(0.13f, 0.13f, 0.14f, 1.0f);
        c[ImGuiCol_ScrollbarBg] = bg0;
        c[ImGuiCol_ScrollbarGrab] = bg3;
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.38f, 1.0f);
        c[ImGuiCol_ScrollbarGrabActive] = accent;
        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = ImVec4(accent.x, accent.y, accent.z, 0.8f);
        c[ImGuiCol_Button] = bg2;
        c[ImGuiCol_ButtonHovered] = bg3;
        c[ImGuiCol_ButtonActive] = accentDim;
        c[ImGuiCol_Header] = accentDim;
        c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.50f);
        c[ImGuiCol_HeaderActive] = accent;
        c[ImGuiCol_Separator] = border;
        c[ImGuiCol_Text] = ImVec4(0.88f, 0.88f, 0.90f, 1.0f);
        c[ImGuiCol_TextDisabled] = textDim;

        // ─────────────────────────────────────────
        // LAYOUT
        // ─────────────────────────────────────────
        ImGuiIO& io = ImGui::GetIO();
        float W = io.DisplaySize.x;
        float H = io.DisplaySize.y;

        float menuH = 19.0f;
        float toolbarW = 44.0f;
        float propW = 210.0f;
        float timelineH = 170.0f;
        float viewW = W - toolbarW - propW;
        float viewH = H - menuH - timelineH;

        ImVec2 viewPanelPos = ImVec2(toolbarW, menuH);
        ImVec2 viewPanelSize = ImVec2(viewW, viewH);

        // Tell the viewport where its panel is
        g_Viewport.SetPanelBounds(viewPanelPos, viewPanelSize);

        // Fit canvas to panel on first run
        if (!g_ViewportFitted && g_CanvasInited) {
            g_Viewport.FitToPanel(g_Canvas.Width(), g_Canvas.Height());
            g_ViewportFitted = true;
        }

        // ─────────────────────────────────────────
        // MENU BAR
        // ─────────────────────────────────────────
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Project", "Ctrl+N")) {}
                if (ImGui::MenuItem("Open...", "Ctrl+O")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Save", "Ctrl+S")) {}
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {}
                ImGui::Separator();
                if (ImGui::BeginMenu("Export")) {
                    if (ImGui::MenuItem("MP4 Video")) {}
                    if (ImGui::MenuItem("WebM Video")) {}
                    if (ImGui::MenuItem("GIF")) {}
                    if (ImGui::MenuItem("Sprite Sheet")) {}
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) { m_KeepRunning = false; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z"))   DoUndo();
                if (ImGui::MenuItem("Redo", "Ctrl+Y"))   DoRedo();
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Frame", "Del")) { PushUndoState(); g_Canvas.Clear(); g_Canvas.UploadTexture(); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Zoom In", "Ctrl++")) g_Viewport.ZoomAround(io.MousePos, 1.25f);
                if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) g_Viewport.ZoomAround(io.MousePos, 0.8f);
                if (ImGui::MenuItem("Fit to Window", "Ctrl+0")) g_Viewport.FitToPanel(g_Canvas.Width(), g_Canvas.Height());
                ImGui::EndMenu();
            }
            // Right-aligned version string
            float vw = ImGui::CalcTextSize("ANIMA  v1.0").x + 20.0f;
            ImGui::SetCursorPosX(W - vw);
            ImGui::PushStyleColor(ImGuiCol_Text, textDim);
            ImGui::Text("ANIMA  v1.0");
            ImGui::PopStyleColor();
            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar();

        // ─────────────────────────────────────────
        // TOOLBAR
        // ─────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(0, menuH));
        ImGui::SetNextWindowSize(ImVec2(toolbarW, viewH));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 8));
        ImGui::Begin("##toolbar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        struct ToolDef { const char* label; const char* tip; int id; };
        ToolDef toolDefs[] = {
            {"Br", "Brush (B)",       0},
            {"Er", "Eraser (E)",      1},
            {"Bk", "Bucket (G)",      2},
            {"Ey", "Eyedropper (I)",  3},
        };
        for (auto& t : toolDefs) {
            bool active = (g_SelectedTool == t.id);
            if (active) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(p.x - 4, p.y), ImVec2(p.x - 1, p.y + 34),
                    IM_COL32(46, 133, 230, 255));
            }
            if (ToolButton(t.label, ImVec2(36, 34), active, accent))
                g_SelectedTool = t.id;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.tip);
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, border); ImGui::Separator(); ImGui::PopStyleColor();
        ImGui::Spacing();

        if (ToolButton("Un", ImVec2(36, 28), false, accent)) DoUndo();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo (Ctrl+Z)");
        if (ToolButton("Re", ImVec2(36, 28), false, accent)) DoRedo();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Redo (Ctrl+Y)");

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, border); ImGui::Separator(); ImGui::PopStyleColor();
        ImGui::Spacing();

        // Color swatch
        ImVec2 swatchPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##swatch", ImVec2(36, 36));
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##swatchpop");
        ImGui::GetWindowDrawList()->AddRectFilled(
            swatchPos, ImVec2(swatchPos.x + 36, swatchPos.y + 36),
            IM_COL32((int)(g_DrawColor.x * 255), (int)(g_DrawColor.y * 255), (int)(g_DrawColor.z * 255), 255), 4.0f);
        ImGui::GetWindowDrawList()->AddRect(
            swatchPos, ImVec2(swatchPos.x + 36, swatchPos.y + 36),
            IM_COL32(80, 80, 86, 255), 4.0f);
        if (ImGui::BeginPopup("##swatchpop")) {
            ImGui::ColorPicker4("##cp", (float*)&g_DrawColor,
                ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
            ImGui::EndPopup();
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // ─────────────────────────────────────────
        // VIEWPORT — this is where drawing happens
        // ─────────────────────────────────────────
        ImGui::SetNextWindowPos(viewPanelPos);
        ImGui::SetNextWindowSize(viewPanelSize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##viewport", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        ImDrawList* vdraw = ImGui::GetWindowDrawList();

        // Is the user panning right now? (space+drag or middle mouse)
        bool spaceHeld = ImGui::IsKeyDown(ImGuiKey_Space);
        bool middleMouse = io.MouseDown[2];
        bool isPanning = spaceHeld || middleMouse;

        // Handle pan/zoom (Viewport reads mouse from ImGuiIO directly)
        // We only allow it when the mouse is actually in the viewport
        bool mouseInView = (io.MousePos.x >= viewPanelPos.x &&
            io.MousePos.x < viewPanelPos.x + viewW &&
            io.MousePos.y >= viewPanelPos.y &&
            io.MousePos.y < viewPanelPos.y + viewH);
        g_Viewport.HandleInput(mouseInView);

        // Handle brush/eraser drawing
        HandleDrawInput(viewPanelPos, viewPanelSize, isPanning);

        // Draw checkerboard background
        float tileSize = 14.0f;
        int tilesX = (int)(viewW / tileSize) + 2;
        int tilesY = (int)(viewH / tileSize) + 2;
        for (int ty = 0; ty < tilesY; ty++) {
            for (int tx = 0; tx < tilesX; tx++) {
                bool light = (tx + ty) % 2 == 0;
                ImU32 tc = light ? IM_COL32(22, 22, 24, 255) : IM_COL32(18, 18, 20, 255);
                vdraw->AddRectFilled(
                    ImVec2(viewPanelPos.x + tx * tileSize, viewPanelPos.y + ty * tileSize),
                    ImVec2(viewPanelPos.x + (tx + 1) * tileSize, viewPanelPos.y + (ty + 1) * tileSize),
                    tc);
            }
        }

        // Draw the canvas texture
        // CanvasToScreen converts the canvas corners to screen positions
        // accounting for current pan and zoom
        ImVec2 canvasTopLeft = g_Viewport.CanvasToScreen({ 0, 0 });
        ImVec2 canvasTopLeft2 = g_Viewport.CanvasToScreen({ (float)g_Canvas.Width(), (float)g_Canvas.Height() });

        // Drop shadow
        vdraw->AddRectFilled(
            ImVec2(canvasTopLeft.x + 6, canvasTopLeft.y + 6),
            ImVec2(canvasTopLeft2.x + 6, canvasTopLeft2.y + 6),
            IM_COL32(0, 0, 0, 100), 2.0f);

        // Canvas image — ImTextureID is just the GL texture ID cast to a pointer
        // Ivan: ImGui::Image takes top-left and bottom-right UV corners.
        // UV (0,0) = top-left of texture, UV (1,1) = bottom-right.
        vdraw->AddImage(
            (ImTextureID)(uintptr_t)g_Canvas.TextureID(),
            canvasTopLeft, canvasTopLeft2,
            ImVec2(0, 0), ImVec2(1, 1));

        // Canvas border
        vdraw->AddRect(
            ImVec2(canvasTopLeft.x - 1, canvasTopLeft.y - 1),
            ImVec2(canvasTopLeft2.x + 1, canvasTopLeft2.y + 1),
            IM_COL32(50, 50, 56, 255));

        // Cursor preview — show brush size as a circle
        if (mouseInView && !isPanning && (g_SelectedTool == 0 || g_SelectedTool == 1)) {
            float screenRadius = g_BrushSize * g_Viewport.Zoom();
            vdraw->AddCircle(io.MousePos, screenRadius,
                IM_COL32(200, 200, 200, 180), 32, 1.0f);
            // Small crosshair dot at center
            vdraw->AddCircleFilled(io.MousePos, 2.0f, IM_COL32(255, 255, 255, 200));
        }

        // Zoom indicator
        char zoomLabel[16];
        snprintf(zoomLabel, sizeof(zoomLabel), "%.0f%%", g_Viewport.Zoom() * 100.0f);
        ImVec2 zp = ImVec2(viewPanelPos.x + viewW - 50, viewPanelPos.y + viewH - 22);
        vdraw->AddRectFilled(zp, ImVec2(zp.x + 42, zp.y + 16), IM_COL32(0, 0, 0, 100), 3.0f);
        vdraw->AddText(ImVec2(zp.x + 4, zp.y + 2), IM_COL32(120, 120, 130, 255), zoomLabel);

        // Frame / tool indicator
        char infoLabel[48];
        const char* toolNames[] = { "Brush","Eraser","Bucket","Eyedrop" };
        snprintf(infoLabel, sizeof(infoLabel), "Fr %d/%d  |  %s  %.0fpx",
            g_CurrentFrame + 1, g_TotalFrames, toolNames[g_SelectedTool], g_BrushSize);
        float iw = ImGui::CalcTextSize(infoLabel).x + 10;
        ImVec2 ip = ImVec2(viewPanelPos.x + 8, viewPanelPos.y + 8);
        vdraw->AddRectFilled(ip, ImVec2(ip.x + iw, ip.y + 18), IM_COL32(0, 0, 0, 120), 3.0f);
        vdraw->AddText(ImVec2(ip.x + 5, ip.y + 3), IM_COL32(180, 180, 190, 200), infoLabel);

        // Cursor: hide system cursor when over canvas
        if (mouseInView) ImGui::SetMouseCursor(ImGuiMouseCursor_None);

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // ─────────────────────────────────────────
        // PROPERTIES PANEL
        // ─────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(toolbarW + viewW, menuH));
        ImGui::SetNextWindowSize(ImVec2(propW, viewH));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg1);
        ImGui::Begin("##props", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        SectionHeader("BRUSH");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bg2);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##size", &g_BrushSize, 1.0f, 100.0f, "Size: %.0fpx");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##opacity", &g_Opacity, 0.0f, 1.0f, "Opacity: %.0f%%");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##smooth", &g_Smoothing, 0.0f, 1.0f, "Smooth: %.0f%%");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(g_DrawColor.x, g_DrawColor.y, g_DrawColor.z, 1.0f));
        ImGui::BeginChild("##colorbar", ImVec2(-1, 22), false);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##propcolor");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to change color");
        if (ImGui::BeginPopup("##propcolor")) {
            ImGui::ColorPicker4("##pp", (float*)&g_DrawColor,
                ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
            ImGui::EndPopup();
        }

        SectionHeader("LAYERS");
        ImGui::PushStyleColor(ImGuiCol_Button, accentDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
        if (ImGui::Button("+ Add Layer", ImVec2(-1, 22))) {
            g_Layers.insert(g_Layers.begin(), "Layer " + to_string(g_Layers.size() + 1));
            g_SelectedLayer = 0;
        }
        ImGui::PopStyleColor(2);
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg0);
        ImGui::BeginChild("##layerlist", ImVec2(-1, 120), true);
        for (int i = 0; i < (int)g_Layers.size(); i++) {
            bool sel = (g_SelectedLayer == i);
            ImGui::GetWindowDrawList()->AddCircleFilled(
                ImVec2(ImGui::GetCursorScreenPos().x + 6, ImGui::GetCursorScreenPos().y + 9),
                4.0f, IM_COL32(60, 200, 100, 200));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 18);
            ImGui::PushStyleColor(ImGuiCol_Header, accentDim);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accent.x, accent.y, accent.z, 0.3f));
            if (ImGui::Selectable(g_Layers[i].c_str(), sel)) g_SelectedLayer = i;
            ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        SectionHeader("CANVAS");
        ImGui::PushStyleColor(ImGuiCol_Text, textDim);
        ImGui::Text("%d x %d  |  %.0f%%", g_Canvas.Width(), g_Canvas.Height(), g_Viewport.Zoom() * 100.0f);
        ImGui::Text("FPS: %d  |  Frames: %d", g_FPS, g_TotalFrames);
        ImGui::PopStyleColor();

        // Keyboard shortcut reference
        SectionHeader("SHORTCUTS");
        ImGui::PushStyleColor(ImGuiCol_Text, textDim);
        ImGui::TextWrapped("B Brush  E Eraser  G Bucket  I Eyedrop\n"
            "Space Play  ←/→ Frame\n"
            "Scroll Zoom  Mid/Space+Drag Pan\n"
            "Ctrl+Z Undo  Ctrl+Y Redo");
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleColor();

        // ─────────────────────────────────────────
        // TIMELINE
        // ─────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(0, menuH + viewH));
        ImGui::SetNextWindowSize(ImVec2(W, timelineH));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
        ImGui::Begin("##timeline", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        // Accent top border
        ImVec2 tlPos = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(tlPos.x, tlPos.y),
            ImVec2(tlPos.x + W, tlPos.y),
            IM_COL32(46, 133, 230, 180), 1.5f);

        ImGui::Spacing();

        // Play button
        ImGui::PushStyleColor(ImGuiCol_Button, g_Playing ? accent : bg2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
        if (ImGui::Button(g_Playing ? " || " : "  >  ", ImVec2(42, 24)))
            g_Playing = !g_Playing;
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        // Seeker
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(accent.x, accent.y, accent.z, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bg2);
        ImGui::SetNextItemWidth(W - 310.0f);
        float seekF = (float)g_CurrentFrame;
        if (ImGui::SliderFloat("##seek", &seekF, 0.0f, (float)(g_TotalFrames - 1), ""))
            g_CurrentFrame = (int)seekF;
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, textDim);
        ImGui::Text("%03d / %03d", g_CurrentFrame + 1, g_TotalFrames);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 20);
        ImGui::PushStyleColor(ImGuiCol_Text, textDim);
        ImGui::Text("FPS");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bg2);
        ImGui::SetNextItemWidth(36);
        ImGui::InputInt("##fps", &g_FPS, 0, 0);
        ImGui::PopStyleColor();
        g_FPS = max(1, min(g_FPS, 60));
        ImGui::SameLine(0, 16);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bg2);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, accent);
        ImGui::Checkbox("Loop", &g_Looping);
        ImGui::PopStyleColor(2);

        ImGui::Spacing();

        // Frame strip
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
        ImGui::BeginChild("##frames", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (int i = 0; i < g_TotalFrames; i++) {
            if (i > 0) ImGui::SameLine(0, 2);
            bool isCur = (g_CurrentFrame == i);
            bool hasData = (i == 0); // placeholder — replace with real frame check
            ImVec4 bc = isCur ? accent
                : hasData ? ImVec4(0.28f, 0.28f, 0.30f, 1.0f)
                : ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, bc);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isCur ? accent : bg3);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
            char lbl[8]; snprintf(lbl, sizeof(lbl), "##f%d", i);
            if (ImGui::Button(lbl, ImVec2(22, 52))) { g_CurrentFrame = i; g_Playing = false; }
            ImVec2 bpos = ImGui::GetItemRectMin();
            if (i % 5 == 0) {
                char num[6]; snprintf(num, sizeof(num), "%d", i);
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(bpos.x + 3, bpos.y + 36),
                    isCur ? IM_COL32(255, 255, 255, 220) : IM_COL32(90, 90, 100, 255), num);
            }
            if (hasData && !isCur)
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    ImVec2(bpos.x + 11, bpos.y + 16), 3.0f, IM_COL32(100, 160, 255, 200));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}