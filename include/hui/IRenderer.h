#pragma once

#include "hui/types.h"
#include <string_view>
#include <cstdint>

namespace hui {

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // --- Frame lifecycle ---
    virtual void beginFrame() = 0;
    virtual void endFrame()   = 0;   // present / flip

    // --- Clipping ---
    // Clips are additive intersections; push/pop form a stack.
    virtual void pushClip(Rect r)  = 0;
    virtual void popClip()         = 0;

    // --- Primitives ---
    virtual void fillRect(Rect r, Color c)                     = 0;
    virtual void drawRect(Rect r, Color c, int thickness = 1)  = 0;
    virtual void drawLine(Point a, Point b, Color c)           = 0;

    // --- Text ---
    // Returns rendered advance width in pixels.
    virtual int  drawText(std::string_view text,
                          Point origin,
                          FontHandle font,
                          Color color)                         = 0;
    virtual Size measureText(std::string_view text,
                             FontHandle font)                  = 0;

    // Draws text clipped to maxWidth pixels with "…" suffix if it overflows.
    virtual void drawTextEllipsis(std::string_view text,
                                  Point origin,
                                  FontHandle font,
                                  Color color,
                                  int maxWidth)                = 0;

    // --- Images / Textures ---
    virtual TextureHandle loadTexture(std::string_view path)   = 0;
    virtual void          freeTexture(TextureHandle h)         = 0;
    virtual Size          textureSize(TextureHandle h)         = 0;
    virtual void          drawTexture(TextureHandle h,
                                      Rect dst,
                                      uint8_t alpha = 255)     = 0;

    // --- Alpha modulation ---
    // Sets a global multiplier applied to all subsequent draw calls.
    // Used to dim views behind overlays.
    virtual void setGlobalAlpha(uint8_t alpha) = 0;

    // --- Querying ---
    virtual Size screenSize() const = 0;

    // --- Text cache (§13.1) ---
    // Called by list containers when row data changes. Default is a no-op.
    virtual void invalidateTextCache() {}
};

} // namespace hui
