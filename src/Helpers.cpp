#include "hui/Helpers.h"
#include <cmath>
#include <vector>

namespace hui {

std::string leftTruncate(std::string_view text,
                         FontHandle font,
                         int maxWidth,
                         IRenderer& r) {
    if (r.measureText(text, font).w <= maxWidth) {
        return std::string(text);
    }

    std::vector<size_t> slashPositions;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '/') {
            slashPositions.push_back(i);
        }
    }

    if (slashPositions.empty()) {
        return std::string(text);
    }

    std::string fallback;
    for (size_t idx = 0; idx < slashPositions.size(); ++idx) {
        size_t pos = slashPositions[idx];
        // Skip consecutive slashes
        while (pos + 1 < text.size() && text[pos + 1] == '/') {
            ++pos;
        }
        if (pos + 1 >= text.size() && idx > 0) {
            continue;
        }

        std::string_view suffix = text.substr(pos + 1);
        std::string candidate = "…/" + std::string(suffix);
        fallback = candidate;
        if (r.measureText(candidate, font).w <= maxWidth) {
            return candidate;
        }
    }

    return fallback.empty() ? std::string(text) : fallback;
}

Color hueToColor(float hue) {
    float h = hue - std::floor(hue);
    if (h < 0.0f) h += 1.0f;

    const float s = 0.5f;
    const float v = 0.7f;

    float h6 = h * 6.0f;
    int i = static_cast<int>(h6) % 6;
    float f = h6 - static_cast<float>(static_cast<int>(h6));

    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    float r = 0.0f, g = 0.0f, b = 0.0f;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: break;
    }

    return Color{
        static_cast<uint8_t>(std::round(r * 255.0f)),
        static_cast<uint8_t>(std::round(g * 255.0f)),
        static_cast<uint8_t>(std::round(b * 255.0f)),
        255
    };
}

uint32_t labelHash(std::string_view label) {
    uint32_t hash = 2166136261u;
    for (char c : label) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

Color buttonGlyphColor(std::string_view buttonLabel, const Theme& theme) {
    if (buttonLabel == "A") return Color{220,  50,  50, 255};
    if (buttonLabel == "B") return Color{220, 160,  40, 255};
    if (buttonLabel == "X") return Color{ 60, 120, 220, 255};
    if (buttonLabel == "Y") return Color{ 60, 180,  80, 255};
    return theme.textSecondary;
}

} // namespace hui
