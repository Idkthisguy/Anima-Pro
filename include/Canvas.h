#pragma once
// Canvas.h — owns the pixel data for one animation frame
// and the OpenGL texture that gets displayed on screen.
// Ivan: think of this like a single sheet of paper.
// The pixels live in RAM as a plain array of RGBA bytes.
// The texture is the GPU copy — we upload pixels → GPU once per stroke end.

#include <vector>
#include <cstdint>

namespace Anima {

    // One RGBA pixel — 4 bytes, values 0-255
    struct Pixel {
        uint8_t r, g, b, a;
    };

    class Canvas {
    public:
        Canvas();
        ~Canvas();

        // Allocate pixel buffer + create OpenGL texture
        // Call once after OpenGL is ready
        bool Init(int width, int height);

        // Destroy the GL texture and free pixel memory
        void Destroy();

        // --- Drawing ---
        // Paint a soft circle at (x,y) in canvas-pixel coordinates.
        // radius, opacity, and color all come from the tool settings.
        void PaintCircle(float x, float y, float radius,
            float opacity,
            uint8_t r, uint8_t g, uint8_t b);

        // Erase: same as PaintCircle but writes transparent pixels
        void EraseCircle(float x, float y, float radius, float opacity);

        // Fill: flood-fill from seed point with a color
        void FloodFill(int x, int y, uint8_t r, uint8_t g, uint8_t b);

        // Clear all pixels to transparent
        void Clear();

        // --- Texture ---
        // Upload the pixel buffer to the GPU texture.
        // Call this after a stroke finishes (not every frame — it's slow).
        void UploadTexture();

        // Upload only a dirty rectangle instead of the whole canvas.
        // Much faster for incremental drawing.
        void UploadRegion(int x, int y, int w, int h);

        // The OpenGL texture ID — pass to ImGui::Image() to display
        unsigned int TextureID() const { return m_TexID; }

        int Width()  const { return m_Width; }
        int Height() const { return m_Height; }

        // Direct pixel access — bounds-checked
        Pixel  GetPixel(int x, int y) const;
        void   SetPixel(int x, int y, Pixel p);

        // Raw buffer access for saving/loading
        const std::vector<Pixel>& Pixels() const { return m_Pixels; }
        std::vector<Pixel>& Pixels() { return m_Pixels; }

    private:
        int  m_Width = 0;
        int  m_Height = 0;
        unsigned int m_TexID = 0;          // OpenGL texture handle
        std::vector<Pixel> m_Pixels;       // CPU-side pixel buffer

        // Blend src color+opacity onto dst using standard alpha compositing
        // Ivan: this is why brush strokes don't just stamp opaque squares
        void BlendPixel(int x, int y, Pixel src);
    };

} // namespace Anima