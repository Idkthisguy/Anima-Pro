// Canvas.cpp — pixel buffer + OpenGL texture management
// Ivan: OpenGL textures are GPU memory. We keep a CPU copy (m_Pixels)
// so we can read pixels back (for flood fill, eyedropper, saving).
// The GPU copy is only updated when we call UploadTexture/UploadRegion.

#include "Canvas.h"
#include <SDL_opengl.h>
#include <cmath>
#include <algorithm>
#include <stack>

namespace Anima {

    Canvas::Canvas() {}
    Canvas::~Canvas() { Destroy(); }

    bool Canvas::Init(int width, int height) {
        m_Width = width;
        m_Height = height;

        // Allocate all pixels as fully transparent white
        m_Pixels.assign(width * height, Pixel{ 255, 255, 255, 0 });

        // Create an OpenGL texture
        // glGenTextures asks the GPU driver for a texture slot and gives us an ID
        glGenTextures(1, &m_TexID);
        glBindTexture(GL_TEXTURE_2D, m_TexID);

        // GL_NEAREST = no interpolation when zoomed in (pixel art style)
        // GL_LINEAR  = smooth when zoomed in (painting style)
        // We use LINEAR so brush strokes look smooth at high zoom
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Upload the blank canvas to the GPU for the first time
        // GL_RGBA8 = 4 bytes per pixel on the GPU
        // GL_UNSIGNED_BYTE = our pixel values are 0-255
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
            width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE,
            m_Pixels.data());

        glBindTexture(GL_TEXTURE_2D, 0);
        return (m_TexID != 0);
    }

    void Canvas::Destroy() {
        if (m_TexID) {
            glDeleteTextures(1, &m_TexID);
            m_TexID = 0;
        }
        m_Pixels.clear();
    }

    // --- Pixel access ---

    Pixel Canvas::GetPixel(int x, int y) const {
        // Bounds check — returning transparent avoids crashes at canvas edges
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
            return { 0, 0, 0, 0 };
        return m_Pixels[y * m_Width + x];
    }

    void Canvas::SetPixel(int x, int y, Pixel p) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;
        m_Pixels[y * m_Width + x] = p;
    }

    // --- Alpha compositing ---
    // Formula: out = src * srcA + dst * (1 - srcA)
    // This is "Porter-Duff over" — what Photoshop uses for every brush stroke
    Pixel Canvas::BlendPixel(Pixel dst, uint8_t r, uint8_t g, uint8_t b, float opacity) {
        float a = opacity;
        float ia = 1.0f - a;
        return {
            (uint8_t)(r * a + dst.r * ia),
            (uint8_t)(g * a + dst.g * ia),
            (uint8_t)(b * a + dst.b * ia),
            (uint8_t)(255 * a + dst.a * ia)
        };
    }

    // --- Drawing ---

    void Canvas::PaintCircle(float cx, float cy, float radius,
        float opacity,
        uint8_t r, uint8_t g, uint8_t b) {
        // Bounding box of the circle — only iterate pixels we might touch
        int x0 = (int)(cx - radius) - 1;
        int y0 = (int)(cy - radius) - 1;
        int x1 = (int)(cx + radius) + 1;
        int y1 = (int)(cy + radius) + 1;

        // Clamp to canvas bounds
        x0 = std::max(0, x0);
        y0 = std::max(0, y0);
        x1 = std::min(m_Width - 1, x1);
        y1 = std::min(m_Height - 1, y1);

        for (int py = y0; py <= y1; py++) {
            for (int px = x0; px <= x1; px++) {
                // Distance from pixel center to brush center
                float dx = px - cx;
                float dy = py - cy;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist <= radius) {
                    // Soft falloff: full opacity at center, fades to 0 at edge
                    // This gives the brush a "feathered" look
                    float falloff = 1.0f - (dist / radius);
                    falloff = falloff * falloff; // square it for smoother edge

                    Pixel dst = GetPixel(px, py);
                    SetPixel(px, py, BlendPixel(dst, r, g, b, opacity * falloff));
                }
            }
        }
    }

    void Canvas::EraseCircle(float cx, float cy, float radius, float opacity) {
        int x0 = std::max(0, (int)(cx - radius) - 1);
        int y0 = std::max(0, (int)(cy - radius) - 1);
        int x1 = std::min(m_Width - 1, (int)(cx + radius) + 1);
        int y1 = std::min(m_Height - 1, (int)(cy + radius) + 1);

        for (int py = y0; py <= y1; py++) {
            for (int px = x0; px <= x1; px++) {
                float dx = px - cx;
                float dy = py - cy;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist <= radius) {
                    float falloff = 1.0f - (dist / radius);
                    falloff = falloff * falloff;

                    Pixel dst = GetPixel(px, py);
                    // Erasing = reducing alpha toward 0
                    float newAlpha = dst.a * (1.0f - opacity * falloff);
                    dst.a = (uint8_t)newAlpha;
                    SetPixel(px, py, dst);
                }
            }
        }
    }

    void Canvas::FloodFill(int startX, int startY,
        uint8_t r, uint8_t g, uint8_t b) {
        if (startX < 0 || startX >= m_Width ||
            startY < 0 || startY >= m_Height) return;

        // The color we're replacing
        Pixel target = GetPixel(startX, startY);

        // Don't fill if already the same color
        if (target.r == r && target.g == g && target.b == b && target.a == 255)
            return;

        // Iterative flood fill using an explicit stack
        // Ivan: recursive flood fill crashes on big canvases due to stack overflow
        // An explicit stack keeps everything on the heap
        std::stack<std::pair<int, int>> toVisit;
        toVisit.push({ startX, startY });

        while (!toVisit.empty()) {
            auto [x, y] = toVisit.top();
            toVisit.pop();

            if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) continue;

            Pixel here = GetPixel(x, y);
            // Only fill pixels that match the original target color
            if (here.r != target.r || here.g != target.g ||
                here.b != target.b || here.a != target.a) continue;

            SetPixel(x, y, { r, g, b, 255 });

            toVisit.push({ x + 1, y });
            toVisit.push({ x - 1, y });
            toVisit.push({ x, y + 1 });
            toVisit.push({ x, y - 1 });
        }
    }

    void Canvas::Clear() {
        // Reset all pixels to transparent white
        std::fill(m_Pixels.begin(), m_Pixels.end(), Pixel{ 255, 255, 255, 0 });
    }

    // --- Texture upload ---

    void Canvas::UploadTexture() {
        // glTexSubImage2D replaces texture data without reallocating
        // Faster than glTexImage2D which destroys and recreates the texture
        glBindTexture(GL_TEXTURE_2D, m_TexID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            m_Width, m_Height,
            GL_RGBA, GL_UNSIGNED_BYTE,
            m_Pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Canvas::UploadRegion(int x, int y, int w, int h) {
        // glPixelStorei tells OpenGL that our rows are m_Width pixels wide
        // so it knows where each row starts in the buffer when uploading a sub-region
        glBindTexture(GL_TEXTURE_2D, m_TexID);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_Width);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, y);

        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
            GL_RGBA, GL_UNSIGNED_BYTE,
            m_Pixels.data());

        // Reset to defaults so other uploads aren't affected
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

} // namespace Anima