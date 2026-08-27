#include "hui/ListHeaderWidget.h"
#include "hui/Helpers.h"
#include <algorithm>

namespace hui {

void ListHeaderWidget::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    // Header background fill
    renderer.fillRect(bounds_, theme.surface);

    const int pad = 8;
    int currentLeft = bounds_.x + pad;
    int currentRight = bounds_.x + bounds_.w - pad;

    // Optional icon on the left
    if (icon_ != 0) {
        Size iconSz = renderer.textureSize(icon_);
        if (iconSz.w > 0 && iconSz.h > 0) {
            int iconY = bounds_.y + (bounds_.h - iconSz.h) / 2;
            renderer.drawTexture(icon_, {currentLeft, iconY, iconSz.w, iconSz.h});
            currentLeft += iconSz.w + pad;
        }
    }

    // Optional sort badge on the far right
    if (!sortBadge_.empty()) {
        std::string badgeText = "Sort: " + sortBadge_;
        Size badgeSz = renderer.measureText(badgeText, theme.fontSmall);
        int badgeW = badgeSz.w + 12;
        int badgeH = std::max(16, bounds_.h - 8);
        currentRight -= badgeW;
        int badgeY = bounds_.y + (bounds_.h - badgeH) / 2;

        renderer.fillRect({currentRight, badgeY, badgeW, badgeH}, theme.surfaceAlt);
        renderer.drawRect({currentRight, badgeY, badgeW, badgeH}, theme.surface, 1);
        int textY = bounds_.y + (bounds_.h - badgeSz.h) / 2;
        renderer.drawText(badgeText, {currentRight + 6, textY}, theme.fontSmall, theme.textSecondary);
        currentRight -= pad;
    }

    // Optional item count before the sort badge
    if (itemCount_ >= 0) {
        std::string countStr = std::to_string(itemCount_) + (itemCount_ == 1 ? " item" : " items");
        Size countSz = renderer.measureText(countStr, theme.fontSmall);
        currentRight -= countSz.w;
        int textY = bounds_.y + (bounds_.h - countSz.h) / 2;
        renderer.drawText(countStr, {currentRight, textY}, theme.fontSmall, theme.textSecondary);
        currentRight -= pad;
    }

    // Left-truncated label in the remaining middle area
    int availableWidth = std::max(0, currentRight - currentLeft);
    if (availableWidth > 0 && !label_.empty()) {
        std::string truncated = leftTruncate(label_, theme.fontSmall, availableWidth, renderer);
        Size labelSz = renderer.measureText(truncated, theme.fontSmall);
        int textY = bounds_.y + (bounds_.h - labelSz.h) / 2;
        renderer.drawText(truncated, {currentLeft, textY}, theme.fontSmall, theme.textPrimary);
    }
}

} // namespace hui
