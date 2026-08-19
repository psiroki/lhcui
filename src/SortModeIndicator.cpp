#include "hui/SortModeIndicator.h"

namespace hui {

SortModeIndicator::SortModeIndicator(std::string mode)
    : mode_(std::move(mode)) {}

void SortModeIndicator::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    // Badge background
    renderer.fillRect(bounds_, theme.surfaceAlt);
    renderer.drawRect(bounds_, theme.surface, 1);

    // Label text
    std::string text = "Sort: " + mode_;
    Size sz = renderer.measureText(text, theme.fontSmall);
    int textX = bounds_.x + (bounds_.w - sz.w) / 2;
    int textY = bounds_.y + (bounds_.h - sz.h) / 2;
    renderer.drawText(text, {textX, textY}, theme.fontSmall, isDisabled() ? theme.textDisabled : theme.textSecondary);
}

} // namespace hui
