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

// Phase 12 Organisms
#include "hui/ListView.h"
#include "hui/GuideOverlayView.h"

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
#include <unordered_set>
#include <sstream>

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
// Phase 12 QA — helpers and test views (manual sign-off harness)
// ---------------------------------------------------------------------------

class CountingListSource : public hui::IListSource {
public:
    explicit CountingListSource(int count) : count_(count) {
        rows_.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            rows_.push_back({"Row " + std::to_string(i), "Secondary " + std::to_string(i)});
        }
    }

    int rowCount() const override { return count_; }

    void rowAt(int index, hui::RowData& out) const override {
        ++rowAtCalls_;
        const auto& row = rows_[static_cast<size_t>(index)];
        out.primary = row.first;
        out.secondary = row.second;
        out.variant = hui::ListItemVariant::Track;
    }

    void resetRowAtCounter() { rowAtCalls_ = 0; }
    int rowAtCalls() const { return rowAtCalls_; }

private:
    int count_;
    mutable int rowAtCalls_ = 0;
    std::vector<std::pair<std::string, std::string>> rows_;
};

class RendererProbe : public hui::IRenderer {
public:
    explicit RendererProbe(hui::IRenderer& inner) : inner_(inner) {}

    int pushClipCount = 0;
    int popClipCount = 0;
    int drawTextCalls = 0;
    int drawTextCacheHits = 0;

    void resetFrameCounters() {
        pushClipCount = 0;
        popClipCount = 0;
    }

    void resetCacheStats() {
        drawTextCalls = 0;
        drawTextCacheHits = 0;
        seenDrawKeys_.clear();
    }

    float cacheHitRate() const {
        if (drawTextCalls == 0) {
            return 0.0f;
        }
        return 100.0f * static_cast<float>(drawTextCacheHits) / static_cast<float>(drawTextCalls);
    }

    void beginFrame() override { inner_.beginFrame(); }
    void endFrame() override { inner_.endFrame(); }

    void pushClip(hui::Rect r) override {
        ++pushClipCount;
        inner_.pushClip(r);
    }
    void popClip() override {
        ++popClipCount;
        inner_.popClip();
    }

    void fillRect(hui::Rect r, hui::Color c) override { inner_.fillRect(r, c); }
    void drawRect(hui::Rect r, hui::Color c, int thickness = 1) override {
        inner_.drawRect(r, c, thickness);
    }
    void drawLine(hui::Point a, hui::Point b, hui::Color c) override { inner_.drawLine(a, b, c); }

    int drawText(std::string_view text, hui::Point origin, hui::FontHandle font,
                 hui::Color color) override {
        ++drawTextCalls;
        std::string key = std::string(text) + "|" + std::to_string(font) + "|" +
                          std::to_string(color.r) + "," + std::to_string(color.g) + "," +
                          std::to_string(color.b) + "," + std::to_string(color.a);
        if (seenDrawKeys_.count(key) != 0) {
            ++drawTextCacheHits;
        } else {
            seenDrawKeys_.insert(std::move(key));
        }
        return inner_.drawText(text, origin, font, color);
    }

    hui::Size measureText(std::string_view text, hui::FontHandle font) override {
        return inner_.measureText(text, font);
    }

    void drawTextEllipsis(std::string_view text, hui::Point origin, hui::FontHandle font,
                          hui::Color color, int maxWidth) override {
        inner_.drawTextEllipsis(text, origin, font, color, maxWidth);
    }

    hui::TextureHandle loadTexture(std::string_view path) override { return inner_.loadTexture(path); }
    void freeTexture(hui::TextureHandle h) override { inner_.freeTexture(h); }
    hui::Size textureSize(hui::TextureHandle h) override { return inner_.textureSize(h); }
    void drawTexture(hui::TextureHandle h, hui::Rect dst, uint8_t alpha = 255) override {
        inner_.drawTexture(h, dst, alpha);
    }
    void setGlobalAlpha(uint8_t alpha) override { inner_.setGlobalAlpha(alpha); }
    hui::Size screenSize() const override { return inner_.screenSize(); }
    void invalidateTextCache() override { inner_.invalidateTextCache(); }

private:
    hui::IRenderer& inner_;
    std::unordered_set<std::string> seenDrawKeys_;
};

static void drawParagraph(hui::IRenderer& r, const hui::Theme& theme, int x, int y, int maxW,
                          std::string_view text, hui::Color color) {
    std::istringstream stream{std::string(text)};
    std::string line;
    int lineH = 16;
    while (std::getline(stream, line)) {
        r.drawText(line, {x, y}, theme.fontSmall, color);
        y += lineH;
        (void)maxW;
    }
}

static void drawBackHint(hui::IRenderer& r, const hui::Theme& theme, hui::Rect bounds) {
    r.drawText("B: Back to menu", {bounds.x + 12, bounds.y + bounds.h - 22}, theme.fontSmall,
               theme.textSecondary);
}

static void pushGuideOverlay(bool animationsEnabled) {
    if (!g_viewStack) {
        return;
    }
    auto guide = std::make_unique<hui::GuideOverlayView>(*g_viewStack, animationsEnabled);
    guide->setOnEqualizer([]() {
        if (g_toast) {
            g_toast->show("Equalizer activated", 2.0f);
        }
    });
    guide->setOnSettings([]() {
        if (g_toast) {
            g_toast->show("Settings activated", 2.0f);
        }
    });
    guide->setOnClose([]() {
        if (g_toast) {
            g_toast->show("Guide closed", 1.5f);
        }
    });
    g_viewStack->push(std::move(guide));
}

class GuideNavQATestView : public hui::View {
public:
    HUI_VIEW_TYPE(GuideNavQATestView)

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
        openBounds_ = {bounds_.x + 12, bounds_.y + bounds_.h - 64, bounds_.w - 24, 36};
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        r.fillRect(bounds_, theme.background);
        r.drawText("Guide Overlay — Nav & Sliders", {bounds_.x + 12, bounds_.y + 8}, theme.fontBody,
                   theme.accent);
        drawParagraph(
            r, theme, bounds_.x + 12, bounds_.y + 34, bounds_.w - 24,
            "Open the guide. Verify:\n"
            "• Up/Down traverses 2 sliders + 3 action items as one list\n"
            "• Left/Right on a slider adjusts value by 5\n"
            "• Left/Right on an action item does nothing (no focus move)",
            theme.textSecondary);

        bool focused = actionFocused_;
        r.fillRect(openBounds_, focused ? theme.focusFillColor : theme.surface);
        r.drawRect(openBounds_, focused ? theme.focusBorderColor : theme.surfaceAlt,
                   focused ? theme.focusBorderWidth : 1);
        r.drawText("Open Guide Overlay (A)", {openBounds_.x + 14, openBounds_.y + 10},
                   theme.fontSmall, focused ? theme.textPrimary : theme.textSecondary);
        drawBackHint(r, theme, bounds_);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager&) override {
        if (b == hui::Button::B) {
            if (g_viewStack) {
                g_viewStack->pop();
            }
            return true;
        }
        if (b == hui::Button::A) {
            if (actionFocused_) {
                pushGuideOverlay(true);
            }
            return true;
        }
        return false;
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {{"A", "Open Guide", false, 1}, {"B", "Back", false, 10}};
    }

private:
    hui::Rect openBounds_{0, 0, 0, 0};
    bool actionFocused_ = true;
};

class GuideAnimQATestView : public hui::View {
public:
    HUI_VIEW_TYPE(GuideAnimQATestView)

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
        int y = bounds_.y + bounds_.h - 110;
        animatedBounds_ = {bounds_.x + 12, y, bounds_.w - 24, 36};
        instantBounds_ = {bounds_.x + 12, y + 42, bounds_.w - 24, 36};
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        r.fillRect(bounds_, theme.background);
        r.drawText("Guide Overlay — Slide Animation", {bounds_.x + 12, bounds_.y + 8},
                   theme.fontBody, theme.accent);
        drawParagraph(
            r, theme, bounds_.x + 12, bounds_.y + 34, bounds_.w - 24,
            "SDL2: animated open should slide in from the right.\n"
            "Instant open should appear with zero slide offset (no tween frames).",
            theme.textSecondary);

        auto drawBtn = [&](const hui::Rect& rect, bool sel, const char* label) {
            r.fillRect(rect, sel ? theme.focusFillColor : theme.surface);
            r.drawRect(rect, sel ? theme.focusBorderColor : theme.surfaceAlt,
                       sel ? theme.focusBorderWidth : 1);
            r.drawText(label, {rect.x + 14, rect.y + 10}, theme.fontSmall,
                       sel ? theme.textPrimary : theme.textSecondary);
        };
        drawBtn(animatedBounds_, focusIndex_ == 0, "Open with slide animation (SDL2)");
        drawBtn(instantBounds_, focusIndex_ == 1, "Open instant (animations disabled)");
        drawBackHint(r, theme, bounds_);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager&) override {
        if (b == hui::Button::Up && focusIndex_ > 0) {
            --focusIndex_;
            return true;
        }
        if (b == hui::Button::Down && focusIndex_ < 1) {
            ++focusIndex_;
            return true;
        }
        if (b == hui::Button::B) {
            if (g_viewStack) {
                g_viewStack->pop();
            }
            return true;
        }
        if (b == hui::Button::A && g_viewStack) {
            pushGuideOverlay(focusIndex_ == 0);
            return true;
        }
        return false;
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {{"A", "Open Guide", false, 1}, {"B", "Back", false, 10}};
    }

private:
    hui::Rect animatedBounds_{0, 0, 0, 0};
    hui::Rect instantBounds_{0, 0, 0, 0};
    int focusIndex_ = 0;
};

class ListWindowQATestView : public hui::View {
public:
    HUI_VIEW_TYPE(ListWindowQATestView)

    ListWindowQATestView() : source_(5000) {
        list_.setSource(&source_);
        list_.layout({0, 0, 200, 200});
    }

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
        list_.layout({bounds_.x + 10, bounds_.y + 130, bounds_.w - 20, bounds_.h - 160});
    }

    void update(float, hui::FocusManager& fm) override {
        if (fm.focused() != &list_) {
            fm.setFocus(&list_);
        }
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        r.fillRect(bounds_, theme.background);
        r.drawText("ListView — 5,000 Row Windowing", {bounds_.x + 12, bounds_.y + 8}, theme.fontBody,
                   theme.accent);
        drawParagraph(
            r, theme, bounds_.x + 12, bounds_.y + 34, bounds_.w - 24,
            "Scroll with Up/Down. Each frame should draw at most pageRows+2 rows\n"
            "and use exactly one pushClip/popClip pair for the list body.",
            theme.textSecondary);

        source_.resetRowAtCounter();
        RendererProbe probe(r);
        probe.resetFrameCounters();
        list_.draw(probe, theme);

        lastRowDraws_ = source_.rowAtCalls();
        lastPushClip_ = probe.pushClipCount;
        lastPopClip_ = probe.popClipCount;
        const int maxRows = list_.pageRows() + 2;
        pass_ = (lastRowDraws_ <= maxRows && lastPushClip_ == 1 && lastPopClip_ == 1);

        std::ostringstream stats;
        stats << "pageRows=" << list_.pageRows()
              << "  row draws=" << lastRowDraws_ << " (max " << maxRows << ")"
              << "  clip=" << lastPushClip_ << "/" << lastPopClip_
              << "  => " << (pass_ ? "PASS" : "CHECK");
        r.drawText(stats.str(), {bounds_.x + 12, bounds_.y + 108}, theme.fontSmall,
                   pass_ ? theme.success : theme.warning);

        drawBackHint(r, theme, bounds_);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager&) override {
        if (b == hui::Button::B) {
            if (g_viewStack) {
                g_viewStack->pop();
            }
            return true;
        }
        return list_.onButtonDown(b);
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {{"Up/Down", "Scroll List", false, 2}, {"B", "Back", false, 10}};
    }

private:
    CountingListSource source_;
    hui::ListView list_{40};
    int lastRowDraws_ = 0;
    int lastPushClip_ = 0;
    int lastPopClip_ = 0;
    bool pass_ = false;
};

class ListCacheQATestView : public hui::View {
public:
    HUI_VIEW_TYPE(ListCacheQATestView)

    ListCacheQATestView() : source_(5000) {
        list_.setSource(&source_);
    }

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
        startBounds_ = {bounds_.x + 12, bounds_.y + bounds_.h - 64, bounds_.w - 24, 36};
        list_.layout({bounds_.x + 10, bounds_.y + 130, bounds_.w - 20, bounds_.h - 160});
    }

    void update(float dt, hui::FocusManager& fm) override {
        if (!running_ && fm.focused() != &list_) {
            fm.setFocus(&list_);
        }
        if (!running_) {
            return;
        }

        scrollTime_ += dt;
        list_.onButtonDown(hui::Button::Down);

        if (scrollTime_ >= 5.0f) {
            running_ = false;
            finished_ = true;
        }
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        r.fillRect(bounds_, theme.background);
        r.drawText("ListView — Scroll Text Cache (5 s)", {bounds_.x + 12, bounds_.y + 8},
                   theme.fontBody, theme.accent);
        drawParagraph(
            r, theme, bounds_.x + 12, bounds_.y + 34, bounds_.w - 24,
            "Press A to auto-scroll for 5 s. drawText cache hit rate should stay high\n"
            "(near-zero means cache is keyed on position, not content).",
            theme.textSecondary);

        if (running_ || finished_) {
            std::ostringstream line;
            const float hitRate = (drawTextCalls_ > 0)
                ? (100.0f * static_cast<float>(drawTextCacheHits_) /
                   static_cast<float>(drawTextCalls_))
                : 0.0f;
            line << "Elapsed: " << static_cast<int>(scrollTime_) << "s / 5s"
                 << "  drawText hits: " << drawTextCacheHits_ << " / " << drawTextCalls_
                 << " (" << static_cast<int>(hitRate) << "%)";
            r.drawText(line.str(), {bounds_.x + 12, bounds_.y + 108}, theme.fontSmall,
                       theme.textPrimary);
            if (finished_) {
                const bool pass = hitRate >= 50.0f;
                r.drawText(pass ? "PASS — cache hit rate healthy" : "CHECK — hit rate low",
                           {bounds_.x + 12, bounds_.y + 124}, theme.fontSmall,
                           pass ? theme.success : theme.warning);
            }
        }

        if (running_ || finished_) {
            if (!probe_) {
                probe_ = std::make_unique<RendererProbe>(r);
            }
            list_.draw(*probe_, theme);
            drawTextCalls_ = probe_->drawTextCalls;
            drawTextCacheHits_ = probe_->drawTextCacheHits;
        } else {
            list_.draw(r, theme);
        }

        if (!running_) {
            bool focused = !finished_;
            r.fillRect(startBounds_, focused ? theme.focusFillColor : theme.surface);
            r.drawRect(startBounds_, focused ? theme.focusBorderColor : theme.surfaceAlt,
                       focused ? theme.focusBorderWidth : 1);
            r.drawText(finished_ ? "Test complete — B to go back" : "Start 5 s scroll test (A)",
                       {startBounds_.x + 14, startBounds_.y + 10}, theme.fontSmall,
                       focused ? theme.textPrimary : theme.textSecondary);
        }
        drawBackHint(r, theme, bounds_);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager&) override {
        if (b == hui::Button::B) {
            if (g_viewStack) {
                g_viewStack->pop();
            }
            return true;
        }
        if (b == hui::Button::A && !running_ && !finished_) {
            probe_.reset();
            drawTextCalls_ = 0;
            drawTextCacheHits_ = 0;
            scrollTime_ = 0.0f;
            running_ = true;
            return true;
        }
        if (!running_) {
            return list_.onButtonDown(b);
        }
        return true;
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {{"A", "Start Test", false, 1}, {"B", "Back", false, 10}};
    }

private:
    CountingListSource source_;
    hui::ListView list_{40};
    std::unique_ptr<RendererProbe> probe_;
    int drawTextCalls_ = 0;
    int drawTextCacheHits_ = 0;
    hui::Rect startBounds_{0, 0, 0, 0};
    float scrollTime_ = 0.0f;
    bool running_ = false;
    bool finished_ = false;
};

class ListShortQATestView : public hui::View {
public:
    HUI_VIEW_TYPE(ListShortQATestView)

    ListShortQATestView() : source_(3) {
        list_.setSource(&source_);
    }

    void layout(hui::Rect contentRect) override {
        bounds_ = contentRect;
        list_.layout({bounds_.x + 10, bounds_.y + 120, bounds_.w - 20, bounds_.h - 150});
    }

    void update(float, hui::FocusManager& fm) override {
        if (fm.focused() != &list_) {
            fm.setFocus(&list_);
        }
    }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        r.fillRect(bounds_, theme.background);
        r.drawText("ListView — Short List scrollOffset", {bounds_.x + 12, bounds_.y + 8},
                   theme.fontBody, theme.accent);
        drawParagraph(
            r, theme, bounds_.x + 12, bounds_.y + 34, bounds_.w - 24,
            "Only 3 items in a tall viewport. scrollOffset must stay 0 when navigating.\n"
            "Wrap Up/Down — offset should remain 0 (no UB clamp issues).",
            theme.textSecondary);

        const int offset = list_.scrollOffset();
        const bool pass = (offset == 0);
        std::ostringstream stats;
        stats << "scrollOffset=" << offset << "  focus=" << list_.getFocusIndex()
              << "  => " << (pass ? "PASS" : "FAIL");
        r.drawText(stats.str(), {bounds_.x + 12, bounds_.y + 100}, theme.fontSmall,
                   pass ? theme.success : theme.warning);

        list_.draw(r, theme);
        drawBackHint(r, theme, bounds_);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager&) override {
        if (b == hui::Button::B) {
            if (g_viewStack) {
                g_viewStack->pop();
            }
            return true;
        }
        return list_.onButtonDown(b);
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {{"Up/Down", "Navigate / Wrap", false, 2}, {"B", "Back", false, 10}};
    }

private:
    CountingListSource source_;
    hui::ListView list_{40};
};

class Phase12QAMenuView : public hui::View {
public:
    HUI_VIEW_TYPE(Phase12QAMenuView)

    Phase12QAMenuView() {
        items_ = {
            "Guide Overlay — Nav & Sliders",
            "Guide Overlay — Slide Animation",
            "ListView — 5,000 Row Windowing",
            "ListView — Scroll Text Cache (5 s)",
            "ListView — Short List scrollOffset",
        };
    }

    void layout(hui::Rect contentRect) override { bounds_ = contentRect; }

    void draw(hui::IRenderer& r, const hui::Theme& theme) override {
        r.fillRect(bounds_, theme.background);
        r.drawText("Phase 12 QA — Manual Tests", {bounds_.x + 12, bounds_.y + 8}, theme.fontBody,
                   theme.accent);
        drawParagraph(r, theme, bounds_.x + 12, bounds_.y + 30, bounds_.w - 24,
                      "Remaining sign-off items. Select a test, follow on-screen instructions.",
                      theme.textSecondary);

        int y = bounds_.y + 72;
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            bool focused = (i == focusIndex_);
            hui::Rect row{bounds_.x + 10, y, bounds_.w - 20, 30};
            r.fillRect(row, focused ? theme.focusFillColor : theme.surface);
            if (focused) {
                r.drawRect(row, theme.focusBorderColor, theme.focusBorderWidth);
            }
            r.drawText(items_[i], {row.x + 10, row.y + 7}, theme.fontSmall,
                       focused ? theme.textPrimary : theme.textSecondary);
            y += 34;
        }
        drawBackHint(r, theme, bounds_);
    }

    bool onButtonDown(hui::Button b, hui::FocusManager&) override {
        if (b == hui::Button::Up && focusIndex_ > 0) {
            --focusIndex_;
            return true;
        }
        if (b == hui::Button::Down &&
            focusIndex_ < static_cast<int>(items_.size()) - 1) {
            ++focusIndex_;
            return true;
        }
        if (b == hui::Button::B) {
            if (g_viewStack) {
                g_viewStack->pop();
            }
            return true;
        }
        if (b == hui::Button::A && g_viewStack) {
            switch (focusIndex_) {
                case 0:
                    g_viewStack->push(std::make_unique<GuideNavQATestView>());
                    break;
                case 1:
                    g_viewStack->push(std::make_unique<GuideAnimQATestView>());
                    break;
                case 2:
                    g_viewStack->push(std::make_unique<ListWindowQATestView>());
                    break;
                case 3:
                    g_viewStack->push(std::make_unique<ListCacheQATestView>());
                    break;
                case 4:
                    g_viewStack->push(std::make_unique<ListShortQATestView>());
                    break;
            }
            return true;
        }
        return false;
    }

    std::vector<hui::HintEntry> currentHints() const override {
        return {{"A", "Run Test", false, 1}, {"B", "Back", false, 10}};
    }

private:
    std::vector<std::string> items_;
    int focusIndex_ = 0;
};

// ---------------------------------------------------------------------------
// Interactive Main Showcase View
// ---------------------------------------------------------------------------
class MoleculeShowcaseView : public hui::View {
public:
    HUI_VIEW_TYPE(MoleculeShowcaseView)

    MoleculeShowcaseView() {
        listHeader_.setLabel("/home/user/music/rock/progressive/dream_theater/scenes_from_a_memory/05_strange_deja_vu.flac");
        listHeader_.setItemCount(12);
        listHeader_.setSortBadge("Track #");

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

        volumeSlider_.setLabel("Master Volume");
        volumeSlider_.setRange(0, 100, 5);
        volumeSlider_.setValue(75);
        volumeSlider_.setOnValueChanged([](int val) {
            if (g_toast) {
                g_toast->show("Volume: " + std::to_string(val) + "%", 1.0f);
            }
        });

        playbackControls_.setPlaybackState(playbackState_);
        playbackControls_.setShuffle(true);
        playbackControls_.setRepeatMode(hui::RepeatMode::All);
        playbackControls_.setOnActivate([this](hui::TransportAction action) {
            switch (action) {
                case hui::TransportAction::Previous:
                    if (g_toast) g_toast->show("Previous track", 1.0f);
                    break;
                case hui::TransportAction::PlayPause:
                    playbackState_ = (playbackState_ == hui::PlaybackState::Playing)
                        ? hui::PlaybackState::Paused : hui::PlaybackState::Playing;
                    playbackControls_.setPlaybackState(playbackState_);
                    break;
                case hui::TransportAction::Next:
                    if (g_toast) g_toast->show("Next track", 1.0f);
                    break;
                case hui::TransportAction::Shuffle:
                    shuffleOn_ = !shuffleOn_;
                    playbackControls_.setShuffle(shuffleOn_);
                    break;
                case hui::TransportAction::Repeat:
                    repeatMode_ = static_cast<hui::RepeatMode>(
                        (static_cast<int>(repeatMode_) + 1) % 3);
                    playbackControls_.setRepeatMode(repeatMode_);
                    break;
            }
        });

        sortIndicator_.setMode("Track #");

        gridCell1_.setCell("Metropolis Pt. 2", "1999", 0, false, false);
        gridCell2_.setCell("Images and Words", "1992", 0, true, false);
        gridCell3_.setCell("Octavarium", "2005", 0, false, false);
    }

    void onSuspend() override { inputSuspended_ = true; }
    void onResume() override { inputSuspended_ = false; }

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

        // 4. Playback Controls Row
        playbackRowBounds_ = {bounds_.x + 10, y, bounds_.w - 20, 36};
        playbackControls_.layout(playbackRowBounds_);
        sortIndicator_.layout({playbackRowBounds_.x + playbackRowBounds_.w - 100, playbackRowBounds_.y + 6, 90, 24});
        y += 42;

        // 5. Toast Trigger, Modal Trigger, and Phase 12 QA action buttons
        toastActionBounds_ = {bounds_.x + 10, y, (bounds_.w - 26) / 2, 32};
        modalActionBounds_ = {bounds_.x + 16 + (bounds_.w - 26) / 2, y, (bounds_.w - 26) / 2, 32};
        y += 38;
        qaActionBounds_ = {bounds_.x + 10, y, bounds_.w - 20, 32};
        y += 38;

        // 6. Stamp Previews (List Items & Grid Cells)
        stampSectionY_ = y;
    }

    void update(float dt, hui::FocusManager& fm) override {
        (void)dt;
        if (inputSuspended_) {
            return;
        }
        // Sync focus with focusIndex_
        if (focusIndex_ == 0 && fm.focused() != &seekableProgress_) {
            fm.setFocus(&seekableProgress_);
        } else if (focusIndex_ == 1 && fm.focused() != &volumeSlider_) {
            fm.setFocus(&volumeSlider_);
        } else if (focusIndex_ == 2 && fm.focused() != &playbackControls_) {
            fm.setFocus(&playbackControls_);
        } else if (focusIndex_ >= 3 && fm.focused() != nullptr) {
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

        // 4. Draw Playback Controls
        playbackControls_.draw(r, theme);
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

        bool isQaFocused = (focusIndex_ == 5);
        r.fillRect(qaActionBounds_, isQaFocused ? theme.focusFillColor : theme.surface);
        r.drawRect(qaActionBounds_, isQaFocused ? theme.focusBorderColor : theme.surfaceAlt, isQaFocused ? theme.focusBorderWidth : 1);
        r.drawText("Phase 12 QA Tests (Press A)", {qaActionBounds_.x + 14, qaActionBounds_.y + 8}, theme.fontSmall, isQaFocused ? theme.textPrimary : theme.textSecondary);

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
            if (focusIndex_ < 5) {
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

        if (focusIndex_ == 2) {
            return playbackControls_.onButtonDown(b);
        }

        if (b == hui::Button::A) {
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
            if (focusIndex_ == 5) {
                if (g_viewStack) {
                    g_viewStack->push(std::make_unique<Phase12QAMenuView>());
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
    bool inputSuspended_ = false;
    int focusIndex_ = 0;
    float currentTime_ = 145.0f;
    float totalTime_ = 312.0f;
    hui::PlaybackState playbackState_ = hui::PlaybackState::Playing;

    hui::ListHeaderWidget listHeader_;
    hui::SeekableProgressBar seekableProgress_;
    hui::Slider volumeSlider_;
    hui::PlaybackControlsRow playbackControls_;
    hui::SortModeIndicator sortIndicator_;
    bool shuffleOn_ = true;
    hui::RepeatMode repeatMode_ = hui::RepeatMode::All;

    hui::GridCellWidget gridCell1_;
    hui::GridCellWidget gridCell2_;
    hui::GridCellWidget gridCell3_;

    hui::Rect playbackRowBounds_{0, 0, 0, 0};
    hui::Rect toastActionBounds_{0, 0, 0, 0};
    hui::Rect modalActionBounds_{0, 0, 0, 0};
    hui::Rect qaActionBounds_{0, 0, 0, 0};
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
    SDL_Window* window = SDL_CreateWindow("LHCUI Showcase — Molecules & Phase 12 QA",
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
    toast.show("Welcome! Navigate to Phase 12 QA Tests for manual sign-off.", 3.0f);

    std::cout << "\n========================================================\n"
              << "          LHCUI Showcase (Molecules + Phase 12 QA)       \n"
              << "========================================================\n"
              << " Controls (Keyboard Mapping):\n"
              << "  - Up / Down       : Move focus between interactive rows\n"
              << "  - Left / Right    : Adjust Slider (Master Volume)\n"
              << "  - W / R (or 3 / 4): Seek track on SeekableProgressBar (L2/R2)\n"
              << "  - Z / Space / Ent : Button A (Activate / Cycle transport / Toast)\n"
              << "  - X / Esc         : Button B (Dismiss modal / Quit)\n"
              << "  - A / C / S / V   : Buttons X and Y\n"
              << "  - Q / E           : Shoulders L1 and R1\n"
              << "\n"
              << " Phase 12 manual QA: focus \"Phase 12 QA Tests\" and press A.\n"
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
