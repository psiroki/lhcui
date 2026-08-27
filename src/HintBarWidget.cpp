#include "hui/HintBarWidget.h"
#include "hui/Helpers.h"
#include "hui/View.h"
#include <algorithm>

namespace hui {

HintBarWidget::HintBarWidget(const ViewStack* stack)
    : stack_(stack) {}

void HintBarWidget::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    // Background bar
    renderer.fillRect(bounds_, theme.surface);
    renderer.drawLine({bounds_.x, bounds_.y}, {bounds_.x + bounds_.w, bounds_.y}, theme.surfaceAlt);

    // Retrieve hints
    std::vector<HintEntry> hints;
    if (useExplicitHints_) {
        hints = explicitHints_;
    } else if (stack_ && stack_->top()) {
        hints = stack_->top()->currentHints();
    }

    if (hints.empty()) {
        return;
    }

    // 1. Sort by sortOrder
    std::stable_sort(hints.begin(), hints.end(), [](const HintEntry& a, const HintEntry& b) {
        return a.sortOrder < b.sortOrder;
    });

    // 2. Cap at five visible hints, truncating from the middle
    while (hints.size() > 5) {
        hints.erase(hints.begin() + hints.size() / 2);
    }

    // 3. Render hints horizontally
    int currentX = bounds_.x + 12;
    const int itemSpacing = 16;
    const int glyphPadding = 4;

    for (const auto& hint : hints) {
        Color glyphColor = buttonGlyphColor(hint.buttonLabel, theme);
        Size btnSz = renderer.measureText(hint.buttonLabel, theme.fontSmall);

        // Draw button pill / background
        Rect btnRect{currentX, bounds_.y + (bounds_.h - btnSz.h - 4) / 2, btnSz.w + 8, btnSz.h + 4};
        renderer.fillRect(btnRect, theme.surfaceAlt);
        renderer.drawRect(btnRect, glyphColor, 1);

        // Draw button label in glyph color
        int btnTextX = btnRect.x + (btnRect.w - btnSz.w) / 2;
        int btnTextY = bounds_.y + (bounds_.h - btnSz.h) / 2;
        renderer.drawText(hint.buttonLabel, {btnTextX, btnTextY}, theme.fontSmall, glyphColor);

        currentX += btnRect.w + glyphPadding;

        // Draw action label
        Size actSz = renderer.measureText(hint.actionLabel, theme.fontSmall);
        int actTextY = bounds_.y + (bounds_.h - actSz.h) / 2;
        renderer.drawText(hint.actionLabel, {currentX, actTextY}, theme.fontSmall, theme.textPrimary);

        currentX += actSz.w + itemSpacing;
    }
}

} // namespace hui
