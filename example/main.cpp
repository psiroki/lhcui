#include "hui/types.h"
#include "hui/IRenderer.h"
#include "hui/Widget.h"
#include "hui/FocusManager.h"
#include "hui/View.h"
#include "hui/ViewStack.h"

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
#include <functional>

namespace example {

// Global references for demo loop and ViewStack
static hui::ViewStack* g_viewStack = nullptr;
static bool g_running = true;

// ---------------------------------------------------------------------------
// Custom Interactive Button Widget with prominent focus styling
// ---------------------------------------------------------------------------
class ButtonWidget : public hui::Widget {
public:
    std::string label;
    std::function<void()> onClick;

    ButtonWidget(std::string text, std::function<void()> callback = nullptr)
        : label(std::move(text)), onClick(std::move(callback)) {}

    void draw(hui::IRenderer& r, hui::Rect bounds, const hui::Theme& theme) override {
        // Draw card background
        hui::Color bg = isFocused() ? theme.focusFillColor : theme.surface;
        r.fillRect(bounds, bg);

        // Draw border: thick accent border when focused
        if (isFocused()) {
            r.drawRect(bounds, theme.focusBorderColor, theme.focusBorderWidth);
            // Visual cursor indicator on the left
            r.fillRect({bounds.x + 4, bounds.y + 4, 6, bounds.h - 8}, theme.accent);
        } else {
            r.drawRect(bounds, theme.surfaceAlt, 1);
        }

        // Draw label text
        hui::Color textColor = isFocused() ? theme.textPrimary : theme.textSecondary;
        int textX = bounds.x + (isFocused() ? 20 : 16);
        int textY = bounds.y + (bounds.h - 16) / 2;
        r.drawText(label, {textX, textY}, theme.fontBody, textColor);
    }

    bool onButtonDown(hui::Button b) override {
        if (b == hui::Button::A) {
            if (onClick) onClick();
            return true;
        }
        return false;
    }
};

// Forward declaration of factory functions
std::unique_ptr<hui::View> createMainMenuView();
std::unique_ptr<hui::View> createContextMenuOverlayView();
std::unique_ptr<hui::View> createSettingsView();

// ---------------------------------------------------------------------------
// 1. ContextMenuOverlayView (Modal Overlay)
// ---------------------------------------------------------------------------
class ContextMenuOverlayView : public hui::View {
public:
    HUI_VIEW_TYPE(ContextMenuOverlayView)

    ContextMenuOverlayView() {
        buttons_.push_back(std::make_unique<ButtonWidget>("1. Confirm Action (Press A)", []() {
            std::cout << "[Overlay] Action Confirmed!\n";
            if (g_viewStack) g_viewStack->pop();
        }));
        buttons_.push_back(std::make_unique<ButtonWidget>("2. Cancel / Close Overlay (Press A or B)", []() {
            std::cout << "[Overlay] Dismissed!\n";
            if (g_viewStack) g_viewStack->pop();
        }));
    }

    void onPush() override {
        focusIndex_ = 0;
    }

    void update(float dt, hui::FocusManager& fm) override {
        (void)dt;
        if (!buttons_.empty() && focusIndex_ >= 0 && focusIndex_ < static_cast<int>(buttons_.size())) {
            if (!buttons_[focusIndex_]->isFocused()) {
                fm.setFocus(buttons_[focusIndex_].get());
            }
        }
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        // Fill full screen with dark overlay background
        r.fillRect({0, 0, 640, 480}, theme.overlay);

        // Draw modal card window
        hui::Rect modalRect{120, 90, 400, 290};
        r.fillRect(modalRect, theme.surface);
        r.drawRect(modalRect, theme.accent, 2);

        // Header
        r.drawText("CONTEXT MENU OVERLAY", {modalRect.x + 20, modalRect.y + 20}, theme.fontBody, theme.textPrimary);
        r.drawText("Base screen behind is dimmed automatically!", {modalRect.x + 20, modalRect.y + 45}, theme.fontSmall, theme.textSecondary);

        // Draw options inside modal
        int itemY = modalRect.y + 80;
        for (size_t i = 0; i < buttons_.size(); ++i) {
            hui::Rect itemRect{modalRect.x + 20, itemY, modalRect.w - 40, 48};
            buttons_[i]->draw(r, itemRect, theme);
            itemY += 60;
        }

        // Footer hint
        r.drawText("[Up/Down] Focus  |  [A] Select  |  [B] Close", {modalRect.x + 20, modalRect.y + 250}, theme.fontSmall, theme.warning);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager& fm) override {
        if (b == hui::Button::Up) {
            if (focusIndex_ > 0) {
                --focusIndex_;
                fm.setFocus(buttons_[focusIndex_].get());
            }
            return true;
        }
        if (b == hui::Button::Down) {
            if (focusIndex_ < static_cast<int>(buttons_.size()) - 1) {
                ++focusIndex_;
                fm.setFocus(buttons_[focusIndex_].get());
            }
            return true;
        }
        if (b == hui::Button::B) {
            if (g_viewStack) g_viewStack->pop(&fm);
            return true;
        }
        if (focusIndex_ >= 0 && focusIndex_ < static_cast<int>(buttons_.size())) {
            return buttons_[focusIndex_]->onButtonDown(b);
        }
        return false;
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {
            {"A", "Select", false, 1},
            {"B", "Close Overlay", false, 2}
        };
    }

private:
    std::vector<std::unique_ptr<ButtonWidget>> buttons_;
    int focusIndex_ = 0;
};

// ---------------------------------------------------------------------------
// 2. SettingsView (Full Screen Pushed View)
// ---------------------------------------------------------------------------
class SettingsView : public hui::View {
public:
    HUI_VIEW_TYPE(SettingsView)

    SettingsView() {
        buttons_.push_back(std::make_unique<ButtonWidget>("1. Toggle Option (Press A)", []() {
            std::cout << "[Settings] Option Toggled!\n";
        }));
        buttons_.push_back(std::make_unique<ButtonWidget>("2. Back to Main Menu (Press A or B)", []() {
            if (g_viewStack && g_viewStack->size() > 1) {
                g_viewStack->pop();
            } else if (g_viewStack) {
                g_viewStack->replace(createMainMenuView());
            }
        }));
        buttons_.push_back(std::make_unique<ButtonWidget>("3. Quit Application (Press A)", []() {
            std::cout << "[Settings] Quitting application...\n";
            g_running = false;
        }));
    }

    void onPush() override {
        focusIndex_ = 0;
    }

    void update(float dt, hui::FocusManager& fm) override {
        (void)dt;
        if (!buttons_.empty() && focusIndex_ >= 0 && focusIndex_ < static_cast<int>(buttons_.size())) {
            if (!buttons_[focusIndex_]->isFocused()) {
                fm.setFocus(buttons_[focusIndex_].get());
            }
        }
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        r.fillRect({0, 0, 640, 480}, theme.background);

        // Top title bar
        r.fillRect({0, 0, 640, 50}, theme.surface);
        r.drawText("SETTINGS SCREEN (Full View Pushed)", {20, 16}, theme.fontBody, theme.textPrimary);
        r.drawLine({0, 50}, {640, 50}, theme.surfaceAlt);

        // Content
        int itemY = 80;
        for (size_t i = 0; i < buttons_.size(); ++i) {
            hui::Rect itemRect{40, itemY, 560, 50};
            buttons_[i]->draw(r, itemRect, theme);
            itemY += 65;
        }

        // Instructions
        r.drawText("Press B or select Option 2 to go back / Main Menu. Select Option 3 to Quit.", {40, 380}, theme.fontSmall, theme.textSecondary);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager& fm) override {
        if (b == hui::Button::Up) {
            if (focusIndex_ > 0) {
                --focusIndex_;
                fm.setFocus(buttons_[focusIndex_].get());
            }
            return true;
        }
        if (b == hui::Button::Down) {
            if (focusIndex_ < static_cast<int>(buttons_.size()) - 1) {
                ++focusIndex_;
                fm.setFocus(buttons_[focusIndex_].get());
            }
            return true;
        }
        if (b == hui::Button::B) {
            if (g_viewStack && g_viewStack->size() > 1) {
                g_viewStack->pop(&fm);
            } else if (g_viewStack) {
                g_viewStack->replace(createMainMenuView(), &fm);
            }
            return true;
        }
        if (focusIndex_ >= 0 && focusIndex_ < static_cast<int>(buttons_.size())) {
            return buttons_[focusIndex_]->onButtonDown(b);
        }
        return false;
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {
            {"A", "Select", false, 1},
            {"B", "Back", false, 2}
        };
    }

private:
    std::vector<std::unique_ptr<ButtonWidget>> buttons_;
    int focusIndex_ = 0;
};

// ---------------------------------------------------------------------------
// 3. MainMenuView (Base Screen)
// ---------------------------------------------------------------------------
class MainMenuView : public hui::View {
public:
    HUI_VIEW_TYPE(MainMenuView)

    MainMenuView() {
        buttons_.push_back(std::make_unique<ButtonWidget>("1. Open Context Menu Overlay (Press A)", []() {
            if (g_viewStack) {
                g_viewStack->push(createContextMenuOverlayView());
            }
        }));
        buttons_.push_back(std::make_unique<ButtonWidget>("2. Push Settings View (Press A)", []() {
            if (g_viewStack) {
                g_viewStack->push(createSettingsView());
            }
        }));
        buttons_.push_back(std::make_unique<ButtonWidget>("3. Replace with Settings View (Press A)", []() {
            if (g_viewStack) {
                g_viewStack->replace(createSettingsView());
            }
        }));
        buttons_.push_back(std::make_unique<ButtonWidget>("4. Quit Application (Press A or B)", []() {
            std::cout << "[Main Menu] Quitting application...\n";
            g_running = false;
        }));
    }

    void onPush() override {
        focusIndex_ = 0;
    }

    void update(float dt, hui::FocusManager& fm) override {
        (void)dt;
        if (!buttons_.empty() && focusIndex_ >= 0 && focusIndex_ < static_cast<int>(buttons_.size())) {
            if (!fm.focused() || !buttons_[focusIndex_]->isFocused()) {
                if (!isDimmed()) {
                    fm.setFocus(buttons_[focusIndex_].get());
                }
            }
        }
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        r.fillRect({0, 0, 640, 480}, theme.background);

        // Header Bar
        r.fillRect({0, 0, 640, 45}, theme.surface);
        r.drawText("MAIN MENU (Base Screen)", {20, 14}, theme.fontBody, theme.textPrimary);
        r.drawText("Phase 5 View Stack Demo", {380, 16}, theme.fontSmall, theme.accent);
        r.drawLine({0, 45}, {640, 45}, theme.surfaceAlt);

        // Status Card
        hui::Rect statusRect{40, 55, 560, 35};
        r.fillRect(statusRect, theme.surfaceAlt);
        r.drawText(isDimmed() ? "Status: Obscured / Dimmed (Overlay active)" : "Status: Active / Focused Screen",
                   {55, 65}, theme.fontSmall, isDimmed() ? theme.warning : theme.success);

        // Menu Buttons
        int itemY = 100;
        for (size_t i = 0; i < buttons_.size(); ++i) {
            hui::Rect itemRect{40, itemY, 560, 48};
            buttons_[i]->draw(r, itemRect, theme);
            itemY += 56;
        }

        // Bottom Controls Hint Box
        hui::Rect hintRect{40, 335, 560, 115};
        r.fillRect(hintRect, theme.surface);
        r.drawRect(hintRect, theme.surfaceAlt, 1);
        r.drawText("Controls:", {55, 345}, theme.fontBody, theme.textPrimary);
        r.drawText("W / S / Up / Down: Move Focus", {55, 368}, theme.fontSmall, theme.textSecondary);
        r.drawText("Space / Enter / Z / Button A: Select Focused Item", {55, 388}, theme.fontSmall, theme.textSecondary);
        r.drawText("Escape / X / Button B: Quit / Back", {55, 408}, theme.fontSmall, theme.textSecondary);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager& fm) override {
        if (b == hui::Button::Up) {
            if (focusIndex_ > 0) {
                --focusIndex_;
                fm.setFocus(buttons_[focusIndex_].get());
            }
            return true;
        }
        if (b == hui::Button::Down) {
            if (focusIndex_ < static_cast<int>(buttons_.size()) - 1) {
                ++focusIndex_;
                fm.setFocus(buttons_[focusIndex_].get());
            }
            return true;
        }
        if (b == hui::Button::B) {
            g_running = false;
            return true;
        }
        if (focusIndex_ >= 0 && focusIndex_ < static_cast<int>(buttons_.size())) {
            return buttons_[focusIndex_]->onButtonDown(b);
        }
        return false;
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {
            {"A", "Select", false, 1},
            {"B", "Quit", false, 2},
            {"Up/Down", "Navigate", false, 3}
        };
    }

private:
    std::vector<std::unique_ptr<ButtonWidget>> buttons_;
    int focusIndex_ = 0;
};

// Factory implementations
std::unique_ptr<hui::View> createMainMenuView() {
    return std::make_unique<MainMenuView>();
}

std::unique_ptr<hui::View> createContextMenuOverlayView() {
    return std::make_unique<ContextMenuOverlayView>();
}

std::unique_ptr<hui::View> createSettingsView() {
    return std::make_unique<SettingsView>();
}

} // namespace example

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
    SDL_Window* window = SDL_CreateWindow("LHCUI Phase 5 Interactive Demo",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          640, 480, SDL_WINDOW_SHOWN);
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
        std::cerr << "Notice: Font ../assets/Roboto-Regular.ttf not found, default rendering used.\n";
    }

    hui::FontHandle fontHandle = 0;
    if (rawFont) {
#ifdef HUI_USE_SDL1
        fontHandle = static_cast<hui::SDL1Renderer*>(renderer.get())->registerFont(rawFont);
#else
        fontHandle = static_cast<hui::SDL2Renderer*>(renderer.get())->registerFont(rawFont);
#endif
    }

    // Theme configuration
    hui::Theme theme{};
    theme.background       = {20, 22, 28, 255};
    theme.surface          = {35, 38, 48, 255};
    theme.surfaceAlt       = {45, 50, 65, 255};
    theme.accent           = {80, 160, 240, 255};
    theme.textPrimary      = {245, 245, 250, 255};
    theme.textSecondary    = {160, 165, 180, 255};
    theme.textDisabled     = {100, 105, 120, 255};
    theme.warning          = {240, 90, 90, 255};
    theme.success          = {90, 220, 120, 255};
    theme.overlay          = {0, 0, 0, 180};
    theme.focusBorderColor = {100, 180, 255, 255};
    theme.focusBorderWidth = 2;
    theme.focusFillColor   = {40, 60, 90, 255};
    theme.fontBody         = fontHandle;
    theme.fontSmall        = fontHandle;

    // FocusManager and ViewStack (bound together)
    hui::FocusManager focusManager;
    hui::ViewStack viewStack(&focusManager);
    example::g_viewStack = &viewStack;
    example::g_running = true;

    // Push initial Base Screen
    viewStack.push(example::createMainMenuView(), &focusManager);

    uint64_t lastTime = SDL_GetTicks();

    while (example::g_running) {
        uint64_t now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                example::g_running = false;
            } else if (e.type == SDL_KEYDOWN) {
                hui::Button mappedButton = hui::Button::COUNT;
                switch (e.key.keysym.sym) {
                    case SDLK_UP:     case SDLK_w: mappedButton = hui::Button::Up; break;
                    case SDLK_DOWN:   case SDLK_s: mappedButton = hui::Button::Down; break;
                    case SDLK_LEFT:   case SDLK_a: mappedButton = hui::Button::Left; break;
                    case SDLK_RIGHT:  case SDLK_d: mappedButton = hui::Button::Right; break;
                    case SDLK_RETURN: case SDLK_SPACE: case SDLK_z: mappedButton = hui::Button::A; break;
                    case SDLK_ESCAPE: case SDLK_x: mappedButton = hui::Button::B; break;
                    default: break;
                }
                if (mappedButton != hui::Button::COUNT) {
                    viewStack.dispatchButtonDown(mappedButton, focusManager);
                }
            }
        }

        // Drive update on all views
        viewStack.update(dt, focusManager);

        // Render frame
        renderer->beginFrame();
        viewStack.draw(*renderer, theme);
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
