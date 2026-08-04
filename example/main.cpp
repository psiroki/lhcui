#include "hui/types.h"
#include "hui/IRenderer.h"
#include "hui/Widget.h"
#include "hui/FocusManager.h"
#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/UISystem.h"
#include "hui/Shell.h"

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
static hui::UISystem* g_uiSystem = nullptr;
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

    bool isFocusable() const override { return true; }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        // Draw card background
        hui::Color bg = isFocused() ? theme.focusFillColor : theme.surface;
        r.fillRect(bounds_, bg);

        // Draw border: thick accent border when focused
        if (isFocused()) {
            r.drawRect(bounds_, theme.focusBorderColor, theme.focusBorderWidth);
            // Visual cursor indicator on the left
            r.fillRect({bounds_.x + 4, bounds_.y + 4, 6, bounds_.h - 8}, theme.accent);
        } else {
            r.drawRect(bounds_, theme.surfaceAlt, 1);
        }

        // Draw label text
        hui::Color textColor = isFocused() ? theme.textPrimary : theme.textSecondary;
        int textX = bounds_.x + (isFocused() ? 20 : 16);
        int textY = bounds_.y + (bounds_.h - 16) / 2;
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

    bool dimsBelow() const override { return true; }

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
        hui::Rect modalRect{bounds_.x + 120, bounds_.y + 90, 400, 290};
        int itemY = modalRect.y + 80;
        for (size_t i = 0; i < buttons_.size(); ++i) {
            buttons_[i]->layout({modalRect.x + 20, itemY, modalRect.w - 40, 48});
            itemY += 60;
        }
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
        // Draw modal card window (ViewStack draws the single scrim behind us)
        hui::Rect modalRect{bounds_.x + 120, bounds_.y + 90, 400, 290};
        r.fillRect(modalRect, theme.surface);
        r.drawRect(modalRect, theme.accent, 2);

        // Header
        r.drawText("CONTEXT MENU OVERLAY", {modalRect.x + 20, modalRect.y + 20}, theme.fontBody, theme.textPrimary);
        r.drawText("Base screen behind is dimmed automatically!", {modalRect.x + 20, modalRect.y + 45}, theme.fontSmall, theme.textSecondary);

        // Draw options inside modal
        for (size_t i = 0; i < buttons_.size(); ++i) {
            buttons_[i]->draw(r, theme);
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
            if (g_viewStack) g_viewStack->pop();
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

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
        int itemY = bounds_.y + 80;
        for (size_t i = 0; i < buttons_.size(); ++i) {
            buttons_[i]->layout({bounds_.x + 40, itemY, bounds_.w - 80, 50});
            itemY += 65;
        }
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
        r.fillRect(bounds_, theme.background);

        // Top title bar
        r.fillRect({bounds_.x, bounds_.y, bounds_.w, 50}, theme.surface);
        r.drawText("SETTINGS SCREEN (Full View Pushed)", {bounds_.x + 20, bounds_.y + 16}, theme.fontBody, theme.textPrimary);
        r.drawLine({bounds_.x, bounds_.y + 50}, {bounds_.x + bounds_.w, bounds_.y + 50}, theme.surfaceAlt);

        // Content
        for (size_t i = 0; i < buttons_.size(); ++i) {
            buttons_[i]->draw(r, theme);
        }

        // Instructions
        r.drawText("Press B or select Option 2 to go back / Main Menu. Select Option 3 to Quit.", {bounds_.x + 40, bounds_.y + 380}, theme.fontSmall, theme.textSecondary);
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
                g_viewStack->pop();
            } else if (g_viewStack) {
                g_viewStack->replace(createMainMenuView());
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
        buttons_.push_back(std::make_unique<ButtonWidget>("3. Toggle Suspend Mode (Press A)", []() {
            if (g_uiSystem) {
                g_uiSystem->setSuspended(!g_uiSystem->isSuspended());
                std::cout << "[Demo] UISystem suspended state: " << (g_uiSystem->isSuspended() ? "true" : "false") << "\n";
            }
        }));
        buttons_.push_back(std::make_unique<ButtonWidget>("4. Quit Application (Press A or B)", []() {
            std::cout << "[Main Menu] Quitting application...\n";
            g_running = false;
        }));
    }

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
        int itemY = bounds_.y + 100;
        for (size_t i = 0; i < buttons_.size(); ++i) {
            buttons_[i]->layout({bounds_.x + 40, itemY, bounds_.w - 80, 48});
            itemY += 56;
        }
    }

    void onPush() override {
        focusIndex_ = 0;
    }

    void update(float dt, hui::FocusManager& fm) override {
        (void)dt;
        if (!buttons_.empty() && focusIndex_ >= 0 && focusIndex_ < static_cast<int>(buttons_.size())) {
            if (!fm.focused() || !buttons_[focusIndex_]->isFocused()) {
                fm.setFocus(buttons_[focusIndex_].get());
            }
        }
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        r.fillRect(bounds_, theme.background);

        // Header Bar
        r.fillRect({bounds_.x, bounds_.y, bounds_.w, 45}, theme.surface);
        r.drawText("MAIN MENU (Base Screen)", {bounds_.x + 20, bounds_.y + 14}, theme.fontBody, theme.textPrimary);
        r.drawText("Phase 6 & 7 Input/UISystem Demo", {bounds_.x + 350, bounds_.y + 16}, theme.fontSmall, theme.accent);
        r.drawLine({bounds_.x, bounds_.y + 45}, {bounds_.x + bounds_.w, bounds_.y + 45}, theme.surfaceAlt);

        // Status Card
        hui::Rect statusRect{bounds_.x + 40, bounds_.y + 55, bounds_.w - 80, 35};
        r.fillRect(statusRect, theme.surfaceAlt);
        r.drawText("Status: KeyRepeatDriver & ChordDetector Active",
                   {bounds_.x + 55, bounds_.y + 65}, theme.fontSmall, theme.success);

        // Menu Buttons
        for (size_t i = 0; i < buttons_.size(); ++i) {
            buttons_[i]->draw(r, theme);
        }

        // Bottom Controls Hint Box
        hui::Rect hintRect{bounds_.x + 40, bounds_.y + 335, bounds_.w - 80, 115};
        r.fillRect(hintRect, theme.surface);
        r.drawRect(hintRect, theme.surfaceAlt, 1);
        r.drawText("Controls & Features:", {bounds_.x + 55, bounds_.y + 345}, theme.fontBody, theme.textPrimary);
        r.drawText("Hold Up / Down: KeyRepeatDriver auto-repeats navigation", {bounds_.x + 55, bounds_.y + 368}, theme.fontSmall, theme.textSecondary);
        r.drawText("Press [1] + [2] simultaneously: Fires Start+Select -> Guide Chord", {bounds_.x + 55, bounds_.y + 388}, theme.fontSmall, theme.textSecondary);
        r.drawText("Space / Enter / Z / Button A: Select  |  Esc / X / Button B: Quit", {bounds_.x + 55, bounds_.y + 408}, theme.fontSmall, theme.textSecondary);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager& fm) override {
        if (b == hui::Button::Guide) {
            std::cout << "[Chord Triggered!] Start+Select -> Guide chord fired!\n";
            if (g_viewStack) {
                g_viewStack->push(createContextMenuOverlayView());
            }
            return true;
        }
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
            {"Up/Down", "Navigate (Held Repeat)", false, 3},
            {"1+2", "Start+Select Chord -> Guide", false, 4}
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
    SDL_Window* window = SDL_CreateWindow("LHCUI Interactive Demo (Phase 6 & 7)",
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

    // Instantiate UISystem (owns ViewStack, FocusManager, KeyRepeatDriver, ChordDetector)
    hui::UISystem uiSystem(*renderer, theme);
    example::g_viewStack = &uiSystem.viewStack();
    example::g_uiSystem = &uiSystem;
    example::g_running = true;

    // Push initial Base Screen
    uiSystem.viewStack().push(example::createMainMenuView());

    uint64_t lastTime = SDL_GetTicks();

    while (example::g_running) {
        uint64_t now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                example::g_running = false;
            } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                // Wake up from suspend mode on key press
                if (uiSystem.isSuspended() && e.type == SDL_KEYDOWN) {
                    uiSystem.setSuspended(false);
                    std::cout << "[Demo] Application woke up from suspend mode on keypress!\n";
                }

                hui::Button mappedButton = hui::Button::COUNT;
                switch (e.key.keysym.sym) {
                    case SDLK_UP:     mappedButton = hui::Button::Up; break;
                    case SDLK_DOWN:   mappedButton = hui::Button::Down; break;
                    case SDLK_LEFT:   mappedButton = hui::Button::Left; break;
                    case SDLK_RIGHT:  mappedButton = hui::Button::Right; break;
                    case SDLK_RETURN: case SDLK_SPACE: case SDLK_z: mappedButton = hui::Button::A; break;
                    case SDLK_ESCAPE: case SDLK_x: mappedButton = hui::Button::B; break;
                    case SDLK_1:      mappedButton = hui::Button::Start; break;
                    case SDLK_2:      mappedButton = hui::Button::Select; break;
                    case SDLK_g:      mappedButton = hui::Button::Guide; break;
                    default: break;
                }
                if (mappedButton != hui::Button::COUNT) {
                    if (e.type == SDL_KEYDOWN) {
                        uiSystem.onButtonDown(mappedButton);
                    } else {
                        uiSystem.onButtonUp(mappedButton);
                    }
                }
            }
        }

        // Drive update phase via UISystem (handles dt clamping, key repeat, chord timers, and view updates)
        uiSystem.update(dt);

        // Render frame via UISystem
        renderer->beginFrame();
        uiSystem.draw();
        renderer->endFrame();

        if (uiSystem.isSuspended()) {
            SDL_Delay(100);
        } else {
            SDL_Delay(16);
        }
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
