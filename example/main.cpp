#include "hui/types.h"
#include "hui/IRenderer.h"

#ifdef HUI_USE_SDL1
#include <SDL.h>
#include <SDL_ttf.h>
#include "../src/renderer/SDL1Renderer.h"
#else
#include <SDL.h>
#include <SDL_ttf.h>
#include "../src/renderer/SDL2Renderer.h"
#endif

#include <iostream>
#include <memory>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    std::unique_ptr<hui::IRenderer> renderer;

#ifdef HUI_USE_SDL1
    SDL_Surface* screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE);
    if (!screen) {
        std::cerr << "SDL_SetVideoMode failed: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    renderer = std::make_unique<hui::SDL1Renderer>(screen);
#else
    SDL_Window* window = SDL_CreateWindow("LHCUI QA Test (Phase 3)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    renderer = std::make_unique<hui::SDL2Renderer>(sdlRenderer);
#endif

    // Load font
    TTF_Font* rawFont = TTF_OpenFont("../assets/Roboto-Regular.ttf", 16);
    if (!rawFont) {
        std::cerr << "Failed to load font. Make sure assets/Roboto-Regular.ttf exists.\n";
    }

    hui::FontHandle fontHandle = 0;
    if (rawFont) {
#ifdef HUI_USE_SDL1
        fontHandle = static_cast<hui::SDL1Renderer*>(renderer.get())->registerFont(rawFont);
#else
        fontHandle = static_cast<hui::SDL2Renderer*>(renderer.get())->registerFont(rawFont);
#endif
    }

    // QA Test: Test loading a non-existent texture
    hui::TextureHandle badTex = renderer->loadTexture("does_not_exist.bmp");
    if (badTex != 0) {
        std::cerr << "QA Warning: Non-existent texture should return 0, got " << badTex << "\n";
    }

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
        }
        
        renderer->beginFrame();

        // Background
        renderer->fillRect({0, 0, 640, 480}, hui::Color{30, 30, 30, 255});

        hui::Color white = hui::Color::white();
        hui::Color red = hui::Color{255, 50, 50, 255};
        hui::Color green = hui::Color{50, 255, 50, 255};
        hui::Color blue = hui::Color{50, 50, 255, 255};
        
        // 1. Basic Primitives
        renderer->drawText("1. Primitives: Fill, Border, Line", {20, 20}, fontHandle, white);
        renderer->fillRect({20, 50, 50, 50}, red);
        renderer->drawRect({90, 50, 50, 50}, green, 2);
        renderer->drawLine({160, 50}, {210, 100}, blue);

        // 2. Clipping
        renderer->drawText("2. Clipping (should not spill out of blue boxes)", {20, 120}, fontHandle, white);
        
        // Simple clip
        renderer->drawRect({20, 150, 100, 50}, blue, 1);
        renderer->pushClip({20, 150, 100, 50});
        renderer->fillRect({0, 130, 200, 90}, hui::Color{255, 0, 0, 128}); // Red box that exceeds clip
        renderer->popClip();

        // Nested clip
        renderer->drawRect({140, 150, 100, 50}, blue, 1); // outer
        renderer->pushClip({140, 150, 100, 50});
        renderer->pushClip({160, 160, 60, 30});           // inner
        renderer->fillRect({140, 150, 100, 50}, green);   // Fill inner
        renderer->popClip();
        // After inner pop, we should still be confined to outer
        renderer->fillRect({140, 150, 10, 50}, hui::Color{255, 255, 0, 255}); // Left side of outer
        renderer->popClip();

        // 3. Text & Ellipsis
        renderer->drawText("3. Text & Ellipsis (Max width 150)", {20, 220}, fontHandle, white);
        renderer->drawRect({20, 250, 150, 20}, hui::Color{100, 100, 100, 255}, 1);
        renderer->drawTextEllipsis("Short string", {20, 250}, fontHandle, white, 150);
        
        renderer->drawRect({20, 280, 150, 20}, hui::Color{100, 100, 100, 255}, 1);
        renderer->drawTextEllipsis("This is a very long string that should be truncated.", {20, 280}, fontHandle, white, 150);

        // UTF-8 test
        renderer->drawRect({20, 310, 150, 20}, hui::Color{100, 100, 100, 255}, 1);
        renderer->drawTextEllipsis("UTF-8: é, ö, 中文, 1234567890", {20, 310}, fontHandle, white, 150);

        // 4. Global Alpha
        renderer->drawText("4. Global Alpha (0 = invisible, 128 = dim, 255 = normal)", {280, 20}, fontHandle, white);
        
        renderer->setGlobalAlpha(0);
        renderer->fillRect({280, 50, 50, 50}, white); // Should not be seen
        
        renderer->setGlobalAlpha(128);
        renderer->fillRect({340, 50, 50, 50}, white); // Dimmed
        renderer->drawText("Dim", {350, 65}, fontHandle, hui::Color::black());
        
        renderer->setGlobalAlpha(255);
        renderer->fillRect({400, 50, 50, 50}, white); // Normal
        renderer->drawText("Full", {410, 65}, fontHandle, hui::Color::black());
        
        // 5. Texture loading
        renderer->drawText("5. Texture: (Non-existent texture test)", {280, 120}, fontHandle, white);
        std::string texResult = badTex == 0 ? "OK (bad path -> handle 0)" : "FAIL";
        renderer->drawText(texResult, {280, 150}, fontHandle, badTex == 0 ? green : red);

        renderer->endFrame();
        
        SDL_Delay(16);
    }

    if (rawFont) {
        TTF_CloseFont(rawFont);
    }

#ifndef HUI_USE_SDL1
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
#endif

    renderer.reset();
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}
