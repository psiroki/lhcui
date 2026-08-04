#ifndef HUI_USE_SDL1

#include "SDL2Renderer.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <string>

namespace hui {

SDL2Renderer::SDL2Renderer(SDL_Renderer* renderer)
    : renderer_(renderer) {
}

SDL2Renderer::~SDL2Renderer() {
    for (auto& pair : textures_) {
        SDL_DestroyTexture(pair.second);
    }
}

void SDL2Renderer::beginFrame() {
    clipStack_.clear();
    SDL_RenderSetClipRect(renderer_, nullptr);
}

void SDL2Renderer::endFrame() {
    SDL_RenderPresent(renderer_);
}

void SDL2Renderer::applyClip() {
    if (clipStack_.empty()) {
        SDL_RenderSetClipRect(renderer_, nullptr);
    } else {
        Rect c = clipStack_.back();
        SDL_Rect r{c.x, c.y, c.w, c.h};
        SDL_RenderSetClipRect(renderer_, &r);
    }
}

void SDL2Renderer::pushClip(Rect r) {
    if (!clipStack_.empty()) {
        Rect cur = clipStack_.back();
        int x1 = std::max(cur.x, r.x);
        int y1 = std::max(cur.y, r.y);
        int x2 = std::min(cur.x + cur.w, r.x + r.w);
        int y2 = std::min(cur.y + cur.h, r.y + r.h);
        r = {x1, y1, std::max(0, x2 - x1), std::max(0, y2 - y1)};
    }
    clipStack_.push_back(r);
    applyClip();
}

void SDL2Renderer::popClip() {
    if (!clipStack_.empty()) {
        clipStack_.pop_back();
        applyClip();
    }
}

void SDL2Renderer::fillRect(Rect r, Color c) {
    uint8_t alpha = (c.a * globalAlpha_) / 255;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, alpha);
    SDL_Rect rect{r.x, r.y, r.w, r.h};
    SDL_RenderFillRect(renderer_, &rect);
}

void SDL2Renderer::drawRect(Rect r, Color c, int thickness) {
    if (thickness <= 0) return;
    uint8_t alpha = (c.a * globalAlpha_) / 255;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, alpha);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect rect{r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i};
        SDL_RenderDrawRect(renderer_, &rect);
    }
}

void SDL2Renderer::drawLine(Point a, Point b, Color c) {
    uint8_t alpha = (c.a * globalAlpha_) / 255;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, alpha);
    SDL_RenderDrawLine(renderer_, a.x, a.y, b.x, b.y);
}

FontHandle SDL2Renderer::registerFont(TTF_Font* font) {
    FontHandle handle = nextFontHandle_++;
    fonts_[handle] = font;
    return handle;
}

int SDL2Renderer::drawText(std::string_view text, Point origin, FontHandle font, Color color) {
    auto it = fonts_.find(font);
    if (it == fonts_.end() || text.empty()) return 0;

    TTF_Font* ttf = it->second;
    uint8_t alpha = (color.a * globalAlpha_) / 255;
    SDL_Color sdlColor{color.r, color.g, color.b, alpha};

    std::string str(text);
    SDL_Surface* surface = TTF_RenderUTF8_Blended(ttf, str.c_str(), sdlColor);
    if (!surface) return 0;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    int w = surface->w;
    int h = surface->h;
    SDL_FreeSurface(surface);

    if (!texture) return 0;

    SDL_Rect dst{origin.x, origin.y, w, h};
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    return w;
}

Size SDL2Renderer::measureText(std::string_view text, FontHandle font) {
    auto it = fonts_.find(font);
    if (it == fonts_.end() || text.empty()) return {0, 0};

    TTF_Font* ttf = it->second;
    int w = 0, h = 0;
    std::string str(text);
    TTF_SizeUTF8(ttf, str.c_str(), &w, &h);
    return {w, h};
}

void SDL2Renderer::drawTextEllipsis(std::string_view text, Point origin, FontHandle font, Color color, int maxWidth) {
    if (maxWidth <= 0) return;
    Size sz = measureText(text, font);
    if (sz.w <= maxWidth) {
        drawText(text, origin, font, color);
        return;
    }

    std::string str(text);
    std::string ellipsis = "...";
    while (!str.empty()) {
        str.pop_back();
        std::string candidate = str + ellipsis;
        if (measureText(candidate, font).w <= maxWidth) {
            drawText(candidate, origin, font, color);
            return;
        }
    }
    drawText(ellipsis, origin, font, color);
}

TextureHandle SDL2Renderer::loadTexture(std::string_view path) {
    std::string p(path);
    SDL_Surface* surface = SDL_LoadBMP(p.c_str());
    if (!surface) return 0;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (!tex) return 0;

    TextureHandle handle = nextTextureHandle_++;
    textures_[handle] = tex;
    return handle;
}

void SDL2Renderer::freeTexture(TextureHandle h) {
    auto it = textures_.find(h);
    if (it != textures_.end()) {
        SDL_DestroyTexture(it->second);
        textures_.erase(it);
    }
}

Size SDL2Renderer::textureSize(TextureHandle h) {
    auto it = textures_.find(h);
    if (it == textures_.end()) return {0, 0};

    int w = 0, h_out = 0;
    SDL_QueryTexture(it->second, nullptr, nullptr, &w, &h_out);
    return {w, h_out};
}

void SDL2Renderer::drawTexture(TextureHandle h, Rect dst, uint8_t alpha) {
    auto it = textures_.find(h);
    if (it == textures_.end()) return;

    SDL_Texture* tex = it->second;
    SDL_SetTextureAlphaMod(tex, (alpha * globalAlpha_) / 255);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    SDL_Rect r{dst.x, dst.y, dst.w, dst.h};
    SDL_RenderCopy(renderer_, tex, nullptr, &r);
}

void SDL2Renderer::setGlobalAlpha(uint8_t alpha) {
    globalAlpha_ = alpha;
}

Size SDL2Renderer::screenSize() const {
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(renderer_, &w, &h);
    return {w, h};
}

} // namespace hui

#endif
