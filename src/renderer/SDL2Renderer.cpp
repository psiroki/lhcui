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
        SDL_Rect r{clipStack_.back().x, clipStack_.back().y, clipStack_.back().w, clipStack_.back().h};
        SDL_RenderSetClipRect(renderer_, &r);
    }
}

void SDL2Renderer::pushClip(Rect r) {
    if (clipStack_.empty()) {
        clipStack_.push_back(r);
    } else {
        Rect current = clipStack_.back();
        
        int x1 = std::max(current.x, r.x);
        int y1 = std::max(current.y, r.y);
        int x2 = std::min(current.x + current.w, r.x + r.w);
        int y2 = std::min(current.y + current.h, r.y + r.h);
        
        Rect intersection = {x1, y1, std::max(0, x2 - x1), std::max(0, y2 - y1)};
        clipStack_.push_back(intersection);
    }
    applyClip();
}

void SDL2Renderer::popClip() {
    if (!clipStack_.empty()) {
        clipStack_.pop_back();
    }
    applyClip();
}

void SDL2Renderer::fillRect(Rect r, Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, (c.a * globalAlpha_) / 255);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_Rect rect{r.x, r.y, r.w, r.h};
    SDL_RenderFillRect(renderer_, &rect);
}

void SDL2Renderer::drawRect(Rect r, Color c, int thickness) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, (c.a * globalAlpha_) / 255);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    
    // Quick implementation of thick borders: draw multiple rects
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect rect{r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i};
        if (rect.w > 0 && rect.h > 0) {
            SDL_RenderDrawRect(renderer_, &rect);
        }
    }
}

void SDL2Renderer::drawLine(Point a, Point b, Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, (c.a * globalAlpha_) / 255);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_RenderDrawLine(renderer_, a.x, a.y, b.x, b.y);
}

int SDL2Renderer::drawText(std::string_view text, Point origin, FontHandle font, Color color) {
    if (text.empty() || font == 0) return 0;
    
    auto it = fonts_.find(font);
    if (it == fonts_.end()) return 0;
    TTF_Font* ttf = it->second;

    SDL_Color c{color.r, color.g, color.b, static_cast<Uint8>((color.a * globalAlpha_) / 255)};
    
    // In SDL2, TTF_RenderUTF8_Blended expects null-terminated string, so we must copy if needed.
    std::string s(text);
    SDL_Surface* surface = TTF_RenderUTF8_Blended(ttf, s.c_str(), c);
    if (!surface) return 0;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    int width = surface->w;
    int height = surface->h;
    SDL_FreeSurface(surface);
    
    if (texture) {
        SDL_Rect dst{origin.x, origin.y, width, height};
        SDL_RenderCopy(renderer_, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    
    return width;
}

Size SDL2Renderer::measureText(std::string_view text, FontHandle font) {
    if (text.empty() || font == 0) return {0, 0};
    
    auto it = fonts_.find(font);
    if (it == fonts_.end()) return {0, 0};
    TTF_Font* ttf = it->second;
    
    int w = 0, h = 0;
    std::string s(text);
    TTF_SizeUTF8(ttf, s.c_str(), &w, &h);
    return {w, h};
}

void SDL2Renderer::drawTextEllipsis(std::string_view text, Point origin, FontHandle font, Color color, int maxWidth) {
    if (text.empty() || font == 0) return;
    
    Size fullSize = measureText(text, font);
    if (fullSize.w <= maxWidth) {
        drawText(text, origin, font, color);
        return;
    }
    
    std::string current(text);
    const std::string ellipsis = "…";
    
    // Iteratively remove characters from the end
    while (!current.empty()) {
        // Pop last UTF-8 char
        while (!current.empty()) {
            char c = current.back();
            current.pop_back();
            if ((c & 0xC0) != 0x80) { // Not a continuation byte
                break;
            }
        }
        
        std::string test = current + ellipsis;
        Size s = measureText(test, font);
        if (s.w <= maxWidth || current.empty()) {
            drawText(test, origin, font, color);
            return;
        }
    }
}

TextureHandle SDL2Renderer::loadTexture(std::string_view path) {
    // For smoke tests, we won't implement a full image loader unless required,
    // since SDL2_image isn't strictly requested, but we can do a dummy or just load BMP.
    // DESIGN.md says "loadTexture". Let's assume SDL_LoadBMP for basic support without SDL_image.
    std::string s(path);
    SDL_Surface* surface = SDL_LoadBMP(s.c_str());
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

FontHandle SDL2Renderer::registerFont(TTF_Font* font) {
    FontHandle handle = nextFontHandle_++;
    fonts_[handle] = font;
    return handle;
}

} // namespace hui
