#pragma once

#include "hui/IRenderer.h"

#ifdef HUI_USE_SDL1

#include <vector>
#include <unordered_map>
#include <string>

#include <SDL.h>
#include <SDL_ttf.h>

namespace hui {

class SDL1Renderer : public IRenderer {
public:
    explicit SDL1Renderer(SDL_Surface* screen);
    ~SDL1Renderer() override;

    void beginFrame() override;
    void endFrame() override;

    void pushClip(Rect r) override;
    void popClip() override;

    void fillRect(Rect r, Color c) override;
    void drawRect(Rect r, Color c, int thickness = 1) override;
    void drawLine(Point a, Point b, Color c) override;

    int drawText(std::string_view text, Point origin, FontHandle font, Color color) override;
    Size measureText(std::string_view text, FontHandle font) override;
    void drawTextEllipsis(std::string_view text, Point origin, FontHandle font, Color color, int maxWidth) override;

    TextureHandle loadTexture(std::string_view path) override;
    void freeTexture(TextureHandle h) override;
    Size textureSize(TextureHandle h) override;
    void drawTexture(TextureHandle h, Rect dst, uint8_t alpha = 255) override;

    void setGlobalAlpha(uint8_t alpha) override;

    Size screenSize() const override;

    void invalidateTextCache() override;

    // Optional helpers to register fonts
    FontHandle registerFont(TTF_Font* font);

private:
    SDL_Surface* screen_;
    uint8_t globalAlpha_ = 255;
    
    std::vector<Rect> clipStack_;
    
    std::unordered_map<FontHandle, TTF_Font*> fonts_;
    uint32_t nextFontHandle_ = 1;
    
    std::unordered_map<TextureHandle, SDL_Surface*> textures_;
    uint32_t nextTextureHandle_ = 1;
    
    // Simple glyph/string surface cache
    struct CacheKey {
        std::string text;
        FontHandle font;
        Color color;
        
        bool operator==(const CacheKey& o) const {
            return text == o.text && font == o.font && 
                   color.r == o.color.r && color.g == o.color.g && 
                   color.b == o.color.b && color.a == o.color.a;
        }
    };
    
    struct CacheKeyHash {
        std::size_t operator()(const CacheKey& k) const {
            std::size_t h1 = std::hash<std::string>()(k.text);
            std::size_t h2 = std::hash<uint32_t>()(k.font);
            return h1 ^ (h2 << 1);
        }
    };
    
    std::unordered_map<CacheKey, SDL_Surface*, CacheKeyHash> textCache_;

    void applyClip();
    uint32_t mapColor(Color c);
};

} // namespace hui

#endif // HUI_USE_SDL1
