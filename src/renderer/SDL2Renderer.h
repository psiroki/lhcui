#pragma once

#include "hui/IRenderer.h"
#include <vector>
#include <unordered_map>

#include <SDL.h>
#include <SDL_ttf.h>

namespace hui {

class SDL2Renderer : public IRenderer {
public:
    explicit SDL2Renderer(SDL_Renderer* renderer);
    ~SDL2Renderer() override;

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

    // Optional helpers to register fonts
    FontHandle registerFont(TTF_Font* font);

private:
    SDL_Renderer* renderer_;
    uint8_t globalAlpha_ = 255;
    
    std::vector<Rect> clipStack_;
    
    std::unordered_map<FontHandle, TTF_Font*> fonts_;
    uint32_t nextFontHandle_ = 1;
    
    std::unordered_map<TextureHandle, SDL_Texture*> textures_;
    uint32_t nextTextureHandle_ = 1;

    void applyClip();
};

} // namespace hui
