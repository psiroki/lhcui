#include "SDL1Renderer.h"

#ifdef HUI_USE_SDL1

#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <string>

namespace hui {

SDL1Renderer::SDL1Renderer(SDL_Surface* screen)
    : screen_(screen) {
}

SDL1Renderer::~SDL1Renderer() {
    for (auto& pair : textures_) {
        SDL_FreeSurface(pair.second);
    }
    for (auto& pair : textCache_) {
        SDL_FreeSurface(pair.second);
    }
}

void SDL1Renderer::beginFrame() {
    clipStack_.clear();
    SDL_SetClipRect(screen_, nullptr);
}

void SDL1Renderer::endFrame() {
    SDL_Flip(screen_);
}

void SDL1Renderer::applyClip() {
    if (clipStack_.empty()) {
        SDL_SetClipRect(screen_, nullptr);
    } else {
        SDL_Rect r{static_cast<Sint16>(clipStack_.back().x), 
                   static_cast<Sint16>(clipStack_.back().y), 
                   static_cast<Uint16>(clipStack_.back().w), 
                   static_cast<Uint16>(clipStack_.back().h)};
        SDL_SetClipRect(screen_, &r);
    }
}

void SDL1Renderer::pushClip(Rect r) {
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

void SDL1Renderer::popClip() {
    if (!clipStack_.empty()) {
        clipStack_.pop_back();
    }
    applyClip();
}

uint32_t SDL1Renderer::mapColor(Color c) {
    return SDL_MapRGBA(screen_->format, c.r, c.g, c.b, (c.a * globalAlpha_) / 255);
}

void SDL1Renderer::fillRect(Rect r, Color c) {
    if (c.a == 0 || globalAlpha_ == 0) return;
    
    SDL_Rect rect{static_cast<Sint16>(r.x), static_cast<Sint16>(r.y), 
                  static_cast<Uint16>(r.w), static_cast<Uint16>(r.h)};
                  
    if (c.a == 255 && globalAlpha_ == 255) {
        SDL_FillRect(screen_, &rect, mapColor(c));
    } else {
        // SDL1 does not support alpha blended FillRect natively without creating a surface.
        // For a true alpha rect, we create a temporary surface, fill it, set alpha, blit, free.
        SDL_Surface* temp = SDL_CreateRGBSurface(SDL_SWSURFACE, rect.w, rect.h, 
                                                 screen_->format->BitsPerPixel,
                                                 screen_->format->Rmask,
                                                 screen_->format->Gmask,
                                                 screen_->format->Bmask,
                                                 screen_->format->Amask);
        if (temp) {
            uint32_t colorKey = SDL_MapRGB(temp->format, c.r, c.g, c.b);
            SDL_FillRect(temp, nullptr, colorKey);
            SDL_SetAlpha(temp, SDL_SRCALPHA, (c.a * globalAlpha_) / 255);
            SDL_BlitSurface(temp, nullptr, screen_, &rect);
            SDL_FreeSurface(temp);
        }
    }
}

void SDL1Renderer::drawRect(Rect r, Color c, int thickness) {
    if (c.a == 0 || globalAlpha_ == 0) return;
    
    // Quick implementation: draw 4 filled rects for the borders
    for (int i = 0; i < thickness; ++i) {
        // Top
        fillRect({r.x + i, r.y + i, r.w - 2 * i, 1}, c);
        // Bottom
        fillRect({r.x + i, r.y + r.h - 1 - i, r.w - 2 * i, 1}, c);
        // Left
        fillRect({r.x + i, r.y + i + 1, 1, r.h - 2 * i - 2}, c);
        // Right
        fillRect({r.x + r.w - 1 - i, r.y + i + 1, 1, r.h - 2 * i - 2}, c);
    }
}

void SDL1Renderer::drawLine(Point a, Point b, Color c) {
    // Basic Bresenham or simple line. SDL1 doesn't have RenderDrawLine.
    // For smoke test, we can just do a very primitive implementation or rely on horizontal/vertical
    if (a.y == b.y) {
        int x1 = std::min(a.x, b.x);
        int x2 = std::max(a.x, b.x);
        fillRect({x1, a.y, x2 - x1 + 1, 1}, c);
    } else if (a.x == b.x) {
        int y1 = std::min(a.y, b.y);
        int y2 = std::max(a.y, b.y);
        fillRect({a.x, y1, 1, y2 - y1 + 1}, c);
    } else {
        // Primitive Bresenham
        int dx = std::abs(b.x - a.x);
        int dy = std::abs(b.y - a.y);
        int sx = a.x < b.x ? 1 : -1;
        int sy = a.y < b.y ? 1 : -1;
        int err = (dx > dy ? dx : -dy) / 2;
        int e2;
        
        int cx = a.x;
        int cy = a.y;
        
        while (true) {
            fillRect({cx, cy, 1, 1}, c);
            if (cx == b.x && cy == b.y) break;
            e2 = err;
            if (e2 > -dx) { err -= dy; cx += sx; }
            if (e2 < dy) { err += dx; cy += sy; }
        }
    }
}

int SDL1Renderer::drawText(std::string_view text, Point origin, FontHandle font, Color color) {
    if (text.empty() || font == 0) return 0;
    
    auto it = fonts_.find(font);
    if (it == fonts_.end()) return 0;
    TTF_Font* ttf = it->second;

    CacheKey key{std::string(text), font, color};
    SDL_Surface* surface = nullptr;
    
    auto cacheIt = textCache_.find(key);
    if (cacheIt != textCache_.end()) {
        surface = cacheIt->second;
    } else {
        SDL_Color c{color.r, color.g, color.b, color.a}; // SDL1 TTF ignores alpha but we pass it
        surface = TTF_RenderUTF8_Blended(ttf, key.text.c_str(), c);
        if (surface) {
            textCache_[key] = surface;
        }
    }
    
    if (!surface) return 0;
    
    int width = surface->w;
    SDL_Rect dst{static_cast<Sint16>(origin.x), static_cast<Sint16>(origin.y), 0, 0};
    
    if (globalAlpha_ < 255 || color.a < 255) {
        SDL_SetAlpha(surface, SDL_SRCALPHA, (color.a * globalAlpha_) / 255);
    } else {
        SDL_SetAlpha(surface, 0, 255);
    }
    
    SDL_BlitSurface(surface, nullptr, screen_, &dst);
    
    return width;
}

Size SDL1Renderer::measureText(std::string_view text, FontHandle font) {
    if (text.empty() || font == 0) return {0, 0};
    
    auto it = fonts_.find(font);
    if (it == fonts_.end()) return {0, 0};
    TTF_Font* ttf = it->second;
    
    int w = 0, h = 0;
    std::string s(text);
    TTF_SizeUTF8(ttf, s.c_str(), &w, &h);
    return {w, h};
}

void SDL1Renderer::drawTextEllipsis(std::string_view text, Point origin, FontHandle font, Color color, int maxWidth) {
    if (text.empty() || font == 0) return;
    
    Size fullSize = measureText(text, font);
    if (fullSize.w <= maxWidth) {
        drawText(text, origin, font, color);
        return;
    }
    
    std::string current(text);
    const std::string ellipsis = "…";
    
    while (!current.empty()) {
        while (!current.empty()) {
            char c = current.back();
            current.pop_back();
            if ((c & 0xC0) != 0x80) {
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

TextureHandle SDL1Renderer::loadTexture(std::string_view path) {
    std::string s(path);
    SDL_Surface* surface = SDL_LoadBMP(s.c_str());
    if (!surface) return 0;
    
    // Convert to display format for faster blitting
    SDL_Surface* optimized = SDL_DisplayFormatAlpha(surface);
    SDL_FreeSurface(surface);
    if (!optimized) return 0;
    
    TextureHandle handle = nextTextureHandle_++;
    textures_[handle] = optimized;
    return handle;
}

void SDL1Renderer::freeTexture(TextureHandle h) {
    auto it = textures_.find(h);
    if (it != textures_.end()) {
        SDL_FreeSurface(it->second);
        textures_.erase(it);
    }
}

Size SDL1Renderer::textureSize(TextureHandle h) {
    auto it = textures_.find(h);
    if (it == textures_.end()) return {0, 0};
    return {it->second->w, it->second->h};
}

void SDL1Renderer::drawTexture(TextureHandle h, Rect dst, uint8_t alpha) {
    auto it = textures_.find(h);
    if (it == textures_.end()) return;
    
    SDL_Surface* surface = it->second;
    SDL_SetAlpha(surface, SDL_SRCALPHA, (alpha * globalAlpha_) / 255);
    
    SDL_Rect r{static_cast<Sint16>(dst.x), static_cast<Sint16>(dst.y), 0, 0};
    SDL_BlitSurface(surface, nullptr, screen_, &r);
}

void SDL1Renderer::setGlobalAlpha(uint8_t alpha) {
    globalAlpha_ = alpha;
}

Size SDL1Renderer::screenSize() const {
    if (!screen_) return {0, 0};
    return {screen_->w, screen_->h};
}

FontHandle SDL1Renderer::registerFont(TTF_Font* font) {
    FontHandle handle = nextFontHandle_++;
    fonts_[handle] = font;
    return handle;
}

void SDL1Renderer::invalidateTextCache() {
    for (auto& pair : textCache_) {
        SDL_FreeSurface(pair.second);
    }
    textCache_.clear();
}

} // namespace hui

#endif // HUI_USE_SDL1
