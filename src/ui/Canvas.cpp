#include "Canvas.h"
#include <SDL_opengl.h>
#include <cmath>
#include <algorithm>
#include <stack>

float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

namespace Anima {

    Canvas::Canvas() {}
    Canvas::~Canvas() { Destroy(); }

    bool Canvas::Init(int width, int height) {
        m_Width = width;
        m_Height = height;

        m_Pixels.assign(width * height, Pixel{ 255, 255, 255, 0 });

        glGenTextures(1, &m_TexID);
        glBindTexture(GL_TEXTURE_2D, m_TexID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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


    Pixel Canvas::GetPixel(int x, int y) const {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
            return { 0, 0, 0, 0 };
        return m_Pixels[y * m_Width + x];
    }

    void Canvas::SetPixel(int x, int y, Pixel p) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;
        m_Pixels[y * m_Width + x] = p;
    }

    void Canvas::BlendPixel(int x, int y, Pixel src) {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;

        Pixel& dst = m_Pixels[y * m_Width + x];

        if (src.a == 0) return;
        if (src.a == 255) { dst = src; return; }

        float srcA = src.a / 255.0f;
        float dstA = dst.a / 255.0f;
        float outA = srcA + dstA * (1.0f - srcA);

        if (outA <= 0.0f) return;

        dst.r = (uint8_t)((src.r * srcA + dst.r * dstA * (1.0f - srcA)) / outA);
        dst.g = (uint8_t)((src.g * srcA + dst.g * dstA * (1.0f - srcA)) / outA);
        dst.b = (uint8_t)((src.b * srcA + dst.b * dstA * (1.0f - srcA)) / outA);
        dst.a = (uint8_t)(outA * 255.0f);
    }


    void Canvas::PaintCircle(float cx, float cy, float radius, float opacity, uint8_t r, uint8_t g, uint8_t b) {
        int x0 = (int)(cx - radius);
        int y0 = (int)(cy - radius);
        int x1 = (int)(cx + radius);
        int y1 = (int)(cy + radius);

        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                float dx = x - cx;
                float dy = y - cy;
                float distSq = dx * dx + dy * dy;
                float radSq = radius * radius;

                if (distSq < radSq) {
                    float dist = sqrtf(distSq);

                    float falloff = 1.0f - smoothstep(radius * 0.8f, radius, dist);
                    uint8_t finalAlpha = (uint8_t)(opacity * falloff * 255);

                    BlendPixel(x, y, Pixel{ r, g, b, finalAlpha });
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

        Pixel target = GetPixel(startX, startY);

        if (target.r == r && target.g == g && target.b == b && target.a == 255)
            return;
        std::stack<std::pair<int, int>> toVisit;
        toVisit.push({ startX, startY });

        while (!toVisit.empty()) {
            auto [x, y] = toVisit.top();
            toVisit.pop();

            if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) continue;

            Pixel here = GetPixel(x, y);
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
        std::fill(m_Pixels.begin(), m_Pixels.end(), Pixel{ 255, 255, 255, 0 });
    }

    void Canvas::UploadTexture() {
        glBindTexture(GL_TEXTURE_2D, m_TexID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            m_Width, m_Height,
            GL_RGBA, GL_UNSIGNED_BYTE,
            m_Pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Canvas::UploadRegion(int x, int y, int w, int h) {
        glBindTexture(GL_TEXTURE_2D, m_TexID);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_Width);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, x);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, y);

        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
            GL_RGBA, GL_UNSIGNED_BYTE,
            m_Pixels.data());

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
