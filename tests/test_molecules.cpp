#include "doctest.h"
#include "hui/types.h"
#include "hui/IRenderer.h"
#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/FocusManager.h"
#include "hui/ListHeaderWidget.h"
#include "hui/SeekableProgressBar.h"
#include "hui/PlaybackControlsRow.h"
#include "hui/HintBarWidget.h"
#include "hui/StatusBarWidget.h"
#include "hui/ToastNotification.h"
#include "hui/Helpers.h"

#include <vector>
#include <string>
#include <string_view>
#include <memory>

using namespace hui;

namespace {

struct DrawRectCall {
    Rect rect;
    Color color;
    int thickness;
};

struct FillRectCall {
    Rect rect;
    Color color;
};

struct DrawTextCall {
    std::string text;
    Point origin;
    Color color;
};

struct DrawTextEllipsisCall {
    std::string text;
    Point origin;
    Color color;
    int maxWidth;
};

class MoleculeTestRenderer : public IRenderer {
public:
    std::vector<DrawRectCall> drawRects;
    std::vector<FillRectCall> fillRects;
    std::vector<DrawTextCall> drawTexts;
    std::vector<DrawTextEllipsisCall> drawTextEllipses;

    void clearLogs() {
        drawRects.clear();
        fillRects.clear();
        drawTexts.clear();
        drawTextEllipses.clear();
    }

    void beginFrame() override {}
    void endFrame() override {}
    void pushClip(Rect) override {}
    void popClip() override {}

    void fillRect(Rect r, Color c) override {
        fillRects.push_back({r, c});
    }

    void drawRect(Rect r, Color c, int thickness = 1) override {
        drawRects.push_back({r, c, thickness});
    }

    void drawLine(Point, Point, Color) override {}

    int drawText(std::string_view text, Point origin, FontHandle, Color color) override {
        drawTexts.push_back({std::string(text), origin, color});
        return static_cast<int>(text.size()) * 8;
    }

    Size measureText(std::string_view text, FontHandle) override {
        return {static_cast<int>(text.size()) * 8, 16};
    }

    void drawTextEllipsis(std::string_view text, Point origin, FontHandle, Color color, int maxWidth) override {
        drawTextEllipses.push_back({std::string(text), origin, color, maxWidth});
    }

    TextureHandle loadTexture(std::string_view) override { return 0; }
    void freeTexture(TextureHandle) override {}
    Size textureSize(TextureHandle) override { return {16, 16}; }
    void drawTexture(TextureHandle, Rect, uint8_t) override {}
    void setGlobalAlpha(uint8_t) override {}
    Size screenSize() const override { return {640, 480}; }
};

Theme createTestTheme() {
    Theme t{};
    t.background = {20, 20, 20, 255};
    t.surface = {35, 35, 35, 255};
    t.surfaceAlt = {45, 45, 45, 255};
    t.accent = {0, 150, 255, 255};
    t.textPrimary = {240, 240, 240, 255};
    t.textSecondary = {160, 160, 160, 255};
    t.textDisabled = {90, 90, 90, 255};
    t.warning = {255, 80, 80, 255};
    t.success = {80, 220, 100, 255};
    t.overlay = {0, 0, 0, 180};
    t.focusBorderColor = {0, 180, 255, 255};
    t.focusBorderWidth = 2;
    t.focusFillColor = {0, 150, 255, 40};
    t.fontBody = 1;
    t.fontSmall = 2;
    t.fontMono = 3;
    t.fontBodySize = 14;
    t.fontSmallSize = 11;
    return t;
}

class TestViewWithHints : public View {
public:
    explicit TestViewWithHints(std::vector<HintEntry> hints)
        : hints_(std::move(hints)) {}

    void draw(IRenderer&, const Theme&) override {}

    std::vector<HintEntry> currentHints() const override {
        return hints_;
    }

    void setHints(std::vector<HintEntry> hints) {
        hints_ = std::move(hints);
    }

private:
    std::vector<HintEntry> hints_;
};

} // namespace

TEST_SUITE("Phase 11 - Molecules") {

    TEST_CASE("ListHeaderWidget focusability and properties") {
        ListHeaderWidget header;
        CHECK_FALSE(header.isFocusable());

        header.setLabel("/home/user/music/rock");
        CHECK(header.label() == "/home/user/music/rock");

        header.setItemCount(42);
        CHECK(header.itemCount() == 42);

        header.setSortBadge("Artist");
        CHECK(header.sortBadge() == "Artist");

        header.setIcon(10);
        CHECK(header.icon() == 10);
    }

    TEST_CASE("ListHeaderWidget renders left-truncated path without overflowing bounds") {
        MoleculeTestRenderer r;
        Theme theme = createTestTheme();

        ListHeaderWidget header;
        header.setLabel("/home/user/music/rock/progressive/great_band/album_name/track_title.flac");
        header.setItemCount(15);
        header.setSortBadge("Title");
        header.layout({0, 0, 200, 28});

        header.draw(r, theme);

        CHECK(!r.drawTexts.empty());
        bool foundLeftTruncated = false;
        for (const auto& dt : r.drawTexts) {
            if (dt.text.rfind("…/", 0) == 0) {
                foundLeftTruncated = true;
                // Confirm rightmost part is preserved
                CHECK(dt.text.find("track_title.flac") != std::string::npos);
            }
        }
        CHECK(foundLeftTruncated);
    }

    TEST_CASE("SeekableProgressBar focusable and L2/R2 event consumption") {
        SeekableProgressBar bar;
        CHECK(bar.isFocusable());

        int seekDir = 0;
        int seekCalls = 0;
        bar.setOnSeek([&](int dir) {
            seekDir = dir;
            seekCalls++;
        });

        // Non-seek buttons should return false and not invoke onSeek
        CHECK_FALSE(bar.onButtonDown(Button::A));
        CHECK_FALSE(bar.onButtonDown(Button::B));
        CHECK_FALSE(bar.onButtonDown(Button::Left));
        CHECK_FALSE(bar.onButtonDown(Button::Right));
        CHECK_FALSE(bar.onButtonDown(Button::Up));
        CHECK_FALSE(bar.onButtonDown(Button::Down));
        CHECK_FALSE(bar.onButtonDown(Button::L1));
        CHECK_FALSE(bar.onButtonDown(Button::R1));
        CHECK(seekCalls == 0);

        // L2 button consumes event and calls onSeek with -1
        CHECK(bar.onButtonDown(Button::L2));
        CHECK(seekCalls == 1);
        CHECK(seekDir == -1);

        // R2 button consumes event and calls onSeek with +1
        CHECK(bar.onButtonDown(Button::R2));
        CHECK(seekCalls == 2);
        CHECK(seekDir == 1);

        // When disabled, does not consume L2/R2
        bar.setDisabled(true);
        CHECK_FALSE(bar.onButtonDown(Button::L2));
        CHECK_FALSE(bar.onButtonDown(Button::R2));
        CHECK(seekCalls == 2);
    }

    TEST_CASE("PlaybackControlsRow reflects Play, Pause, and Stop states") {
        MoleculeTestRenderer r;
        Theme theme = createTestTheme();

        PlaybackControlsRow controls;
        CHECK_FALSE(controls.isFocusable());
        controls.layout({0, 0, 300, 40});

        // 1. Stopped
        controls.setPlaybackState(PlaybackState::Stopped);
        CHECK(controls.playbackState() == PlaybackState::Stopped);
        r.clearLogs();
        controls.draw(r, theme);
        bool foundStopSymbol = false;
        for (const auto& dt : r.drawTexts) {
            if (dt.text == "[]") {
                foundStopSymbol = true;
                CHECK(dt.color.r == theme.textDisabled.r);
            }
        }
        CHECK(foundStopSymbol);

        // 2. Playing
        controls.setPlaybackState(PlaybackState::Playing);
        CHECK(controls.playbackState() == PlaybackState::Playing);
        r.clearLogs();
        controls.draw(r, theme);
        bool foundPlayingSymbol = false;
        for (const auto& dt : r.drawTexts) {
            if (dt.text == "||") {
                foundPlayingSymbol = true;
                CHECK(dt.color.r == theme.accent.r);
            }
        }
        CHECK(foundPlayingSymbol);

        // 3. Paused
        controls.setPlaybackState(PlaybackState::Paused);
        CHECK(controls.playbackState() == PlaybackState::Paused);
        r.clearLogs();
        controls.draw(r, theme);
        bool foundPausedSymbol = false;
        for (const auto& dt : r.drawTexts) {
            if (dt.text == "|>") {
                foundPausedSymbol = true;
                CHECK(dt.color.r == theme.textPrimary.r);
            }
        }
        CHECK(foundPausedSymbol);
    }

    TEST_CASE("HintBarWidget face button glyph colors and non-button label colors") {
        MoleculeTestRenderer r;
        Theme theme = createTestTheme();

        HintBarWidget hintBar;
        CHECK_FALSE(hintBar.isFocusable());
        hintBar.layout({0, 450, 640, 30});

        std::vector<HintEntry> hints = {
            {"A", "Play", false, 0},
            {"B", "Back", false, 100},
            {"X", "Queue", false, 10},
            {"Y", "Options", false, 20},
            {"START", "Menu", false, 30},
        };
        hintBar.setHints(hints);
        hintBar.draw(r, theme);

        // Check button glyph colors
        for (const auto& dt : r.drawTexts) {
            if (dt.text == "A") {
                CHECK(dt.color.r == 220);
                CHECK(dt.color.g == 50);
                CHECK(dt.color.b == 50);
            } else if (dt.text == "B") {
                CHECK(dt.color.r == 220);
                CHECK(dt.color.g == 160);
                CHECK(dt.color.b == 40);
            } else if (dt.text == "X") {
                CHECK(dt.color.r == 60);
                CHECK(dt.color.g == 120);
                CHECK(dt.color.b == 220);
            } else if (dt.text == "Y") {
                CHECK(dt.color.r == 60);
                CHECK(dt.color.g == 180);
                CHECK(dt.color.b == 80);
            } else if (dt.text == "START") {
                CHECK(dt.color.r == theme.textSecondary.r);
                CHECK(dt.color.g == theme.textSecondary.g);
                CHECK(dt.color.b == theme.textSecondary.b);
            }
        }
    }

    TEST_CASE("HintBarWidget caps at five hints, truncating from middle, with A and B retained") {
        MoleculeTestRenderer r;
        Theme theme = createTestTheme();

        HintBarWidget hintBar;
        hintBar.layout({0, 450, 640, 30});

        // 7 hints, unordered
        std::vector<HintEntry> hints = {
            {"B", "Back", false, 99},      // Last
            {"X", "ActionX", false, 2},
            {"MID1", "Middle1", false, 3},
            {"MID2", "Middle2", false, 4},
            {"MID3", "Middle3", false, 5},
            {"Y", "ActionY", false, 6},
            {"A", "Select", false, 1},     // First
        };
        hintBar.setHints(hints);
        hintBar.draw(r, theme);

        // Collect drawn button labels in order
        std::vector<std::string> drawnButtons;
        for (const auto& dt : r.drawTexts) {
            if (dt.text == "A" || dt.text == "B" || dt.text == "X" || dt.text == "Y" ||
                dt.text == "MID1" || dt.text == "MID2" || dt.text == "MID3") {
                drawnButtons.push_back(dt.text);
            }
        }

        // Must be capped at 5
        CHECK(drawnButtons.size() == 5);
        // A must be first, B must be last
        CHECK(drawnButtons.front() == "A");
        CHECK(drawnButtons.back() == "B");
    }

    TEST_CASE("HintBarWidget dynamic ViewStack tracking") {
        MoleculeTestRenderer r;
        Theme theme = createTestTheme();

        ViewStack stack;
        FocusManager fm;

        auto baseView = std::make_unique<TestViewWithHints>(std::vector<HintEntry>{
            {"A", "Play", false, 1},
            {"B", "Exit", false, 10}
        });
        auto overlayView = std::make_unique<TestViewWithHints>(std::vector<HintEntry>{
            {"X", "Confirm", false, 1},
            {"B", "Cancel", false, 10}
        });

        stack.push(std::move(baseView));
        stack.applyPendingMutations(fm);

        HintBarWidget hintBar(&stack);
        hintBar.layout({0, 450, 640, 30});

        // 1. Reads base view hints
        r.clearLogs();
        hintBar.draw(r, theme);
        bool foundPlay = false;
        for (const auto& dt : r.drawTexts) {
            if (dt.text == "Play") foundPlay = true;
        }
        CHECK(foundPlay);

        // 2. Push overlay -> reads overlay hints immediately
        stack.push(std::move(overlayView));
        stack.applyPendingMutations(fm);
        r.clearLogs();
        hintBar.draw(r, theme);
        bool foundConfirm = false;
        bool foundOldPlay = false;
        for (const auto& dt : r.drawTexts) {
            if (dt.text == "Confirm") foundConfirm = true;
            if (dt.text == "Play") foundOldPlay = true;
        }
        CHECK(foundConfirm);
        CHECK_FALSE(foundOldPlay);

        // 3. Pop overlay -> reverts to base view hints immediately
        stack.pop();
        stack.applyPendingMutations(fm);
        r.clearLogs();
        hintBar.draw(r, theme);
        foundPlay = false;
        foundConfirm = false;
        for (const auto& dt : r.drawTexts) {
            if (dt.text == "Play") foundPlay = true;
            if (dt.text == "Confirm") foundConfirm = true;
        }
        CHECK(foundPlay);
        CHECK_FALSE(foundConfirm);
    }

    TEST_CASE("StatusBarWidget animated now-playing pulse across nonzero dt") {
        MoleculeTestRenderer r;
        Theme theme = createTestTheme();

        StatusBarWidget statusBar;
        CHECK_FALSE(statusBar.isFocusable());
        statusBar.layout({0, 0, 640, 24});
        statusBar.setViewMode("LIBRARY");
        statusBar.setContextLabel("Rock");
        statusBar.setClock("14:30");
        statusBar.setBatteryLevel(85);
        statusBar.setNowPlaying(true);

        // Frame 1
        statusBar.update(0.0f);
        r.clearLogs();
        statusBar.draw(r, theme);
        REQUIRE(!r.fillRects.empty());
        // Find pulse rect fill color (the one with accent rgb)
        Color color1{0, 0, 0, 0};
        for (const auto& fr : r.fillRects) {
            if (fr.color.r == theme.accent.r && fr.color.g == theme.accent.g && fr.color.b == theme.accent.b) {
                color1 = fr.color;
            }
        }

        // Advance time by nonzero dt
        statusBar.update(0.35f);
        r.clearLogs();
        statusBar.draw(r, theme);
        Color color2{0, 0, 0, 0};
        for (const auto& fr : r.fillRects) {
            if (fr.color.r == theme.accent.r && fr.color.g == theme.accent.g && fr.color.b == theme.accent.b) {
                color2 = fr.color;
            }
        }

        // Pulse alpha must have changed across frames
        CHECK(color1.a != color2.a);
    }

    TEST_CASE("ToastNotification self-timing, auto-dismiss, and replacement policy") {
        MoleculeTestRenderer r;
        Theme theme = createTestTheme();

        ToastNotification toast;
        CHECK_FALSE(toast.isFocusable());
        toast.layout({0, 0, 640, 480});

        // 1. Initial state: not visible, draws nothing
        CHECK_FALSE(toast.isVisible());
        r.clearLogs();
        toast.draw(r, theme);
        CHECK(r.fillRects.empty());
        CHECK(r.drawTexts.empty());

        // 2. Show toast 1 for 1.0s (0.2s fade)
        toast.show("Toast Message 1", 1.0f, 0.2f);
        CHECK(toast.isVisible());
        CHECK(toast.message() == "Toast Message 1");
        CHECK(toast.remainingTime() == doctest::Approx(1.0f));

        r.clearLogs();
        toast.draw(r, theme);
        CHECK(!r.drawTexts.empty());
        CHECK(r.drawTexts.back().text == "Toast Message 1");

        // Advance 0.5s -> still visible
        toast.update(0.5f);
        CHECK(toast.isVisible());
        CHECK(toast.remainingTime() == doctest::Approx(0.5f));

        // 3. Replacement policy: show Toast Message 2 while 1 is active
        toast.show("Toast Message 2", 2.0f, 0.3f);
        CHECK(toast.isVisible());
        CHECK(toast.message() == "Toast Message 2");
        CHECK(toast.remainingTime() == doctest::Approx(2.0f));

        r.clearLogs();
        toast.draw(r, theme);
        CHECK(r.drawTexts.size() == 1);
        CHECK(r.drawTexts.back().text == "Toast Message 2");

        // Advance by 1.8s -> in fade phase
        toast.update(1.8f);
        CHECK(toast.isVisible());
        CHECK(toast.remainingTime() == doctest::Approx(0.2f));

        // Advance by 0.3s -> expired (total 2.1s >= 2.0s)
        toast.update(0.3f);
        CHECK_FALSE(toast.isVisible());
        CHECK(toast.remainingTime() == 0.0f);

        // Drawing after auto-dismiss draws nothing
        r.clearLogs();
        toast.draw(r, theme);
        CHECK(r.fillRects.empty());
        CHECK(r.drawTexts.empty());
    }
}
