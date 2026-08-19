#include "doctest.h"
#include "hui/types.h"
#include "hui/IRenderer.h"
#include "hui/FocusManager.h"
#include "hui/ListSource.h"
#include "hui/ListItemWidget.h"
#include "hui/GridCellWidget.h"
#include "hui/ProgressBar.h"
#include "hui/Slider.h"
#include "hui/SortModeIndicator.h"
#include "hui/ShuffleToggle.h"
#include "hui/RepeatModeToggle.h"
#include "hui/Helpers.h"

#include <vector>
#include <string>
#include <string_view>

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

class InstrumentedRenderer : public IRenderer {
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
    Size textureSize(TextureHandle) override { return {32, 32}; }
    void drawTexture(TextureHandle, Rect, uint8_t) override {}
    void setGlobalAlpha(uint8_t) override {}
    Size screenSize() const override { return {640, 480}; }

private:
    void clearParameters() {
        drawRects.clear();
    }
};

Theme createTestTheme() {
    Theme t{};
    t.background       = {20, 22, 28, 255};
    t.surface          = {35, 38, 48, 255};
    t.surfaceAlt       = {45, 50, 65, 255};
    t.accent           = {80, 160, 240, 255};
    t.textPrimary      = {245, 245, 250, 255};
    t.textSecondary    = {160, 165, 180, 255};
    t.textDisabled     = {100, 105, 120, 255};
    t.warning          = {240, 90, 90, 255};
    t.success          = {90, 220, 120, 255};
    t.overlay          = {0, 0, 0, 180};
    t.focusBorderColor = {100, 180, 255, 255};
    t.focusBorderWidth = 2;
    t.focusFillColor   = {40, 60, 90, 255};
    t.fontBody         = 1;
    t.fontSmall        = 2;
    return t;
}

} // anonymous namespace

TEST_CASE("Phase 10 — VectorListSource") {
    VectorListSource source;
    CHECK(source.rowCount() == 0);

    source.add("Track 1", "Artist A", "3:45", ListItemVariant::Track);
    source.add("Folder X", "12 items", "", ListItemVariant::Folder);
    source.add("Playlist Y", "50 tracks", "", ListItemVariant::Playlist);

    CHECK(source.rowCount() == 3);

    RowData row{};
    source.rowAt(0, row);
    CHECK(row.primary == "Track 1");
    CHECK(row.secondary == "Artist A");
    CHECK(row.rightMeta == "3:45");
    CHECK(row.variant == ListItemVariant::Track);
    CHECK_FALSE(row.playing);
    CHECK_FALSE(row.disabled);

    source.rowAt(1, row);
    CHECK(row.primary == "Folder X");
    CHECK(row.variant == ListItemVariant::Folder);

    source.rowAt(2, row);
    CHECK(row.primary == "Playlist Y");
    CHECK(row.variant == ListItemVariant::Playlist);

    // Out-of-bounds check
    source.rowAt(99, row);
    CHECK(row.primary.empty());

    source.clear();
    CHECK(source.rowCount() == 0);
}

TEST_CASE("Phase 10 — ListItemWidget focusability and FocusManager refusal") {
    ListItemWidget item;
    CHECK_FALSE(item.isFocusable());

    FocusManager fm;
    bool focused = fm.setFocus(&item);
    CHECK_FALSE(focused);
    CHECK_FALSE(item.isFocused());
}

TEST_CASE("Phase 10 — ListItemWidget variants and states rendering") {
    InstrumentedRenderer renderer;
    Theme theme = createTestTheme();
    ListItemWidget item;
    item.layout({0, 0, 200, 40});

    // 1. Four variants rendering
    ListItemVariant variants[] = {
        ListItemVariant::Default,
        ListItemVariant::Track,
        ListItemVariant::Folder,
        ListItemVariant::Playlist
    };

    for (auto var : variants) {
        renderer.clearLogs();
        RowData row;
        row.primary = "Item Label";
        row.variant = var;
        item.setRow(row);
        item.setRowFocused(false);
        item.draw(renderer, theme);

        CHECK(renderer.drawTextEllipses.size() >= 1);
        CHECK(renderer.drawTextEllipses[0].text == "Item Label");
    }

    // 2. Focused state shows focus border and accent fill tint
    renderer.clearLogs();
    RowData row;
    row.primary = "Focused Row";
    item.setRow(row);
    item.setRowFocused(true);
    item.draw(renderer, theme);

    bool hasFocusFill = false;
    for (const auto& f : renderer.fillRects) {
        if (f.color.r == theme.focusFillColor.r && f.color.g == theme.focusFillColor.g && f.color.b == theme.focusFillColor.b) {
            hasFocusFill = true;
        }
    }
    CHECK(hasFocusFill);

    bool hasFocusBorder = false;
    for (const auto& b : renderer.drawRects) {
        if (b.color.r == theme.focusBorderColor.r && b.color.g == theme.focusBorderColor.g && b.color.b == theme.focusBorderColor.b) {
            hasFocusBorder = true;
        }
    }
    CHECK(hasFocusBorder);

    // 3. Disabled state renders in theme.textDisabled
    renderer.clearLogs();
    row.primary = "Disabled Row";
    row.disabled = true;
    item.setRow(row);
    item.setRowFocused(true); // Should not show focus highlight when disabled
    item.draw(renderer, theme);

    CHECK_FALSE(renderer.drawTextEllipses.empty());
    CHECK(renderer.drawTextEllipses[0].color.r == theme.textDisabled.r);
    CHECK(renderer.drawTextEllipses[0].color.g == theme.textDisabled.g);
    CHECK(renderer.drawTextEllipses[0].color.b == theme.textDisabled.b);

    // 4. Very long label uses drawTextEllipsis
    renderer.clearLogs();
    row.primary = "A very very long text string that exceeds width";
    row.disabled = false;
    item.setRow(row);
    item.setRowFocused(false);
    item.draw(renderer, theme);

    REQUIRE_FALSE(renderer.drawTextEllipses.empty());
    CHECK(renderer.drawTextEllipses[0].maxWidth <= 200);
}

TEST_CASE("Phase 10 — ListItemWidget stamp reuse without state leaking") {
    InstrumentedRenderer renderer;
    Theme theme = createTestTheme();
    ListItemWidget stamp;
    stamp.layout({0, 0, 300, 30});

    // 20 successive rows alternating playing / normal / disabled / focused
    for (int i = 0; i < 20; ++i) {
        renderer.clearLogs();
        RowData row;
        std::string name = "Row " + std::to_string(i);
        row.primary = name;
        row.playing = (i == 3);
        row.disabled = (i == 5);
        stamp.setRow(row);
        stamp.setRowFocused(i == 2);
        stamp.draw(renderer, theme);

        if (i == 4) {
            // Row 4 is normal, should NOT have accent color from row 3
            CHECK_FALSE(renderer.drawTextEllipses.empty());
            CHECK(renderer.drawTextEllipses[0].color.r == theme.textPrimary.r);
            CHECK(renderer.drawTextEllipses[0].color.g == theme.textPrimary.g);
            CHECK(renderer.drawTextEllipses[0].color.b == theme.textPrimary.b);
        }
    }
}

TEST_CASE("Phase 10 — GridCellWidget focusability and stamp rendering") {
    GridCellWidget cell;
    CHECK_FALSE(cell.isFocusable());

    FocusManager fm;
    CHECK_FALSE(fm.setFocus(&cell));

    InstrumentedRenderer renderer;
    Theme theme = createTestTheme();
    cell.layout({0, 0, 100, 100});

    // Null texture handle renders gradient placeholder (fills rects, not crash/black)
    renderer.clearLogs();
    cell.setCell("Album Alpha", "Artist A", 0, false, false);
    cell.setCellFocused(false);
    cell.draw(renderer, theme);
    CHECK(renderer.fillRects.size() > 1);

    // Two GridCellWidgets with different labels produce distinct top colors
    uint32_t hashA = labelHash("Album Alpha");
    uint32_t hashB = labelHash("Album Beta");
    CHECK(hashA != hashB);

    float hueA = static_cast<float>(hashA % 1000) / 1000.0f;
    float hueB = static_cast<float>(hashB % 1000) / 1000.0f;
    Color colA = hueToColor(hueA);
    Color colB = hueToColor(hueB);
    CHECK((colA.r != colB.r || colA.g != colB.g || colA.b != colB.b));

    // Focused state renders focus border
    renderer.clearLogs();
    cell.setCellFocused(true);
    cell.draw(renderer, theme);

    bool hasFocusBorder = false;
    for (const auto& b : renderer.drawRects) {
        if (b.color.r == theme.focusBorderColor.r && b.color.g == theme.focusBorderColor.g && b.color.b == theme.focusBorderColor.b) {
            hasFocusBorder = true;
        }
    }
    CHECK(hasFocusBorder);

    // Playing state renders playing badge
    renderer.clearLogs();
    cell.setCell("Album Alpha", "Artist A", 0, true, false);
    cell.draw(renderer, theme);

    bool hasPlayingBadge = false;
    for (const auto& t : renderer.drawTexts) {
        if (t.text == "▶") {
            hasPlayingBadge = true;
        }
    }
    CHECK(hasPlayingBadge);
}

TEST_CASE("Phase 10 — ProgressBar") {
    ProgressBar bar;
    CHECK_FALSE(bar.isFocusable());

    bar.layout({0, 0, 200, 24});
    bar.setTime(30.0f, 60.0f); // 50%
    CHECK(bar.progress() == doctest::Approx(0.5f));
    CHECK(bar.elapsedText() == "0:30");
    CHECK(bar.totalText() == "1:00");

    bar.setProgress(0.0f);
    CHECK(bar.progress() == 0.0f);

    bar.setProgress(1.0f);
    CHECK(bar.progress() == 1.0f);

    InstrumentedRenderer renderer;
    Theme theme = createTestTheme();
    bar.draw(renderer, theme);
    CHECK(renderer.fillRects.size() >= 2); // track + fill
}

TEST_CASE("Phase 10 — Slider input, callback, and disabled handling") {
    Slider slider(0, 100, 50, 5);
    CHECK(slider.isFocusable());

    FocusManager fm;
    CHECK(fm.setFocus(&slider));
    CHECK(slider.isFocused());

    int callbackValue = -1;
    slider.setOnValueChanged([&](int val) {
        callbackValue = val;
    });

    // Right increases by step (50 -> 55)
    bool consumed = slider.onButtonDown(Button::Right);
    CHECK(consumed);
    CHECK(slider.value() == 55);
    CHECK(callbackValue == 55);

    // Left decreases by step (55 -> 50)
    consumed = slider.onButtonDown(Button::Left);
    CHECK(consumed);
    CHECK(slider.value() == 50);
    CHECK(callbackValue == 50);

    // Clamp at max
    slider.setValue(100);
    consumed = slider.onButtonDown(Button::Right);
    CHECK(consumed);
    CHECK(slider.value() == 100);

    // Clamp at min
    slider.setValue(0);
    consumed = slider.onButtonDown(Button::Left);
    CHECK(consumed);
    CHECK(slider.value() == 0);

    // Disabled state ignores input
    slider.setDisabled(true);
    callbackValue = -1;
    consumed = slider.onButtonDown(Button::Right);
    CHECK_FALSE(consumed);
    CHECK(slider.value() == 0);
    CHECK(callbackValue == -1);
}

TEST_CASE("Phase 10 — RepeatModeToggle cycling") {
    RepeatModeToggle toggle;
    CHECK_FALSE(toggle.isFocusable());
    CHECK(toggle.mode() == RepeatMode::Off);

    // Cycle 1: Off -> All
    toggle.cycle();
    CHECK(toggle.mode() == RepeatMode::All);

    // Cycle 2: All -> One
    toggle.cycle();
    CHECK(toggle.mode() == RepeatMode::One);

    // Cycle 3: One -> Off
    toggle.cycle();
    CHECK(toggle.mode() == RepeatMode::Off);

    // Cycle 4: Off -> All (again)
    toggle.cycle();
    CHECK(toggle.mode() == RepeatMode::All);
}

TEST_CASE("Phase 10 — ShuffleToggle and SortModeIndicator") {
    ShuffleToggle shuffle;
    CHECK_FALSE(shuffle.isFocusable());
    CHECK_FALSE(shuffle.isShuffle());

    shuffle.toggle();
    CHECK(shuffle.isShuffle());
    shuffle.setShuffle(false);
    CHECK_FALSE(shuffle.isShuffle());

    SortModeIndicator sort;
    CHECK_FALSE(sort.isFocusable());
    CHECK(sort.mode() == "Default");
    sort.setMode("Title");
    CHECK(sort.mode() == "Title");
}
