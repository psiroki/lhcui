#include "hui/types.h"
#include "hui/IRenderer.h"
#include "hui/Widget.h"
#include "hui/FocusManager.h"
#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/UISystem.h"
#include "hui/ListSource.h"

// Phase 10 Atoms
#include "hui/ListItemWidget.h"
#include "hui/GridCellWidget.h"
#include "hui/ProgressBar.h"
#include "hui/Slider.h"
#include "hui/SortModeIndicator.h"
#include "hui/ShuffleToggle.h"
#include "hui/RepeatModeToggle.h"

// Phase 11 Molecules
#include "hui/ListHeaderWidget.h"
#include "hui/SeekableProgressBar.h"
#include "hui/PlaybackControlsRow.h"
#include "hui/HintBarWidget.h"
#include "hui/StatusBarWidget.h"
#include "hui/ToastNotification.h"

#ifdef HUI_ENABLE_KEYBOARD_FALLBACK
#include "hui/sdl/KeyboardFallback.h"
#endif

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
#include <algorithm>

namespace example {

static hui::ViewStack* g_viewStack = nullptr;
static hui::ToastNotification* g_toast = nullptr;
static bool g_running = true;
static int g_toastCounter = 1;

// ---------------------------------------------------------------------------
// Context Menu Overlay View (for verifying hint bar changes & modal dimming)
// ---------------------------------------------------------------------------
class DemoModalView : public hui::View {
public:
    HUI_VIEW_TYPE(DemoModalView)

    DemoModalView() {
        options_ = {"1. Trigger Quick Toast", "2. Resume Playback", "3. Close Modal (Press B)"};
    }

    bool dimsBelow() const override { return true; }

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        hui::Rect modalRect{bounds_.x + (bounds_.w - 380) / 2, bounds_.y + (bounds_.h - 220) / 2, 380, 220};
        r.fillRect(modalRect, theme.surface);
        r.drawRect(modalRect, theme.accent, 2);

        r.drawText("CONTEXT OPTIONS MODAL", {modalRect.x + 20, modalRect.y + 18}, theme.fontBody, theme.textPrimary);
        r.drawText("Overlay active - Notice hint bar updated!", {modalRect.x + 20, modalRect.y + 40}, theme.fontSmall, theme.accent);
        r.drawLine({modalRect.x, modalRect.y + 60}, {modalRect.x + modalRect.w, modalRect.y + 60}, theme.surfaceAlt);

        int optY = modalRect.y + 75;
        for (int i = 0; i < static_cast<int>(options_.size()); ++i) {
            bool focused = (i == focusIndex_);
            hui::Rect rowRect{modalRect.x + 16, optY, modalRect.w - 32, 34};
            r.fillRect(rowRect, focused ? theme.focusFillColor : theme.surfaceAlt);
            if (focused) {
                r.drawRect(rowRect, theme.focusBorderColor, theme.focusBorderWidth);
            }
            r.drawText(options_[i], {rowRect.x + 12, rowRect.y + 8}, theme.fontSmall, focused ? theme.textPrimary : theme.textSecondary);
            optY += 40;
        }
    }

    bool onButtonDown(hui::Button b, hui::FocusManager&) override {
        if (b == hui::Button::Up) {
            if (focusIndex_ > 0) --focusIndex_;
            return true;
        }
        if (b == hui::Button::Down) {
            if (focusIndex_ < static_cast<int>(options_.size()) - 1) ++focusIndex_;
            return true;
        }
        if (b == hui::Button::A) {
            if (focusIndex_ == 0 && g_toast) {
                g_toast->show("Toast fired from modal!", 2.0f);
            }
            if (g_viewStack) g_viewStack->pop();
            return true;
        }
        if (b == hui::Button::B) {
            if (g_viewStack) g_viewStack->pop();
            return true;
        }
        return false;
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {
            {"A", "Select Action", false, 1},
            {"B", "Dismiss Modal", false, 10}
        };
    }

private:
    std::vector<std::string> options_;
    int focusIndex_ = 0;
};

// ---------------------------------------------------------------------------
// Interactive Main Showcase View
// ---------------------------------------------------------------------------
class MoleculeShowcaseView : public hui::View {
public:
    HUI_VIEW_TYPE(MoleculeShowcaseView)

    MoleculeShowcaseView() {
        // Setup ListHeaderWidget
        listHeader_.setLabel("/home/user/music/rock/progressive/dream_theater/scenes_from_a_memory/05_strange_deja_vu.flac");
        listHeader_.setItemCount(12);
        listHeader_.setSortBadge("Track #");

        // Setup SeekableProgressBar
        seekableProgress_.setTime(145.0f, 312.0f);
        seekableProgress_.setProgress(145.0f / 312.0f);
        seekableProgress_.setOnSeek([this](int direction) {
            currentTime_ = std::clamp(currentTime_ + direction * 10.0f, 0.0f, totalTime_);
            seekableProgress_.setTime(currentTime_, totalTime_);
            seekableProgress_.setProgress(currentTime_ / totalTime_);
            if (g_toast) {
                g_toast->show(direction < 0 ? "<< Seek -10s" : ">> Seek +10s", 1.2f);
            }
        });

        // Setup Volume Slider
        volumeSlider_.setLabel("Master Volume");
        volumeSlider_.setRange(0, 100, 5);
        volumeSlider_.setValue(75);
        volumeSlider_.setOnValueChanged([](int val) {
            if (g_toast) {
                g_toast->show("Volume: " + std::to_string(val) + "%", 1.0f);
            }
        });

        // Setup PlaybackControlsRow
        playbackControls_.setPlaybackState(playbackState_);

        // Setup Toggles
        sortIndicator_.setMode("Track #");
        shuffleToggle_.setShuffle(true);
        repeatModeToggle_.setMode(hui::RepeatMode::All);

        // Setup Grid Stamps
        gridCell1_.setCell("Metropolis Pt. 2", "1999", 0, false, false);
        gridCell2_.setCell("Images and Words", "1992", 0, true, false);
        gridCell3_.setCell("Octavarium", "2005", 0, false, false);
    }

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
        int y = bounds_.y + 6;

        // 1. ListHeaderWidget pinned at top
        listHeader_.layout({bounds_.x + 10, y, bounds_.w - 20, 26});
        y += 32;

        // 2. Interactive SeekableProgressBar
        seekableProgress_.layout({bounds_.x + 10, y, bounds_.w - 20, 28});
        y += 34;

        // 3. Interactive Volume Slider
        volumeSlider_.layout({bounds_.x + 10, y, bounds_.w - 20, 28});
        y += 34;

        // 4. Playback Controls Row & Toggles container
        playbackRowBounds_ = {bounds_.x + 10, y, bounds_.w - 20, 36};
        playbackControls_.layout({playbackRowBounds_.x + 80, playbackRowBounds_.y, playbackRowBounds_.w - 280, 36});
        shuffleToggle_.layout({playbackRowBounds_.x + playbackRowBounds_.w - 180, playbackRowBounds_.y + 6, 24, 24});
        repeatModeToggle_.layout({playbackRowBounds_.x + playbackRowBounds_.w - 140, playbackRowBounds_.y + 6, 24, 24});
        sortIndicator_.layout({playbackRowBounds_.x + playbackRowBounds_.w - 100, playbackRowBounds_.y + 6, 90, 24});
        y += 42;

        // 5. Toast Trigger & Modal Trigger action buttons
        toastActionBounds_ = {bounds_.x + 10, y, (bounds_.w - 26) / 2, 32};
        modalActionBounds_ = {bounds_.x + 16 + (bounds_.w - 26) / 2, y, (bounds_.w - 26) / 2, 32};
        y += 38;

        // 6. Stamp Previews (List Items & Grid Cells)
        stampSectionY_ = y;
    }

    void update(float dt, hui::FocusManager& fm) override {
        (void)dt;
        // Sync focus with focusIndex_
        if (focusIndex_ == 0 && fm.focused() != &seekableProgress_) {
            fm.setFocus(&seekableProgress_);
        } else if (focusIndex_ == 1 && fm.focused() != &volumeSlider_) {
            fm.setFocus(&volumeSlider_);
        } else if (focusIndex_ >= 2 && fm.focused() != nullptr) {
            fm.setFocus(nullptr);
        }
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        // Content background
        r.fillRect(bounds_, theme.background);

        // 1. Draw ListHeaderWidget
        listHeader_.draw(r, theme);

        // 2. Draw SeekableProgressBar
        seekableProgress_.draw(r, theme);

        // 3. Draw Volume Slider
        volumeSlider_.draw(r, theme);

        // 4. Draw Playback Controls & Toggles section
        bool isPlaybackFocused = (focusIndex_ == 2);
        r.fillRect(playbackRowBounds_, isPlaybackFocused ? theme.focusFillColor : theme.surface);
        if (isPlaybackFocused) {
            r.drawRect(playbackRowBounds_, theme.focusBorderColor, theme.focusBorderWidth);
        } else {
            r.drawRect(playbackRowBounds_, theme.surfaceAlt, 1);
        }
        r.drawText("Transport:", {playbackRowBounds_.x + 10, playbackRowBounds_.y + 10}, theme.fontSmall, theme.textSecondary);
        playbackControls_.draw(r, theme);
        shuffleToggle_.draw(r, theme);
        repeatModeToggle_.draw(r, theme);
        sortIndicator_.draw(r, theme);

        // 5. Action Buttons (Toast & Modal)
        bool isToastFocused = (focusIndex_ == 3);
        r.fillRect(toastActionBounds_, isToastFocused ? theme.focusFillColor : theme.surface);
        r.drawRect(toastActionBounds_, isToastFocused ? theme.focusBorderColor : theme.surfaceAlt, isToastFocused ? theme.focusBorderWidth : 1);
        r.drawText("Trigger Toast (Press A)", {toastActionBounds_.x + 14, toastActionBounds_.y + 8}, theme.fontSmall, isToastFocused ? theme.textPrimary : theme.textSecondary);

        bool isModalFocused = (focusIndex_ == 4);
        r.fillRect(modalActionBounds_, isModalFocused ? theme.focusFillColor : theme.surface);
        r.drawRect(modalActionBounds_, isModalFocused ? theme.focusBorderColor : theme.surfaceAlt, isModalFocused ? theme.focusBorderWidth : 1);
        r.drawText("Open Modal Overlay (Press A)", {modalActionBounds_.x + 14, modalActionBounds_.y + 8}, theme.fontSmall, isModalFocused ? theme.textPrimary : theme.textSecondary);

        // 6. Section Separator & Stamp Previews
        int stampY = stampSectionY_;
        r.drawText("Level 1 Atoms Stamps (ListItemWidget & GridCellWidget)", {bounds_.x + 12, stampY}, theme.fontSmall, theme.accent);
        stampY += 18;

        // Render 2 sample list rows
        hui::ListItemWidget listStamp;
        listStamp.setRow({"01. Overture 1928", "Scene Two: I. Overture", "3:37", hui::ListItemVariant::Track, 0, false, false});
        listStamp.layout({bounds_.x + 10, stampY, bounds_.w - 20, 24});
        listStamp.draw(r, theme);
        stampY += 26;

        listStamp.setRow({"02. Strange Déjà Vu", "Scene Two: II. Strange Déjà Vu", "5:12", hui::ListItemVariant::Track, 0, true, false});
        listStamp.layout({bounds_.x + 10, stampY, bounds_.w - 20, 24});
        listStamp.draw(r, theme);
        stampY += 30;

        // Render 3 grid cells with generated gradients
        int cellW = (bounds_.w - 40) / 3;
        gridCell1_.layout({bounds_.x + 10, stampY, cellW, 64});
        gridCell1_.draw(r, theme);

        gridCell2_.layout({bounds_.x + 20 + cellW, stampY, cellW, 64});
        gridCell2_.draw(r, theme);

        gridCell3_.layout({bounds_.x + 30 + cellW * 2, stampY, cellW, 64});
        gridCell3_.draw(r, theme);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager& fm) override {
        (void)fm;
        // Up/Down changes active focus row
        if (b == hui::Button::Up) {
            if (focusIndex_ > 0) {
                --focusIndex_;
            }
            return true;
        }
        if (b == hui::Button::Down) {
            if (focusIndex_ < 4) {
                ++focusIndex_;
            }
            return true;
        }

        // Delegate to focused widget
        if (focusIndex_ == 0) {
            return seekableProgress_.onButtonDown(b);
        }
        if (focusIndex_ == 1) {
            return volumeSlider_.onButtonDown(b);
        }

        if (b == hui::Button::A) {
            if (focusIndex_ == 2) {
                // Cycle playback state
                if (playbackState_ == hui::PlaybackState::Playing) playbackState_ = hui::PlaybackState::Paused;
                else if (playbackState_ == hui::PlaybackState::Paused) playbackState_ = hui::PlaybackState::Stopped;
                else playbackState_ = hui::PlaybackState::Playing;

                playbackControls_.setPlaybackState(playbackState_);
                shuffleToggle_.toggle();
                repeatModeToggle_.cycle();
                if (g_toast) {
                    const char* st = (playbackState_ == hui::PlaybackState::Playing ? "Playing" :
                                     (playbackState_ == hui::PlaybackState::Paused ? "Paused" : "Stopped"));
                    g_toast->show(std::string("Playback: ") + st, 1.2f);
                }
                return true;
            }
            if (focusIndex_ == 3) {
                if (g_toast) {
                    g_toast->show("Toast Notification #" + std::to_string(g_toastCounter++), 2.5f);
                }
                return true;
            }
            if (focusIndex_ == 4) {
                if (g_viewStack) {
                    g_viewStack->push(std::make_unique<DemoModalView>());
                }
                return true;
            }
        }

        if (b == hui::Button::B) {
            g_running = false;
            return true;
        }

        return false;
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {
            {"A", "Interact / Cycle", false, 1},
            {"B", "Quit App", false, 20},
            {"L2/R2", "Seek Track", false, 4},
            {"Up/Down", "Navigate Items", false, 2},
            {"Left/Right", "Adjust Slider", false, 3}
        };
    }

private:
    int focusIndex_ = 0;
    float currentTime_ = 145.0f;
    float totalTime_ = 312.0f;
    hui::PlaybackState playbackState_ = hui::PlaybackState::Playing;

    hui::ListHeaderWidget listHeader_;
    hui::SeekableProgressBar seekableProgress_;
    hui::Slider volumeSlider_;
    hui::PlaybackControlsRow playbackControls_;
    hui::ShuffleToggle shuffleToggle_;
    hui::RepeatModeToggle repeatModeToggle_;
    hui::SortModeIndicator sortIndicator_;

    hui::GridCellWidget gridCell1_;
    hui::GridCellWidget gridCell2_;
    hui::GridCellWidget gridCell3_;

    hui::Rect playbackRowBounds_{0, 0, 0, 0};
    hui::Rect toastActionBounds_{0, 0, 0, 0};
    hui::Rect modalActionBounds_{0, 0, 0, 0};
    int stampSectionY_ = 0;
};

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

    const int screenW = 640;
    const int screenH = 480;

    std::unique_ptr<hui::IRenderer> renderer;

#ifdef HUI_USE_SDL1
    SDL_Surface* screen = SDL_SetVideoMode(screenW, screenH, 32, SDL_SWSURFACE);
    if (!screen) {
        std::cerr << "SDL_SetVideoMode failed: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    renderer = std::make_unique<hui::SDL1Renderer>(screen);
#else
    SDL_Window* window = SDL_CreateWindow("LHCUI Phase 11 Molecules & Atoms Visual Showcase",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          screenW, screenH, SDL_WINDOW_SHOWN);
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

    // Load fonts
    TTF_Font* bodyFont = TTF_OpenFont("assets/Roboto-Regular.ttf", 15);
    TTF_Font* smallFont = TTF_OpenFont("assets/Roboto-Regular.ttf", 12);
    if (!bodyFont) {
        bodyFont = TTF_OpenFont("../assets/Roboto-Regular.ttf", 15);
        smallFont = TTF_OpenFont("../assets/Roboto-Regular.ttf", 12);
    }

    hui::FontHandle fontBodyHandle = 0;
    hui::FontHandle fontSmallHandle = 0;
    if (bodyFont) {
#ifdef HUI_USE_SDL1
        fontBodyHandle = static_cast<hui::SDL1Renderer*>(renderer.get())->registerFont(bodyFont);
        fontSmallHandle = smallFont ? static_cast<hui::SDL1Renderer*>(renderer.get())->registerFont(smallFont) : fontBodyHandle;
#else
        fontBodyHandle = static_cast<hui::SDL2Renderer*>(renderer.get())->registerFont(bodyFont);
        fontSmallHandle = smallFont ? static_cast<hui::SDL2Renderer*>(renderer.get())->registerFont(smallFont) : fontBodyHandle;
#endif
    }

    // Modern Dark Theme
    hui::Theme theme{};
    theme.background       = {18, 20, 26, 255};
    theme.surface          = {28, 32, 42, 255};
    theme.surfaceAlt       = {38, 44, 58, 255};
    theme.accent           = {70, 160, 245, 255};
    theme.textPrimary      = {245, 248, 255, 255};
    theme.textSecondary    = {150, 160, 180, 255};
    theme.textDisabled     = {90, 95, 110, 255};
    theme.warning          = {245, 80, 80, 255};
    theme.success          = {80, 220, 110, 255};
    theme.overlay          = {0, 0, 0, 175};
    theme.focusBorderColor = {90, 175, 255, 255};
    theme.focusBorderWidth = 2;
    theme.focusFillColor   = {35, 55, 85, 255};
    theme.fontBody         = fontBodyHandle;
    theme.fontSmall        = fontSmallHandle;
    theme.fontBodySize     = 15;
    theme.fontSmallSize    = 12;

    // Instantiate UISystem
    hui::UISystem uiSystem(*renderer, theme);
    example::g_viewStack = &uiSystem.viewStack();
    example::g_running = true;

    // Create persistent Chrome widgets
    hui::StatusBarWidget statusBar;
    statusBar.layout({0, 0, screenW, 24});
    statusBar.setViewMode("NOW PLAYING");
    statusBar.setContextLabel("Scenes from a Memory");
    statusBar.setNowPlaying(true);
    statusBar.setClock("14:23");
    statusBar.setBatteryLevel(92);

    hui::HintBarWidget hintBar(&uiSystem.viewStack());
    hintBar.layout({0, screenH - 28, screenW, 28});

    hui::ToastNotification toast;
    toast.layout({0, 0, screenW, screenH});
    example::g_toast = &toast;

    // Set content area for stacked views between status bar and hint bar
    uiSystem.viewStack().setContentRect({0, 24, screenW, screenH - 52});

    // Push initial showcase view
    uiSystem.viewStack().push(std::make_unique<example::MoleculeShowcaseView>());

    // Show initial welcome toast
    toast.show("Welcome to LHCUI Phase 11 Showcase!", 3.0f);

    std::cout << "\n========================================================\n"
              << "          LHCUI Molecules & Atoms Showcase               \n"
              << "========================================================\n"
              << " Controls (Keyboard Mapping):\n"
              << "  - Up / Down       : Move focus between interactive rows\n"
              << "  - Left / Right    : Adjust Slider (Master Volume)\n"
              << "  - W / R (or 3 / 4): Seek track on SeekableProgressBar (L2/R2)\n"
              << "  - Z / Space / Ent : Button A (Activate / Cycle transport / Toast)\n"
              << "  - X / Esc         : Button B (Dismiss modal / Quit)\n"
              << "  - A / C / S / V   : Buttons X and Y\n"
              << "  - Q / E           : Shoulders L1 and R1\n"
              << "========================================================\n\n" << std::flush;

    uint64_t lastTime = SDL_GetTicks();

    while (example::g_running) {
        uint64_t now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                example::g_running = false;
            }
#ifdef HUI_ENABLE_KEYBOARD_FALLBACK
            auto btnEvent = hui::KeyboardFallback::translate(e);
            if (btnEvent) {
                if (btnEvent->kind == hui::ButtonEventKind::Down) {
                    uiSystem.onButtonDown(btnEvent->button);
                } else {
                    uiSystem.onButtonUp(btnEvent->button);
                }
            }
#endif
        }

        // Update system, chrome widgets, and toast
        uiSystem.update(dt);
        statusBar.update(dt);
        toast.update(dt);

        // Render Frame
        renderer->beginFrame();

        // 1. Draw persistent chrome (Status Bar & Hint Bar)
        statusBar.draw(*renderer, theme);
        hintBar.draw(*renderer, theme);

        // 2. Draw view stack (content views + modal overlays + dimming scrim)
        uiSystem.draw();

        // 3. Draw overlay layer (Toast Notification on top of everything)
        toast.draw(*renderer, theme);

        renderer->endFrame();

        SDL_Delay(16);
    }

    if (bodyFont) TTF_CloseFont(bodyFont);
    if (smallFont && smallFont != bodyFont) TTF_CloseFont(smallFont);

#ifndef HUI_USE_SDL1
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
#endif

    renderer.reset();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
