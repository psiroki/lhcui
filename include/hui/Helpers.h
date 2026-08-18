#pragma once

#include "hui/types.h"
#include "hui/IRenderer.h"
#include <string>
#include <string_view>
#include <cstdint>

namespace hui {

// 13.1 Left-truncation (for filesystem paths)
// Returns "…/rest/of/path" where "rest/of/path" fits within maxWidth.
// Preserves the rightmost path component intact.
std::string leftTruncate(std::string_view text,
                         FontHandle font,
                         int maxWidth,
                         IRenderer& r);

// 13.2 Gradient Placeholder Thumbnails
// HSV with S=0.5, V=0.7, A=255.
Color hueToColor(float hue);

// Deterministic 32-bit hash of a label string.
uint32_t labelHash(std::string_view label);

// 13.5 Button Color Coding
// Returns standard colors for "A", "B", "X", "Y", or theme.textSecondary for others.
Color buttonGlyphColor(std::string_view buttonLabel, const Theme& theme);

} // namespace hui
