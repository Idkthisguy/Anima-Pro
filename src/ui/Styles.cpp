#include "Anima.h"
#include "imgui.h"
#include "Canvas.h"
#include "Viewport.h"
#include "InputHandler.h"
#include "Timeline.h"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;

namespace Anima {

    static Timeline     g_TL;
    static Viewport     g_VP;
    static InputHandler g_Input;
    static bool         g_VPFitted = false;

    static int   g_Tool = 0;
    static float g_BrushSize = 8.0f;
    static float g_Opacity = 1.0f;
    static ImVec4 g_Color = { 0.05f, 0.05f, 0.05f, 1.0f };

    static bool   g_Drawing = false;
    static ImVec2 g_LastDrawPos = { -1, -1 };

    static int  g_DX0 = 0, g_DY0 = 0, g_DX1 = 0, g_DY1 = 0;
    static bool g_Dirty = false;

    static void expandDirty(float cx, float cy, float r) {
        int x0 = (int)(cx - r) - 1, y0 = (int)(cy - r) - 1;
        int x1 = (int)(cx + r) + 1, y1 = (int)(cy + r) + 1;
        if (!g_Dirty) {
            g_DX0 = x0; g_DY0 = y0; g_DX1 = x1; g_DY1 = y1;
            g_Dirty = true;
        }
        else {
            g_DX0 = min(g_DX0, x0); g_DY0 = min(g_DY0, y0);
            g_DX1 = max(g_DX1, x1); g_DY1 = max(g_DY1, y1);
        }
    }

    static void flushDirty(Canvas* c) {
        if (!g_Dirty || !c) return;
        int x0 = max(0, g_DX0), y0 = max(0, g_DY0);
        int x1 = min(c->Width() - 1, g_DX1), y1 = min(c->Height() - 1, g_DY1);
        if (x1 > x0 && y1 > y0) c->UploadRegion(x0, y0, x1 - x0, y1 - y0);
        g_Dirty = false;
    }

    static bool toolBtn(const char* id, ImVec2 sz, bool active, ImVec4 acc) {
        ImGui::PushStyleColor(ImGuiCol_Button, active ? acc : ImVec4(0.15f, 0.15f, 0.17f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? acc : ImVec4(0.22f, 0.22f, 0.25f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(acc.x * .8f, acc.y * .8f, acc.z * .8f, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        bool hit = ImGui::Button(id, sz);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        return hit;
    }

    static void sectionLabel(const char* t) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.42f, .42f, .46f, 1));
        ImGui::TextUnformatted(t);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(.20f, .20f, .23f, 1));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    static void applyStyle() {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 0;
        s.ChildRounding = 3;
        s.FrameRounding = 3;
        s.GrabRounding = 2;
        s.PopupRounding = 4;
        s.TabRounding = 3;
        s.WindowBorderSize = 0;
        s.ChildBorderSize = 1;
        s.FrameBorderSize = 0;
        s.ItemSpacing = { 4, 4 };
        s.FramePadding = { 7, 3 };
        s.WindowPadding = { 8, 8 };
        s.ScrollbarSize = 9;
        s.IndentSpacing = 12;

        auto* c = s.Colors;
        auto  BG = [](float v) { return ImVec4(v, v, v + .01f, 1); };

        c[ImGuiCol_WindowBg] = BG(.12f);
        c[ImGuiCol_ChildBg] = BG(.08f);
        c[ImGuiCol_PopupBg] = ImVec4(.13f, .13f, .14f, .97f);
        c[ImGuiCol_Border] = ImVec4(.19f, .19f, .21f, 1);
        c[ImGuiCol_FrameBg] = BG(.18f);
        c[ImGuiCol_FrameBgHovered] = BG(.23f);
        c[ImGuiCol_FrameBgActive] = BG(.25f);
        c[ImGuiCol_TitleBg] = BG(.08f);
        c[ImGuiCol_TitleBgActive] = BG(.10f);
        c[ImGuiCol_MenuBarBg] = BG(.10f);
        c[ImGuiCol_ScrollbarBg] = BG(.07f);
        c[ImGuiCol_ScrollbarGrab] = BG(.28f);
        c[ImGuiCol_ScrollbarGrabHovered] = BG(.34f);
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(.28f, .50f, .92f, 1);
        c[ImGuiCol_SliderGrab] = ImVec4(.28f, .50f, .92f, 1);
        c[ImGuiCol_SliderGrabActive] = ImVec4(.36f, .58f, 1, 1);
        c[ImGuiCol_CheckMark] = ImVec4(.28f, .50f, .92f, 1);
        c[ImGuiCol_Button] = BG(.19f);
        c[ImGuiCol_ButtonHovered] = BG(.26f);
        c[ImGuiCol_ButtonActive] = ImVec4(.22f, .42f, .80f, .5f);
        c[ImGuiCol_Header] = BG(.22f);
        c[ImGuiCol_HeaderHovered] = ImVec4(.28f, .50f, .92f, .45f);
        c[ImGuiCol_HeaderActive] = ImVec4(.28f, .50f, .92f, 1);
        c[ImGuiCol_Separator] = ImVec4(.19f, .19f, .21f, 1);
        c[ImGuiCol_Tab] = BG(.13f);
        c[ImGuiCol_TabHovered] = BG(.26f);
        c[ImGuiCol_TabActive] = BG(.20f);
        c[ImGuiCol_Text] = ImVec4(.88f, .88f, .91f, 1);
        c[ImGuiCol_TextDisabled] = ImVec4(.42f, .42f, .46f, 1);
    }

    static void handleDraw(ImVec2 ppos, ImVec2 psz, bool panning) {
        auto* cur = g_TL.current();
        if (!cur) return;

        ImGuiIO& io = ImGui::GetIO();
        bool ldown = io.MouseDown[0];
        bool inPanel = io.MousePos.x >= ppos.x && io.MousePos.x < ppos.x + psz.x
            && io.MousePos.y >= ppos.y && io.MousePos.y < ppos.y + psz.y;

        if (!inPanel || panning) {
            if (g_Drawing) { flushDirty(cur); g_Drawing = false; g_LastDrawPos = { -1,-1 }; }
            return;
        }

        ImVec2 cp = g_VP.ScreenToCanvas(io.MousePos);

        if (g_Tool == 3 && ImGui::IsMouseClicked(0)) {
            Pixel p = cur->GetPixel((int)cp.x, (int)cp.y);
            g_Color = { p.r / 255.f, p.g / 255.f, p.b / 255.f, 1 };
            return;
        }
        if (g_Tool == 2 && ImGui::IsMouseClicked(0)) {
            g_TL.pushUndo();
            cur->FloodFill((int)cp.x, (int)cp.y,
                (uint8_t)(g_Color.x * 255), (uint8_t)(g_Color.y * 255), (uint8_t)(g_Color.z * 255));
            cur->UploadTexture();
            return;
        }

        if (g_Tool != 0 && g_Tool != 1) return;

        if (ImGui::IsMouseClicked(0)) {
            g_TL.pushUndo();
            g_Drawing = true;
            g_LastDrawPos = cp;
        }

        if (g_Drawing && ldown) {
            float dx = cp.x - g_LastDrawPos.x, dy = cp.y - g_LastDrawPos.y;
            float dist = sqrtf(dx * dx + dy * dy);
            float step = max(1.0f, g_BrushSize * 0.4f);
            int   n = max(1, (int)(dist / step));
            for (int i = 0; i <= n; i++) {
                float t = n > 0 ? (float)i / n : 0;
                float px = g_LastDrawPos.x + dx * t;
                float py = g_LastDrawPos.y + dy * t;
                if (g_Tool == 0)
                    cur->PaintCircle(px, py, g_BrushSize, g_Opacity,
                        (uint8_t)(g_Color.x * 255), (uint8_t)(g_Color.y * 255), (uint8_t)(g_Color.z * 255));
                else
                    cur->EraseCircle(px, py, g_BrushSize, g_Opacity);
                expandDirty(px, py, g_BrushSize);
            }
            g_LastDrawPos = cp;
            flushDirty(cur);
        }

        if (!ldown && g_Drawing) { g_Drawing = false; g_LastDrawPos = { -1,-1 }; }
    }

    static void drawOnionSkins(ImDrawList* dl, const ImVec2& tl, const ImVec2& br) {
        int cur = g_TL.currentFrame;

        if (g_TL.onionBack) {
            for (int d = 2; d >= 1; d--) {
                Canvas* c = g_TL.at(cur - d);
                if (!c) continue;
                float a = g_TL.onionAlpha / (float)d;
                dl->AddImage((ImTextureID)(uintptr_t)c->TextureID(), tl, br,
                    { 0,0 }, { 1,1 }, ImColor(1.f, .35f, .35f, a));
            }
        }

        if (g_TL.onionForward) {
            Canvas* c = g_TL.at(cur + 1);
            if (c)
                dl->AddImage((ImTextureID)(uintptr_t)c->TextureID(), tl, br,
                    { 0,0 }, { 1,1 }, ImColor(.35f, .55f, 1.f, g_TL.onionAlpha));
        }
    }

    void Engine::drawInterface() {
        if (g_TL.frames.empty()) g_TL.addFrame(960, 540);

        applyStyle();

        ImGuiIO& io = ImGui::GetIO();
        g_Input.Process();

        if (g_Input.Fired(Action::SelectBrush))      g_Tool = 0;
        if (g_Input.Fired(Action::SelectEraser))     g_Tool = 1;
        if (g_Input.Fired(Action::SelectBucket))     g_Tool = 2;
        if (g_Input.Fired(Action::SelectEyedropper)) g_Tool = 3;
        if (g_Input.Fired(Action::Undo))             g_TL.undo();
        if (g_Input.Fired(Action::Redo))             g_TL.redo();
        if (g_Input.Fired(Action::NextFrame)) { g_TL.playing = false; g_TL.next(); }
        if (g_Input.Fired(Action::PrevFrame)) { g_TL.playing = false; g_TL.prev(); }
        if (g_Input.Fired(Action::TogglePlay))       g_TL.playing = !g_TL.playing;
        if (g_Input.Fired(Action::ZoomIn))           g_VP.ZoomAround(io.MousePos, 1.2f);
        if (g_Input.Fired(Action::ZoomOut))          g_VP.ZoomAround(io.MousePos, 1.0f / 1.2f);
        if (g_Input.Fired(Action::FitCanvas)) {
            auto* c = g_TL.current();
            if (c) g_VP.FitToPanel(c->Width(), c->Height());
        }
        if (g_Input.Fired(Action::ClearFrame)) {
            auto* c = g_TL.current();
            if (c) { g_TL.pushUndo(); c->Clear(); c->UploadTexture(); }
        }
        if (g_Input.Fired(Action::KillProcess)) exit(0);

        g_TL.advance(io.DeltaTime);

        float W = io.DisplaySize.x, H = io.DisplaySize.y;
        float menuH = 19.f;
        float toolW = 46.f;
        float propW = 214.f;
        float tlH = 175.f;
        float viewW = W - toolW - propW;
        float viewH = H - menuH - tlH;
        ImVec2 vpos = { toolW, menuH };
        ImVec2 vsz = { viewW, viewH };

        ImVec4 acc = { .28f, .50f, .92f, 1 };
        ImVec4 accDim = { .28f, .50f, .92f, .32f };
        ImVec4 bg0 = { .07f, .07f, .08f, 1 };
        ImVec4 bg1 = { .12f, .12f, .13f, 1 };
        ImVec4 bg2 = { .18f, .18f, .19f, 1 };
        ImVec4 bg3 = { .25f, .25f, .27f, 1 };
        ImVec4 dim = { .42f, .42f, .46f, 1 };

        g_VP.SetPanelBounds(vpos, vsz);
        if (!g_VPFitted && g_TL.current()) {
            g_VP.FitToPanel(g_TL.current()->Width(), g_TL.current()->Height());
            g_VPFitted = true;
        }

        bool panning = ImGui::IsMouseDown(ImGuiMouseButton_Middle)
            || (ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDown(ImGuiMouseButton_Left));
        bool overView = ImGui::IsMouseHoveringRect(vpos, { vpos.x + vsz.x, vpos.y + vsz.y });

        // ── MENU BAR ──
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8,4 });
        if (ImGui::BeginMainMenuBar()) {
            auto* cur = g_TL.current();
            if (ImGui::BeginMenu("File")) {
                ImGui::MenuItem("New", "Ctrl+N");
                ImGui::MenuItem("Open", "Ctrl+O");
                ImGui::Separator();
                ImGui::MenuItem("Save", "Ctrl+S");
                ImGui::MenuItem("Save As", "Ctrl+Shift+S");
                ImGui::Separator();
                if (ImGui::BeginMenu("Export")) {
                    ImGui::MenuItem("MP4");
                    ImGui::MenuItem("GIF");
                    ImGui::MenuItem("Sprite Sheet");
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) m_KeepRunning = false;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z"))    g_TL.undo();
                if (ImGui::MenuItem("Redo", "Ctrl+Y"))    g_TL.redo();
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Frame", "Del") && cur) { g_TL.pushUndo(); cur->Clear(); cur->UploadTexture(); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Zoom In", "Ctrl++")) g_VP.ZoomAround(io.MousePos, 1.2f);
                if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) g_VP.ZoomAround(io.MousePos, 1.0f / 1.2f);
                if (ImGui::MenuItem("Fit", "Ctrl+0") && cur) g_VP.FitToPanel(cur->Width(), cur->Height());
                ImGui::EndMenu();
            }
            ImGui::SetCursorPosX(W - ImGui::CalcTextSize("Anima  1.0").x - 18);
            ImGui::PushStyleColor(ImGuiCol_Text, dim);
            ImGui::Text("Anima  1.0");
            ImGui::PopStyleColor();
            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar();

        // ── TOOLBAR ──
        ImGui::SetNextWindowPos({ 0, menuH });
        ImGui::SetNextWindowSize({ toolW, viewH });
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 4,8 });
        ImGui::Begin("##tb", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        struct TD { const char* lbl; const char* tip; int id; };
        TD tools[] = { {"Br","Brush (B)",0},{"Er","Eraser (E)",1},{"Bk","Bucket (G)",2},{"Ey","Eyedrop (I)",3} };

        for (auto& t : tools) {
            bool act = g_Tool == t.id;
            if (act) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled({ p.x - 4,p.y }, { p.x - 1,p.y + 34 }, IM_COL32(72, 130, 230, 255));
            }
            if (toolBtn(t.lbl, { 38,34 }, act, acc)) g_Tool = t.id;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.tip);
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, { .19f,.19f,.21f,1 }); ImGui::Separator(); ImGui::PopStyleColor();
        ImGui::Spacing();

        if (toolBtn("Un", { 38,28 }, false, acc)) g_TL.undo();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo  Ctrl+Z");
        if (toolBtn("Re", { 38,28 }, false, acc)) g_TL.redo();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Redo  Ctrl+Y");

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Separator, { .19f,.19f,.21f,1 }); ImGui::Separator(); ImGui::PopStyleColor();
        ImGui::Spacing();

        ImVec2 sp = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##sw", { 38,38 });
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##swpop");
        ImGui::GetWindowDrawList()->AddRectFilled(sp, { sp.x + 38,sp.y + 38 },
            IM_COL32((int)(g_Color.x * 255), (int)(g_Color.y * 255), (int)(g_Color.z * 255), 255), 5);
        ImGui::GetWindowDrawList()->AddRect(sp, { sp.x + 38,sp.y + 38 }, IM_COL32(70, 70, 78, 255), 5, 0, 1.5f);
        if (ImGui::BeginPopup("##swpop")) {
            ImGui::ColorPicker4("##cp", (float*)&g_Color,
                ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
            ImGui::EndPopup();
        }

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // ── VIEWPORT ──
        ImGui::SetNextWindowPos(vpos);
        ImGui::SetNextWindowSize(vsz);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, { .07f,.07f,.08f,1 });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0,0 });
        ImGui::Begin("##vp", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        g_VP.HandleInput(overView);
        handleDraw(vpos, vsz, panning);

        ImDrawList* vdl = ImGui::GetWindowDrawList();

        // Checkerboard
        {
            float ts = 14;
            int tx = (int)(viewW / ts) + 2, ty = (int)(viewH / ts) + 2;
            for (int r = 0; r < ty; r++)
                for (int col = 0; col < tx; col++) {
                    ImU32 col32 = (r + col) % 2 == 0 ? IM_COL32(20, 20, 22, 255) : IM_COL32(16, 16, 18, 255);
                    vdl->AddRectFilled({ vpos.x + col * ts,vpos.y + r * ts }, { vpos.x + (col + 1) * ts,vpos.y + (r + 1) * ts }, col32);
                }
        }

        auto* cur = g_TL.current();
        if (cur) {
            ImVec2 tl = g_VP.CanvasToScreen({ 0,0 });
            ImVec2 br = g_VP.CanvasToScreen({ (float)cur->Width(),(float)cur->Height() });

            vdl->AddRectFilled({ tl.x + 5,tl.y + 5 }, { br.x + 5,br.y + 5 }, IM_COL32(0, 0, 0, 90), 3);

            vdl->AddRectFilled(tl, br, IM_COL32(255, 255, 255, 255));

            drawOnionSkins(vdl, tl, br);

            vdl->AddImage((ImTextureID)(uintptr_t)cur->TextureID(), tl, br, { 0,0 }, { 1,1 });

            vdl->AddRect({ tl.x - 1,tl.y - 1 }, { br.x + 1,br.y + 1 }, IM_COL32(55, 55, 65, 220), 0, 0, 1.5f);

            if (overView && !panning && (g_Tool == 0 || g_Tool == 1)) {
                float sr = g_BrushSize * g_VP.Zoom();
                vdl->AddCircle(io.MousePos, sr, IM_COL32(200, 200, 210, 160), 48, 1.f);
                vdl->AddCircleFilled(io.MousePos, 1.5f, IM_COL32(255, 255, 255, 220));
            }

            char zbuf[12];
            snprintf(zbuf, sizeof(zbuf), "%.0f%%", g_VP.Zoom() * 100);
            ImVec2 zp = { vpos.x + viewW - 56, vpos.y + viewH - 26 };
            vdl->AddRectFilled(zp, { zp.x + 48,zp.y + 18 }, IM_COL32(0, 0, 0, 130), 4);
            vdl->AddText({ zp.x + 6,zp.y + 3 }, IM_COL32(180, 180, 200, 255), zbuf);

            const char* tn[] = { "Brush","Eraser","Bucket","Eyedrop" };
            char info[64];
            snprintf(info, sizeof(info), "Fr %d/%d  %s  %.0fpx",
                g_TL.currentFrame + 1, g_TL.frameCount(), tn[g_Tool], g_BrushSize);
            float iw = ImGui::CalcTextSize(info).x + 14;
            ImVec2 ip = { vpos.x + 10, vpos.y + 10 };
            vdl->AddRectFilled(ip, { ip.x + iw,ip.y + 20 }, IM_COL32(0, 0, 0, 130), 4);
            vdl->AddText({ ip.x + 7,ip.y + 4 }, IM_COL32(210, 210, 230, 240), info);
        }

        if (overView) ImGui::SetMouseCursor(ImGuiMouseCursor_None);

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // ── PROPERTIES ──
        ImGui::SetNextWindowPos({ toolW + viewW, menuH });
        ImGui::SetNextWindowSize({ propW, viewH });
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg1);
        ImGui::Begin("##pr", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        sectionLabel("BRUSH");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##sz", &g_BrushSize, 1, 100, "Size  %.0fpx");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##op", &g_Opacity, 0, 1, "Opacity  %.0f%%");

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, { g_Color.x,g_Color.y,g_Color.z,1 });
        ImGui::BeginChild("##cb", { -1,20 }, false);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("##pcp");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to change color");
        if (ImGui::BeginPopup("##pcp")) {
            ImGui::ColorPicker4("##pp", (float*)&g_Color,
                ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
            ImGui::EndPopup();
        }

        sectionLabel("ONION SKIN");
        ImGui::Checkbox("Past (red)", &g_TL.onionBack);
        ImGui::Checkbox("Future (blue)", &g_TL.onionForward);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##oa", &g_TL.onionAlpha, .02f, .6f, "Intensity  %.2f");

        sectionLabel("FRAME");
        if (ImGui::Button("+ Add", { -1,22 })) {
            auto* c = g_TL.current();
            g_TL.addFrame(c ? c->Width() : 960, c ? c->Height() : 540);
            g_TL.currentFrame = g_TL.frameCount() - 1;
        }
        if (ImGui::Button("Duplicate", { -1,22 }))
            g_TL.duplicateFrame(g_TL.currentFrame);
        if (ImGui::Button("Delete", { -1,22 }))
            g_TL.deleteFrame(g_TL.currentFrame);

        sectionLabel("CANVAS");
        {
            auto* c = g_TL.current();
            ImGui::PushStyleColor(ImGuiCol_Text, dim);
            if (c) ImGui::Text("%d × %d  |  %.0f%%", c->Width(), c->Height(), g_VP.Zoom() * 100);
            ImGui::Text("Frames: %d  |  FPS: %d", g_TL.frameCount(), g_TL.fps);
            ImGui::PopStyleColor();
        }

        sectionLabel("KEYS");
        ImGui::PushStyleColor(ImGuiCol_Text, dim);
        ImGui::TextWrapped("B Brush  E Eraser  G Bucket  I Pick\n"
            "Space Play  ← → Frame\n"
            "Scroll/Ctrl+/-  Zoom\n"
            "Mid / Space+Drag  Pan\n"
            "Ctrl+Z/Y  Undo/Redo");
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleColor();

        // ── TIMELINE ──
        ImGui::SetNextWindowPos({ 0, menuH + viewH });
        ImGui::SetNextWindowSize({ W, tlH });
        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8,6 });
        ImGui::Begin("##tl", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        ImVec2 tlp = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddLine(tlp, { tlp.x + W,tlp.y }, IM_COL32(60, 110, 220, 160), 1.5f);

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, g_TL.playing ? acc : bg2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, acc);
        if (ImGui::Button(g_TL.playing ? " || " : "  >  ", { 44,26 })) g_TL.playing = !g_TL.playing;
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_FrameBg, bg2);
        ImGui::SetNextItemWidth(W - 320);
        int seeker = g_TL.currentFrame;
        if (ImGui::SliderInt("##sk", &seeker, 0, max(0, g_TL.frameCount() - 1), "")) {
            g_TL.currentFrame = seeker;
            g_TL.playing = false;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, dim);
        ImGui::Text("%03d / %03d", g_TL.currentFrame + 1, g_TL.frameCount());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 16);
        ImGui::PushStyleColor(ImGuiCol_Text, dim); ImGui::Text("FPS"); ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bg2);
        ImGui::SetNextItemWidth(36);
        ImGui::InputInt("##fp", &g_TL.fps, 0, 0);
        ImGui::PopStyleColor();
        g_TL.fps = clamp(g_TL.fps, 1, 120);
        ImGui::SameLine(0, 14);
        ImGui::PushStyleColor(ImGuiCol_CheckMark, acc);
        ImGui::Checkbox("Loop", &g_TL.looping);
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2,0 });
        ImGui::BeginChild("##fs", { 0,0 }, false, ImGuiWindowFlags_HorizontalScrollbar);

        int fc = g_TL.frameCount();
        for (int i = 0; i < fc; i++) {
            if (i > 0) ImGui::SameLine(0, 2);
            bool isc = (g_TL.currentFrame == i);
            Canvas* fc_c = g_TL.at(i);
            bool has = fc_c != nullptr;

            ImVec4 bc = isc ? acc : (has ? ImVec4(.26f, .26f, .29f, 1) : ImVec4(.13f, .13f, .15f, 1));
            ImGui::PushStyleColor(ImGuiCol_Button, bc);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isc ? acc : bg3);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2);

            char lbl[12]; snprintf(lbl, sizeof(lbl), "##f%d", i);
            if (ImGui::Button(lbl, { 24,56 })) { g_TL.currentFrame = i; g_TL.playing = false; }

            ImVec2 bp = ImGui::GetItemRectMin();
            if (i % 5 == 0) {
                char n[6]; snprintf(n, sizeof(n), "%d", i);
                ImGui::GetWindowDrawList()->AddText({ bp.x + 3,bp.y + 38 },
                    isc ? IM_COL32(255, 255, 255, 220) : IM_COL32(80, 80, 95, 255), n);
            }
            if (has && !isc)
                ImGui::GetWindowDrawList()->AddCircleFilled({ bp.x + 12,bp.y + 15 }, 3, IM_COL32(90, 150, 255, 200));

            if (i == g_TL.onionBack ? g_TL.currentFrame - 1 : -99)
                ImGui::GetWindowDrawList()->AddRect({ bp.x,bp.y }, { bp.x + 24,bp.y + 56 }, IM_COL32(255, 90, 90, 160), 2);
            if (i == (g_TL.onionForward ? g_TL.currentFrame + 1 : -99))
                ImGui::GetWindowDrawList()->AddRect({ bp.x,bp.y }, { bp.x + 24,bp.y + 56 }, IM_COL32(90, 130, 255, 160), 2);

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