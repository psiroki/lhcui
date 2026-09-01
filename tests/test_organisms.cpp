#include "doctest.h"
#include "hui/types.h"
#include "hui/IRenderer.h"
#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/FocusManager.h"
#include "hui/ListView.h"
#include "hui/GridView.h"
#include "hui/TabBarWidget.h"
#include "hui/QueueList.h"
#include "hui/OnScreenKeyboard.h"
#include "hui/ContextMenuView.h"
#include "hui/ConfirmationDialogView.h"
#include "hui/GuideOverlayView.h"
#include "hui/PlaybackControlsRow.h"
#include "hui/SeekableProgressBar.h"
#include "hui/ListSource.h"

#include <vector>
#include <string>
#include <memory>

using namespace hui;

namespace {

class OrganismTestRenderer : public IRenderer {
public:
    int pushClipCount = 0;
    int popClipCount = 0;
    int fillRectCount = 0;
    int invalidateCacheCount = 0;

    void beginFrame() override {}
    void endFrame() override {}

    void pushClip(Rect) override { ++pushClipCount; }
    void popClip() override { ++popClipCount; }

    void fillRect(Rect, Color) override { ++fillRectCount; }
    void drawRect(Rect, Color, int = 1) override {}
    void drawLine(Point, Point, Color) override {}

    int drawText(std::string_view, Point, FontHandle, Color) override { return 8; }
    Size measureText(std::string_view text, FontHandle) override {
        return {static_cast<int>(text.size()) * 8, 16};
    }
    void drawTextEllipsis(std::string_view, Point, FontHandle, Color, int) override {}

    TextureHandle loadTexture(std::string_view) override { return 0; }
    void freeTexture(TextureHandle) override {}
    Size textureSize(TextureHandle) override { return {16, 16}; }
    void drawTexture(TextureHandle, Rect, uint8_t) override {}
    void setGlobalAlpha(uint8_t) override {}
    Size screenSize() const override { return {640, 480}; }

    void invalidateTextCache() override { ++invalidateCacheCount; }
};

class CountingListItemSource : public IListSource {
public:
    explicit CountingListItemSource(int count) : count_(count) {}

    int rowCount() const override { return count_; }

    void rowAt(int index, RowData& out) const override {
        static thread_local std::string primary;
        static thread_local std::string secondary;
        primary = "Row " + std::to_string(index);
        secondary = "Secondary " + std::to_string(index);
        out.primary = primary;
        out.secondary = secondary;
        out.variant = ListItemVariant::Track;
    }

private:
    int count_;
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
    t.overlay = {0, 0, 0, 180};
    t.focusBorderColor = {0, 180, 255, 255};
    t.focusBorderWidth = 2;
    t.focusFillColor = {0, 150, 255, 40};
    t.fontBody = 1;
    t.fontSmall = 2;
    return t;
}

} // namespace

TEST_SUITE("Phase 12 - Organisms") {

    TEST_CASE("PlaybackControlsRow focusable with five segments and TransportAction") {
        PlaybackControlsRow controls;
        CHECK(controls.isFocusable());

        FocusManager fm;
        fm.setFocus(&controls);

        std::vector<TransportAction> actions;
        controls.setOnActivate([&](TransportAction a) { actions.push_back(a); });

        CHECK(controls.onButtonDown(Button::Up) == false);
        CHECK(controls.onButtonDown(Button::Down) == false);

        // Wrap left from default segment 1: Left -> Previous, Left again -> Repeat
        CHECK(controls.onButtonDown(Button::Left));
        CHECK(controls.onButtonDown(Button::A));
        REQUIRE(actions.size() == 1);
        CHECK(actions.back() == TransportAction::Previous);

        actions.clear();
        CHECK(controls.onButtonDown(Button::Left));
        CHECK(controls.onButtonDown(Button::A));
        REQUIRE(actions.size() == 1);
        CHECK(actions.back() == TransportAction::Repeat);

        actions.clear();
        CHECK(controls.onButtonDown(Button::Right));
        CHECK(controls.onButtonDown(Button::Right));
        CHECK(controls.onButtonDown(Button::A));
        REQUIRE(actions.size() == 1);
        CHECK(actions.back() == TransportAction::PlayPause);
    }

    TEST_CASE("PlaybackControlsRow A fires each TransportAction once") {
        PlaybackControlsRow controls;
        FocusManager fm;
        fm.setFocus(&controls);
        controls.onButtonDown(Button::Left); // move to segment 0 (Previous)

        const TransportAction expected[] = {
            TransportAction::Previous,
            TransportAction::PlayPause,
            TransportAction::Next,
            TransportAction::Shuffle,
            TransportAction::Repeat
        };

        std::vector<TransportAction> actions;
        controls.setOnActivate([&](TransportAction a) { actions.push_back(a); });

        for (int seg = 0; seg < 5; ++seg) {
            if (seg > 0) {
                controls.onButtonDown(Button::Right);
            }
            controls.onButtonDown(Button::A);
        }

        REQUIRE(actions.size() == 5);
        for (int i = 0; i < 5; ++i) {
            CHECK(actions[i] == expected[i]);
        }
    }

    TEST_CASE("PlaybackControlsRow retains selected segment across blur") {
        PlaybackControlsRow controls;
        FocusManager fm;
        fm.setFocus(&controls);

        controls.onButtonDown(Button::Right);
        controls.onButtonDown(Button::Right);

        fm.setFocus(nullptr);
        fm.setFocus(&controls);

        std::vector<TransportAction> actions;
        controls.setOnActivate([&](TransportAction a) { actions.push_back(a); });
        controls.onButtonDown(Button::A);
        REQUIRE(actions.size() == 1);
        CHECK(actions.back() == TransportAction::Shuffle);
    }

    TEST_CASE("PlaybackState has exactly two enumerators") {
        CHECK(static_cast<int>(PlaybackState::Paused) == 0);
        CHECK(static_cast<int>(PlaybackState::Playing) == 1);
    }

    TEST_CASE("SeekableProgressBar Left/Right when focused and L2/R2 always") {
        SeekableProgressBar bar;
        FocusManager fm;

        int seekDir = 0;
        int seekCalls = 0;
        bar.setOnSeek([&](int dir) {
            seekDir = dir;
            ++seekCalls;
        });

        CHECK_FALSE(bar.onButtonDown(Button::Left));

        fm.setFocus(&bar);
        CHECK(bar.onButtonDown(Button::Left));
        CHECK(seekDir == -1);
        CHECK(bar.onButtonDown(Button::Right));
        CHECK(seekDir == 1);
        CHECK_FALSE(bar.onButtonDown(Button::Up));

        fm.setFocus(nullptr);
        CHECK(bar.onButtonDown(Button::L2));
        CHECK(bar.onButtonDown(Button::R2));
    }

    TEST_CASE("SeekableProgressBar and PlaybackControlsRow no dead focus stops") {
        SeekableProgressBar bar;
        PlaybackControlsRow controls;
        FocusManager fm;

        fm.setFocus(&bar);
        CHECK_FALSE(bar.onButtonDown(Button::Up));
        CHECK_FALSE(bar.onButtonDown(Button::Down));
        CHECK(bar.onButtonDown(Button::Left));
        CHECK(bar.onButtonDown(Button::Right));

        fm.setFocus(&controls);
        CHECK_FALSE(controls.onButtonDown(Button::Up));
        CHECK_FALSE(controls.onButtonDown(Button::Down));
        CHECK(controls.onButtonDown(Button::Left));
        CHECK(controls.onButtonDown(Button::Right));
    }

    TEST_CASE("ListView navigation wrap and focus memory") {
        CountingListItemSource source(20);
        ListView list(20);
        list.setSource(&source);
        list.layout({0, 0, 200, 100});

        for (int i = 0; i < 19; ++i) {
            list.onButtonDown(Button::Down);
        }
        CHECK(list.getFocusIndex() == 19);
        list.onButtonDown(Button::Down);
        CHECK(list.getFocusIndex() == 0);

        list.setFocusIndex(10);
        int scrollBeforeBlur = list.scrollOffset();
        int beforeBlur = list.getFocusIndex();
        FocusManager fm;
        fm.setFocus(&list);
        fm.setFocus(nullptr);
        fm.setFocus(&list);
        CHECK(list.getFocusIndex() == beforeBlur);
        CHECK(list.scrollOffset() == scrollBeforeBlur);
    }

    TEST_CASE("ListView R1 page jump and scroll-to-focus") {
        CountingListItemSource source(50);
        ListView list(20);
        list.setSource(&source);
        list.layout({0, 0, 200, 100});

        list.setFocusIndex(0);
        int scrollStart = list.scrollOffset();
        for (int i = 0; i < list.pageRows() + 2; ++i) {
            list.onButtonDown(Button::Down);
        }
        CHECK(list.scrollOffset() > scrollStart);

        list.setFocusIndex(25);
        int before = list.getFocusIndex();
        list.onButtonDown(Button::R1);
        CHECK(list.getFocusIndex() == before + list.pageRows());
    }

    TEST_CASE("ListView short list keeps scrollOffset at zero") {
        CountingListItemSource source(3);
        ListView list(40);
        list.setSource(&source);
        list.layout({0, 0, 200, 200});
        list.setFocusIndex(2);
        CHECK(list.scrollOffset() == 0);
    }

    TEST_CASE("ListView notifyRowsChanged preserves focus index") {
        CountingListItemSource source(10);
        ListView list;
        list.setSource(&source);
        list.layout({0, 0, 200, 200});
        list.setFocusIndex(7, false);
        list.notifyRowsChanged();
        CHECK(list.getFocusIndex() == 7);
        list.setFocusIndex(3);
        CHECK(list.getFocusIndex() == 3);
    }

    TEST_CASE("ListView L1 R1 L2 R2 page and end jumps") {
        CountingListItemSource source(50);
        ListView list(20);
        list.setSource(&source);
        list.layout({0, 0, 200, 100});

        list.setFocusIndex(25);
        int before = list.getFocusIndex();
        list.onButtonDown(Button::L1);
        CHECK(list.getFocusIndex() == before - list.pageRows());

        list.onButtonDown(Button::L2);
        CHECK(list.getFocusIndex() == 0);
        CHECK(list.scrollOffset() == 0);

        list.onButtonDown(Button::R2);
        CHECK(list.getFocusIndex() == 49);
    }

    TEST_CASE("ListView zero items shows placeholder without clip") {
        CountingListItemSource source(0);
        ListView list;
        list.setSource(&source);
        list.setEmptyMessage("Nothing here");
        list.layout({0, 0, 200, 100});

        class TextRenderer : public OrganismTestRenderer {
        public:
            std::vector<std::string> texts;
            int drawText(std::string_view text, Point, FontHandle, Color) override {
                texts.push_back(std::string(text));
                return OrganismTestRenderer::drawText(text, {}, 0, {});
            }
        };

        TextRenderer r;
        Theme theme = createTestTheme();
        list.draw(r, theme);
        CHECK(r.pushClipCount == 0);
        REQUIRE_FALSE(r.texts.empty());
        CHECK(r.texts.front() == "Nothing here");
    }

    TEST_CASE("ListView windowed draw uses single clip") {
        CountingListItemSource source(5000);
        ListView list(20);
        list.setSource(&source);
        list.layout({0, 0, 200, 100});

        OrganismTestRenderer r;
        Theme theme = createTestTheme();
        list.draw(r, theme);

        CHECK(r.pushClipCount == 1);
        CHECK(r.popClipCount == 1);
    }

    TEST_CASE("ListView notifyRowsChanged invalidates cache not scroll") {
        CountingListItemSource source(5);
        ListView list;
        list.setSource(&source);
        list.layout({0, 0, 200, 100});

        OrganismTestRenderer r;
        Theme theme = createTestTheme();
        list.notifyRowsChanged();
        list.draw(r, theme);
        CHECK(r.invalidateCacheCount == 1);

        r.invalidateCacheCount = 0;
        list.setFocusIndex(2);
        list.draw(r, theme);
        CHECK(r.invalidateCacheCount == 0);
    }

    TEST_CASE("GridView 2D navigation and wrap") {
        CountingListItemSource source(12);
        GridView grid(60, 50, 3);
        grid.setSource(&source);
        grid.layout({0, 0, 180, 150 });

        grid.onButtonDown(Button::Right);
        CHECK(grid.getFocusIndex() == 1);
        grid.onButtonDown(Button::Down);
        CHECK(grid.getFocusIndex() == 4);

        grid.setFocusIndex(0);
        grid.onButtonDown(Button::Up);
        CHECK(grid.getFocusIndex() == 9);
    }

    TEST_CASE("TabBarWidget L1 R1 cycles tabs") {
        TabBarWidget tabs;
        tabs.setTabs({"Artists", "Albums", "Tracks"});
        tabs.layout({0, 0, 300, 32});

        int changed = -1;
        tabs.setOnTabChanged([&](int i) { changed = i; });

        tabs.onButtonDown(Button::R1);
        CHECK(tabs.selectedIndex() == 1);
        CHECK(changed == 1);
        CHECK_FALSE(tabs.onButtonDown(Button::Up));
    }

    TEST_CASE("QueueList grab mode") {
        VectorListSource source;
        for (int i = 0; i < 5; ++i) {
            source.add("Track " + std::to_string(i));
        }

        QueueList queue;
        queue.setSource(&source);
        queue.layout({0, 0, 200, 120});
        queue.setFocusIndex(2);

        queue.onButtonDown(Button::Y);
        CHECK(queue.isGrabMode());
        queue.onButtonDown(Button::Down);

        int from = -1, to = -1;
        queue.setOnReorder([&](int f, int t) { from = f; to = t; });
        queue.onButtonDown(Button::A);
        CHECK_FALSE(queue.isGrabMode());
        CHECK(from == 2);
        CHECK(to == 3);

        queue.setFocusIndex(2);
        queue.onButtonDown(Button::Y);
        queue.onButtonDown(Button::Down);
        queue.onButtonDown(Button::B);
        CHECK(queue.getFocusIndex() == 2);
    }

    TEST_CASE("ContextMenuView B dismisses and destructive color") {
        FocusManager fm;
        ViewStack stack(&fm);
        stack.setContentRect({0, 0, 640, 480});

        class EmptyBaseView : public View {
        public:
            void draw(IRenderer&, const Theme&) override {}
        };

        class ColorCaptureRenderer : public OrganismTestRenderer {
        public:
            std::vector<Color> textColors;
            int drawText(std::string_view, Point, FontHandle, Color color) override {
                textColors.push_back(color);
                return 8;
            }
            void drawTextEllipsis(std::string_view, Point, FontHandle, Color color, int) override {
                textColors.push_back(color);
            }
        };

        auto menu = std::make_unique<ContextMenuView>(stack);
        menu->source().add("Play");
        menu->source().add("Delete", {}, {}, ListItemVariant::Default, 0, false, false, true);
        ContextMenuView* menuPtr = menu.get();
        stack.push(std::make_unique<EmptyBaseView>());
        stack.push(std::move(menu));
        stack.applyPendingMutations(fm);

        ColorCaptureRenderer r;
        Theme theme = createTestTheme();
        stack.draw(r, theme);
        bool foundWarning = false;
        for (const auto& c : r.textColors) {
            if (c.r == theme.warning.r && c.g == theme.warning.g && c.b == theme.warning.b) {
                foundWarning = true;
            }
        }
        CHECK(foundWarning);

        bool cancelled = false;
        menuPtr->setOnCancel([&]() { cancelled = true; });
        menuPtr->onButtonDown(Button::B, fm);
        stack.applyPendingMutations(fm);
        CHECK(cancelled);
    }

    TEST_CASE("ContextMenuView overlay does not add full-screen fill") {
        FocusManager fm;
        ViewStack stack(&fm);
        stack.setContentRect({0, 0, 640, 480});

        class EmptyBaseView : public View {
        public:
            void draw(IRenderer&, const Theme&) override {}
        };

        stack.push(std::make_unique<EmptyBaseView>());
        stack.applyPendingMutations(fm);
        stack.push(std::make_unique<ContextMenuView>(stack));
        stack.applyPendingMutations(fm);

        class FillTracker : public OrganismTestRenderer {
        public:
            int fullScreen = 0;
            void fillRect(Rect rect, Color) override {
                OrganismTestRenderer::fillRect(rect, {});
                if (rect.w >= 640 && rect.h >= 480) {
                    ++fullScreen;
                }
            }
        };
        FillTracker tracker;
        Theme theme = createTestTheme();
        stack.draw(tracker, theme);
        CHECK(tracker.fullScreen == 1);
    }

    TEST_CASE("ConfirmationDialogView focus cancel no-wrap and B dismiss") {
        FocusManager fm;
        ViewStack stack(&fm);
        stack.setContentRect({0, 0, 640, 480});

        auto dialog = std::make_unique<ConfirmationDialogView>(stack, "Delete file?");
        ConfirmationDialogView* dlgPtr = dialog.get();
        stack.push(std::move(dialog));
        stack.applyPendingMutations(fm);

        CHECK(fm.focused() != nullptr);

        dlgPtr->onButtonDown(Button::Left, fm);
        dlgPtr->onButtonDown(Button::Left, fm);

        bool cancelled = false;
        dlgPtr->setOnCancel([&]() { cancelled = true; });
        dlgPtr->onButtonDown(Button::B, fm);
        stack.applyPendingMutations(fm);
        CHECK(cancelled);
    }

    TEST_CASE("ConfirmationDialogView confirm flow") {
        FocusManager fm;
        ViewStack stack(&fm);
        stack.setContentRect({0, 0, 640, 480});

        auto dialog = std::make_unique<ConfirmationDialogView>(stack, "Delete file?");
        ConfirmationDialogView* dlgPtr = dialog.get();
        stack.push(std::move(dialog));
        stack.applyPendingMutations(fm);

        bool confirmed = false;
        dlgPtr->setOnConfirm([&]() { confirmed = true; });
        dlgPtr->onButtonDown(Button::Right, fm);
        dlgPtr->onButtonDown(Button::A, fm);
        stack.applyPendingMutations(fm);
        CHECK(confirmed);
    }

    TEST_CASE("GuideOverlayView slider Left Right and instant open") {
        FocusManager fm;
        ViewStack stack(&fm);
        GuideOverlayView guide(stack, false);
        guide.layout({360, 0, 280, 480});
        fm.setFocus(&guide.masterVolumeSlider());

        int value = guide.masterVolumeSlider().value();
        guide.masterVolumeSlider().onButtonDown(Button::Right);
        CHECK(guide.masterVolumeSlider().value() == value + 5);

        guide.masterVolumeSlider().onButtonDown(Button::Left);
        CHECK(guide.masterVolumeSlider().value() == value);
    }

    TEST_CASE("GuideOverlayView instant open without animations") {
        FocusManager fm;
        ViewStack stack(&fm);
        stack.setContentRect({0, 0, 640, 480});

        auto guide = std::make_unique<GuideOverlayView>(stack, false);
        GuideOverlayView* guidePtr = guide.get();
        stack.push(std::move(guide));
        stack.applyPendingMutations(fm);

        guidePtr->update(0.016f, fm);
        CHECK(guidePtr->slideOffset() == doctest::Approx(0.0f));
    }

    TEST_CASE("GuideOverlayView reclaims focus and action Right is no-op") {
        FocusManager fm;
        ViewStack stack(&fm);
        stack.setContentRect({0, 0, 640, 480});

        class FocusStealerView : public View {
        public:
            void update(float, FocusManager& fm) override {
                if (fm.focused() != &slider_) {
                    fm.setFocus(&slider_);
                }
            }
            void draw(IRenderer&, const Theme&) override {}
        private:
            Slider slider_{0, 100, 50};
        };

        stack.push(std::make_unique<FocusStealerView>());
        stack.applyPendingMutations(fm);

        auto guide = std::make_unique<GuideOverlayView>(stack, false);
        GuideOverlayView* guidePtr = guide.get();
        stack.push(std::move(guide));
        stack.applyPendingMutations(fm);
        guidePtr->layout({360, 0, 280, 480});

        guidePtr->onButtonDown(Button::Down, fm);
        guidePtr->onButtonDown(Button::Down, fm);
        guidePtr->onButtonDown(Button::Down, fm);
        guidePtr->update(0.016f, fm);

        const int volumeBefore = guidePtr->masterVolumeSlider().value();
        CHECK(guidePtr->onButtonDown(Button::Right, fm));
        guidePtr->update(0.016f, fm);
        CHECK(guidePtr->masterVolumeSlider().value() == volumeBefore);
        CHECK(guidePtr->onButtonDown(Button::Right, fm));
        guidePtr->update(0.016f, fm);
        CHECK(guidePtr->masterVolumeSlider().value() == volumeBefore);
    }

    TEST_CASE("OnScreenKeyboard backspace commit and cancel") {
        OnScreenKeyboard keyboard;
        keyboard.layout({0, 0, 400, 240});
        keyboard.setText("ab");

        for (int i = 0; i < 3; ++i) {
            keyboard.onButtonDown(Button::Down);
        }
        for (int i = 0; i < 6; ++i) {
            keyboard.onButtonDown(Button::Right);
        }
        keyboard.onButtonDown(Button::A);
        CHECK(keyboard.text() == "a");

        std::string committed;
        bool cancelled = false;
        keyboard.setOnCommit([&](std::string s) { committed = std::move(s); });
        keyboard.setOnCancel([&]() { cancelled = true; });

        keyboard.onButtonDown(Button::Right);
        keyboard.onButtonDown(Button::Right);
        keyboard.onButtonDown(Button::A);
        CHECK(committed == "a");

        keyboard.setText("xy");
        keyboard.onButtonDown(Button::B);
        CHECK(cancelled);
        CHECK(keyboard.text() == "xy");
    }
}
