#include "doctest.h"
#include "hui/types.h"
#include "hui/IRenderer.h"
#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/FocusManager.h"
#include "hui/Shell.h"
#include "hui/DirectoryView.h"
#include "hui/LibraryView.h"
#include "hui/NowPlayingView.h"
#include "hui/ContextMenuView.h"
#include "hui/ConfirmationDialogView.h"
#include "hui/ListSource.h"
#include "hui/HintBarWidget.h"

#include <memory>
#include <string>
#include <vector>
#include <algorithm>

using namespace hui;

namespace {

class ScreenTestRenderer : public IRenderer {
public:
    int fullScreenFills = 0;

    void beginFrame() override {}
    void endFrame() override {}

    void pushClip(Rect) override {}
    void popClip() override {}

    void fillRect(Rect rect, Color) override {
        if (rect.w >= 640 && rect.h >= 480) {
            ++fullScreenFills;
        }
    }
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

    void invalidateTextCache() override {}
};

Theme createTestTheme() {
    Theme theme{};
    theme.textPrimary = {240, 240, 240, 255};
    theme.textSecondary = {150, 160, 180, 255};
    theme.surface = {28, 32, 42, 255};
    theme.surfaceAlt = {38, 44, 58, 255};
    theme.accent = {70, 160, 245, 255};
    theme.warning = {245, 80, 80, 255};
    theme.overlay = {0, 0, 0, 175};
    theme.focusBorderColor = {90, 175, 255, 255};
    theme.focusBorderWidth = 2;
    theme.focusFillColor = {35, 55, 85, 255};
    return theme;
}

class StaticListSource : public IListSource {
public:
    explicit StaticListSource(int count) {
        for (int i = 0; i < count; ++i) {
            entries_.push_back({
                "Item " + std::to_string(i),
                "Meta " + std::to_string(i),
                {},
                ListItemVariant::Track
            });
        }
    }

    int rowCount() const override { return static_cast<int>(entries_.size()); }

    void rowAt(int index, RowData& out) const override {
        const auto& entry = entries_[static_cast<size_t>(index)];
        out.primary = entry.primary;
        out.secondary = entry.secondary;
        out.variant = entry.variant;
    }

private:
    struct Entry {
        std::string primary;
        std::string secondary;
        std::string rightMeta;
        ListItemVariant variant;
    };
    std::vector<Entry> entries_;
};

class TrackingView : public View {
public:
    void layout(Rect contentRect) override { bounds_ = contentRect; }
    void draw(IRenderer&, const Theme&) override {}
    Rect lastLayoutBounds() const { return bounds_; }
};

void checkShellLayout(Shell& shell, ViewStack& stack, int screenW, int screenH) {
    shell.layout({0, 0, screenW, screenH});

    const Rect content = shell.contentRect();
    CHECK(content.y == Shell::kStatusBarHeight);
    CHECK(content.h == screenH - Shell::kStatusBarHeight - Shell::kHintBarHeight);
    CHECK(content.w == screenW);
    CHECK(content.x == 0);

    CHECK(shell.statusBar().bounds().h == Shell::kStatusBarHeight);
    CHECK(shell.hintBar().bounds().y == screenH - Shell::kHintBarHeight);
    CHECK(shell.hintBar().bounds().h == Shell::kHintBarHeight);

    CHECK(stack.contentRect().h == content.h);
    CHECK(stack.contentRect().y == content.y);

    const Rect status = shell.statusBar().bounds();
    const Rect hint = shell.hintBar().bounds();
    CHECK(content.y >= status.y + status.h);
    CHECK(content.y + content.h <= hint.y);
}

} // namespace

TEST_CASE("Shell content rect avoids status and hint bars") {
    FocusManager fm;
    ViewStack stack(&fm);
    Shell shell(stack);

    checkShellLayout(shell, stack, 480, 320);
    checkShellLayout(shell, stack, 640, 480);
}

TEST_CASE("Shell layout propagates content rect to stacked views") {
    FocusManager fm;
    ViewStack stack(&fm);
    Shell shell(stack);
    shell.layout({0, 0, 640, 480});

    auto view = std::make_unique<TrackingView>();
    auto* raw = view.get();
    stack.push(std::move(view));
    stack.applyPendingMutations(fm);

    CHECK(raw->lastLayoutBounds().h == shell.contentRect().h);
    CHECK(raw->lastLayoutBounds().y == shell.contentRect().y);

    shell.setTabBarVisible(true);
    CHECK(raw->lastLayoutBounds().h == shell.contentRect().h);
    CHECK(shell.tabBar().bounds().h == Shell::kTabBarHeight);
}

TEST_CASE("Shell toast replacement policy") {
    FocusManager fm;
    ViewStack stack(&fm);
    Shell shell(stack);
    shell.layout({0, 0, 640, 480});

    shell.showToast("First", 5.0f);
    CHECK(shell.toast().isVisible());
    CHECK(shell.toast().message() == "First");

    shell.showToast("Second", 5.0f);
    CHECK(shell.toast().message() == "Second");
}

TEST_CASE("Shell hint bar tracks overlay hints") {
    FocusManager fm;
    ViewStack stack(&fm);
    Shell shell(stack);
    shell.layout({0, 0, 640, 480});

    class HintView : public View {
    public:
        explicit HintView(std::vector<HintEntry> hints) : hints_(std::move(hints)) {}
        void draw(IRenderer&, const Theme&) override {}
        std::vector<HintEntry> currentHints() const override { return hints_; }
    private:
        std::vector<HintEntry> hints_;
    };

    stack.push(std::make_unique<HintView>(std::vector<HintEntry>{{"A", "BaseAction", false, 1}}));
    stack.applyPendingMutations(fm);

    ScreenTestRenderer r;
    Theme theme = createTestTheme();
    shell.drawChrome(r, theme);

    stack.push(std::make_unique<HintView>(std::vector<HintEntry>{{"X", "OverlayAction", false, 1}}));
    stack.applyPendingMutations(fm);

    class TextCaptureRenderer : public ScreenTestRenderer {
    public:
        std::vector<std::string> texts;
        int drawText(std::string_view text, Point, FontHandle, Color) override {
            texts.push_back(std::string(text));
            return ScreenTestRenderer::drawText(text, {}, 0, {});
        }
    };

    TextCaptureRenderer capture;
    shell.drawChrome(capture, theme);
    bool foundOverlay = false;
    bool foundBase = false;
    for (const auto& text : capture.texts) {
        if (text == "OverlayAction") foundOverlay = true;
        if (text == "BaseAction") foundBase = true;
    }
    CHECK(foundOverlay);
    CHECK_FALSE(foundBase);

    stack.pop();
    stack.applyPendingMutations(fm);

    capture.texts.clear();
    shell.drawChrome(capture, theme);
    foundBase = false;
    for (const auto& text : capture.texts) {
        if (text == "BaseAction") foundBase = true;
    }
    CHECK(foundBase);
}

TEST_CASE("Shell chrome stays undimmed during ordinary navigation") {
    FocusManager fm;
    ViewStack stack(&fm);
    Shell shell(stack);
    shell.layout({0, 0, 640, 480});

    stack.push(std::make_unique<TrackingView>());
    stack.applyPendingMutations(fm);

    ScreenTestRenderer r;
    Theme theme = createTestTheme();
    shell.drawChrome(r, theme);
    stack.draw(r, theme);
    CHECK(r.fullScreenFills == 0);
}

TEST_CASE("HintBarWidget sorts hints by sortOrder") {
    HintBarWidget hintBar;
    hintBar.layout({0, 450, 640, 30});
    hintBar.setHints({
        {"B", "Back", false, 100},
        {"Y", "Options", false, 30},
        {"A", "Select", false, 1},
        {"X", "Menu", false, 10},
    });

    class TextCaptureRenderer : public ScreenTestRenderer {
    public:
        std::vector<std::string> buttonLabels;
        int drawText(std::string_view text, Point, FontHandle, Color) override {
            const std::string label(text);
            if (label == "A" || label == "B" || label == "X" || label == "Y") {
                buttonLabels.push_back(label);
            }
            return ScreenTestRenderer::drawText(text, {}, 0, {});
        }
    };

    TextCaptureRenderer r;
    Theme theme = createTestTheme();
    hintBar.draw(r, theme);

    REQUIRE(r.buttonLabels.size() == 4);
    CHECK(r.buttonLabels[0] == "A");
    CHECK(r.buttonLabels[1] == "X");
    CHECK(r.buttonLabels[2] == "Y");
    CHECK(r.buttonLabels[3] == "B");
}

TEST_CASE("Shell hint bar shows ContextMenuView hints when menu is open") {
    FocusManager fm;
    ViewStack stack(&fm);
    Shell shell(stack);
    shell.layout({0, 0, 640, 480});

    stack.push(std::make_unique<DirectoryView>(stack));
    stack.applyPendingMutations(fm);
    stack.push(std::make_unique<ContextMenuView>(stack));
    stack.applyPendingMutations(fm);

    class TextCaptureRenderer : public ScreenTestRenderer {
    public:
        std::vector<std::string> texts;
        int drawText(std::string_view text, Point, FontHandle, Color) override {
            texts.push_back(std::string(text));
            return ScreenTestRenderer::drawText(text, {}, 0, {});
        }
    };

    TextCaptureRenderer capture;
    Theme theme = createTestTheme();
    shell.drawChrome(capture, theme);

    bool foundSelect = false;
    bool foundCancel = false;
    for (const auto& text : capture.texts) {
        if (text == "Select") foundSelect = true;
        if (text == "Cancel") foundCancel = true;
    }
    CHECK(foundSelect);
    CHECK(foundCancel);
}

TEST_CASE("Confirmation dialog dims chrome beneath stack") {
    FocusManager fm;
    ViewStack stack(&fm);
    Shell shell(stack);
    shell.layout({0, 0, 640, 480});

    stack.push(std::make_unique<TrackingView>());
    stack.applyPendingMutations(fm);
    stack.push(std::make_unique<ConfirmationDialogView>(stack));
    stack.applyPendingMutations(fm);

    ScreenTestRenderer r;
    Theme theme = createTestTheme();
    shell.drawChrome(r, theme);
    stack.draw(r, theme);
    CHECK(r.fullScreenFills == 1);

    shell.drawOverlay(r, theme);
    CHECK(shell.toast().isVisible() == false);
    shell.showToast("Still visible", 2.0f);
    shell.drawOverlay(r, theme);
    CHECK(shell.toast().isVisible());
}

TEST_CASE("DirectoryView context menu cancel restores list focus index") {
    FocusManager fm;
    ViewStack stack(&fm);
    Shell shell(stack);
    shell.layout({0, 0, 640, 480});

    StaticListSource source(8);
    auto dir = std::make_unique<DirectoryView>(stack, &shell);
    dir->setSource(&source);
    dir->layout(stack.contentRect());
    auto* dirPtr = dir.get();
    stack.push(std::move(dir));
    stack.applyPendingMutations(fm);

    dirPtr->list().setFocusIndex(4);
    fm.setFocus(&dirPtr->list());
    const int before = dirPtr->list().getFocusIndex();

    dirPtr->onButtonDown(Button::X, fm);
    stack.applyPendingMutations(fm);
    CHECK(stack.top()->isType<ContextMenuView>());

    stack.top()->onButtonDown(Button::B, fm);
    stack.applyPendingMutations(fm);

    CHECK(dirPtr->list().getFocusIndex() == before);
    CHECK(fm.hasFocus(&dirPtr->list()));
}

TEST_CASE("DirectoryView empty source renders without crash") {
    FocusManager fm;
    ViewStack stack(&fm);
    StaticListSource source(0);

    DirectoryView view(stack);
    view.setSource(&source);
    view.layout({0, 0, 640, 400});

    ScreenTestRenderer r;
    Theme theme = createTestTheme();
    view.draw(r, theme);
    CHECK(source.rowCount() == 0);
}

TEST_CASE("LibraryView tab switch preserves independent focus memory") {
    FocusManager fm;
    ViewStack stack(&fm);
    StaticListSource listSource(10);
    StaticListSource gridSource(12);

    LibraryView view(stack);
    view.setListSource(&listSource);
    view.setGridSource(&gridSource);
    view.layout({0, 0, 640, 400});

    view.list().setFocusIndex(3);
    view.grid().setFocusIndex(7);

    view.tabBar().onButtonDown(Button::R1);
    CHECK(view.activeTab() == 1);
    CHECK(view.grid().getFocusIndex() == 7);

    fm.setFocus(&view.grid());
    view.onButtonDown(Button::Down, fm);
    CHECK(view.grid().getFocusIndex() == 10);
    CHECK(view.list().getFocusIndex() == 3);

    view.tabBar().onButtonDown(Button::L1);
    CHECK(view.activeTab() == 0);
    CHECK(view.list().getFocusIndex() == 3);
}

TEST_CASE("LibraryView empty sources render without crash") {
    FocusManager fm;
    ViewStack stack(&fm);
    StaticListSource listSource(0);
    StaticListSource gridSource(0);

    LibraryView view(stack);
    view.setListSource(&listSource);
    view.setGridSource(&gridSource);
    view.layout({0, 0, 640, 400});

    ScreenTestRenderer r;
    Theme theme = createTestTheme();
    view.draw(r, theme);

    view.tabBar().onButtonDown(Button::R1);
    view.draw(r, theme);
}

TEST_CASE("NowPlayingView L2 R2 invoke onSeek") {
    FocusManager fm;
    ViewStack stack(&fm);
    StaticListSource queueSource(3);

    NowPlayingView view(stack);
    view.setQueueSource(&queueSource);
    view.layout({0, 0, 640, 400});
    fm.setFocus(&view.queue());

    int seekDirection = 0;
    view.setOnSeek([&](int direction) { seekDirection = direction; });

    view.onButtonDown(Button::L2, fm);
    CHECK(seekDirection == -1);

    view.onButtonDown(Button::R2, fm);
    CHECK(seekDirection == 1);
}

TEST_CASE("NowPlayingView transport reflects external playback state") {
    FocusManager fm;
    ViewStack stack(&fm);
    StaticListSource queueSource(2);

    NowPlayingView view(stack);
    view.setQueueSource(&queueSource);
    view.layout({0, 0, 640, 400});

    view.setPlaybackState(PlaybackState::Playing);
    CHECK(view.transport().playbackState() == PlaybackState::Playing);

    view.setPlaybackState(PlaybackState::Paused);
    CHECK(view.transport().playbackState() == PlaybackState::Paused);
}

TEST_CASE("NowPlayingView transport draw reflects playback state changes") {
    FocusManager fm;
    ViewStack stack(&fm);
    StaticListSource queueSource(1);

    NowPlayingView view(stack);
    view.setQueueSource(&queueSource);
    view.layout({0, 0, 640, 400});

    class TextCaptureRenderer : public ScreenTestRenderer {
    public:
        std::vector<std::string> texts;
        int drawText(std::string_view text, Point, FontHandle, Color) override {
            texts.push_back(std::string(text));
            return ScreenTestRenderer::drawText(text, {}, 0, {});
        }
    };

    TextCaptureRenderer r;
    Theme theme = createTestTheme();

    view.setPlaybackState(PlaybackState::Playing);
    r.texts.clear();
    view.draw(r, theme);
    CHECK(std::find(r.texts.begin(), r.texts.end(), "||") != r.texts.end());

    view.setPlaybackState(PlaybackState::Paused);
    r.texts.clear();
    view.draw(r, theme);
    CHECK(std::find(r.texts.begin(), r.texts.end(), "|>") != r.texts.end());
}

TEST_CASE("NowPlayingView empty queue renders without crash") {
    FocusManager fm;
    ViewStack stack(&fm);
    StaticListSource queueSource(0);

    NowPlayingView view(stack);
    view.setQueueSource(&queueSource);
    view.layout({0, 0, 640, 400});

    ScreenTestRenderer r;
    Theme theme = createTestTheme();
    view.draw(r, theme);
}
