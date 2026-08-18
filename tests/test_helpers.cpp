#include "doctest.h"
#include "hui/Helpers.h"
#include "hui/IRenderer.h"
#include "hui/types.h"

#include <string>
#include <string_view>

using namespace hui;

namespace {

class FixedCharWidthRenderer : public IRenderer {
public:
    int charWidth_ = 10;

    void beginFrame() override {}
    void endFrame() override {}
    void pushClip(Rect) override {}
    void popClip() override {}
    void fillRect(Rect, Color) override {}
    void drawRect(Rect, Color, int) override {}
    void drawLine(Point, Point, Color) override {}
    int drawText(std::string_view, Point, FontHandle, Color) override { return 0; }

    Size measureText(std::string_view text, FontHandle) override {
        // Count UTF-8 characters roughly or byte length:
        // "…" is 3 bytes (1 UTF-8 character) -> count as 1 character width
        int count = 0;
        for (size_t i = 0; i < text.size(); ) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            if ((c & 0x80) == 0) {
                i += 1;
            } else if ((c & 0xE0) == 0xC0) {
                i += 2;
            } else if ((c & 0xF0) == 0xE0) {
                i += 3;
            } else if ((c & 0xF8) == 0xF0) {
                i += 4;
            } else {
                i += 1;
            }
            ++count;
        }
        return { count * charWidth_, 16 };
    }

    void drawTextEllipsis(std::string_view, Point, FontHandle, Color, int) override {}
    TextureHandle loadTexture(std::string_view) override { return 0; }
    void freeTexture(TextureHandle) override {}
    Size textureSize(TextureHandle) override { return {0, 0}; }
    void drawTexture(TextureHandle, Rect, uint8_t) override {}
    void setGlobalAlpha(uint8_t) override {}
    Size screenSize() const override { return {640, 480}; }
};

} // anonymous namespace

TEST_CASE("Phase 9 — leftTruncate") {
    FixedCharWidthRenderer renderer;
    FontHandle font = 1;

    SUBCASE("String that already fits within maxWidth returns unchanged (no … prefix)") {
        std::string_view path = "/home/user/music.flac";
        int maxWidth = 250;

        std::string result = leftTruncate(path, font, maxWidth, renderer);
        CHECK(result == path);
    }

    SUBCASE("Long path returns a string beginning with …/ whose rendered pixel width <= maxWidth") {
        std::string_view longPath = "/home/user/music/rock/album/track.flac";
        // Candidates:
        // …/home/user/music/rock/album/track.flac (39 chars)
        // …/user/music/rock/album/track.flac (34 chars)
        // …/music/rock/album/track.flac (29 chars)
        // …/rock/album/track.flac (23 chars = 230 px)
        // …/album/track.flac (18 chars = 180 px)
        // …/track.flac (12 chars = 120 px)

        int maxWidth = 240; // allows 24 chars -> …/rock/album/track.flac (23 chars = 230 px)
        std::string result = leftTruncate(longPath, font, maxWidth, renderer);

        CHECK(result.rfind("…/", 0) == 0);
        CHECK(renderer.measureText(result, font).w <= maxWidth);
        CHECK(result == "…/rock/album/track.flac");
    }

    SUBCASE("Result always preserves rightmost path component intact") {
        std::string_view longPath = "/home/user/music/rock/album/track.flac";
        // Even with a very tight maxWidth that cannot fit …/track.flac (12 chars = 120px)
        int maxWidth = 50;
        std::string result = leftTruncate(longPath, font, maxWidth, renderer);

        CHECK(result == "…/track.flac");
    }

    SUBCASE("Path without slashes returns intact string") {
        std::string_view simple = "track.flac";
        int maxWidth = 200;
        CHECK(leftTruncate(simple, font, maxWidth, renderer) == "track.flac");

        int tinyWidth = 20;
        CHECK(leftTruncate(simple, font, tinyWidth, renderer) == "track.flac");
    }
}

TEST_CASE("Phase 9 — hueToColor") {
    SUBCASE("hueToColor(0.0f) returns recognisably red-tinted color") {
        Color red = hueToColor(0.0f);
        CHECK(red.r > red.g);
        CHECK(red.r > red.b);
        CHECK(red.a == 255);
    }

    SUBCASE("hueToColor(0.33f) returns recognisably green-tinted color") {
        Color green = hueToColor(0.33f);
        CHECK(green.g > green.r);
        CHECK(green.g > green.b);
        CHECK(green.a == 255);
    }

    SUBCASE("hueToColor(0.67f) returns recognisably blue-tinted color") {
        Color blue = hueToColor(0.67f);
        CHECK(blue.b > blue.r);
        CHECK(blue.b > blue.g);
        CHECK(blue.a == 255);
    }

    SUBCASE("hueToColor wraps properly") {
        Color c0 = hueToColor(0.0f);
        Color c1 = hueToColor(1.0f);
        CHECK(c0.r == c1.r);
        CHECK(c0.g == c1.g);
        CHECK(c0.b == c1.b);
        CHECK(c0.a == c1.a);
    }
}

TEST_CASE("Phase 9 — labelHash") {
    SUBCASE("labelHash is deterministic and discriminates different strings") {
        uint32_t h1 = labelHash("alpha");
        uint32_t h2 = labelHash("beta");
        uint32_t h3 = labelHash("alpha");

        CHECK(h1 != h2);
        CHECK(h1 == h3);
        CHECK(h1 != 0);
    }
}

TEST_CASE("Phase 9 — buttonGlyphColor") {
    Theme theme;
    theme.textSecondary = Color{128, 128, 128, 255};

    SUBCASE("Standard button labels") {
        Color colorA = buttonGlyphColor("A", theme);
        CHECK(colorA.r == 220);
        CHECK(colorA.g == 50);
        CHECK(colorA.b == 50);
        CHECK(colorA.a == 255);

        Color colorB = buttonGlyphColor("B", theme);
        CHECK(colorB.r == 220);
        CHECK(colorB.g == 160);
        CHECK(colorB.b == 40);
        CHECK(colorB.a == 255);

        Color colorX = buttonGlyphColor("X", theme);
        CHECK(colorX.r == 60);
        CHECK(colorX.g == 120);
        CHECK(colorX.b == 220);
        CHECK(colorX.a == 255);

        Color colorY = buttonGlyphColor("Y", theme);
        CHECK(colorY.r == 60);
        CHECK(colorY.g == 180);
        CHECK(colorY.b == 80);
        CHECK(colorY.a == 255);
    }

    SUBCASE("Other labels return theme.textSecondary") {
        Color colorStart = buttonGlyphColor("START", theme);
        CHECK(colorStart.r == theme.textSecondary.r);
        CHECK(colorStart.g == theme.textSecondary.g);
        CHECK(colorStart.b == theme.textSecondary.b);
        CHECK(colorStart.a == theme.textSecondary.a);

        Color colorL1 = buttonGlyphColor("L1/R1", theme);
        CHECK(colorL1.r == theme.textSecondary.r);
        CHECK(colorL1.g == theme.textSecondary.g);
        CHECK(colorL1.b == theme.textSecondary.b);
        CHECK(colorL1.a == theme.textSecondary.a);

        Color colorCustom = buttonGlyphColor("Select", theme);
        CHECK(colorCustom.r == theme.textSecondary.r);
        CHECK(colorCustom.g == theme.textSecondary.g);
        CHECK(colorCustom.b == theme.textSecondary.b);
        CHECK(colorCustom.a == theme.textSecondary.a);
    }
}
