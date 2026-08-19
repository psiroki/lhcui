#include "hui/GridCellWidget.h"
#include "hui/Helpers.h"
#include <algorithm>

namespace hui {

void GridCellWidget::setCell(std::string_view label,
                             std::string_view sublabel,
                             TextureHandle thumbnail,
                             bool playing,
                             bool disabled) {
    label_ = label;
    sublabel_ = sublabel;
    thumbnail_ = thumbnail;
    playing_ = playing;
    disabled_ = disabled;
    setDisabled(disabled);
}

void GridCellWidget::setRow(const RowData& data) {
    setCell(data.primary, data.secondary, data.icon, data.playing, data.disabled);
}

void GridCellWidget::setCellFocused(bool focused) {
    cellFocused_ = focused;
}

void GridCellWidget::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    const bool disabled = disabled_ || isDisabled();

    // 1. Tile card background
    Color bg = (cellFocused_ && !disabled) ? theme.focusFillColor : theme.surface;
    renderer.fillRect(bounds_, bg);

    const int pad = 4;
    int artH = std::max(10, bounds_.h - 36);
    Rect artRect{bounds_.x + pad, bounds_.y + pad, std::max(1, bounds_.w - 2 * pad), artH};

    // 2. Thumbnail or Gradient Placeholder
    if (thumbnail_ != 0) {
        renderer.drawTexture(thumbnail_, artRect, disabled ? 100 : 255);
    } else {
        // Deterministic gradient from label hash
        uint32_t hash = labelHash(label_);
        float hue = static_cast<float>(hash % 1000) / 1000.0f;
        Color topColor = hueToColor(hue);
        Color bottomColor = topColor.lerp(theme.background, 0.55f);

        if (disabled) {
            topColor = topColor.lerp(theme.textDisabled, 0.5f);
            bottomColor = bottomColor.lerp(theme.textDisabled, 0.7f);
        }

        // Draw 2-stop vertical gradient in strips
        int steps = std::max(1, artRect.h / 4);
        int bandH = (artRect.h + steps - 1) / steps;
        for (int i = 0; i < steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps > 1 ? steps - 1 : 1);
            Color bandColor = topColor.lerp(bottomColor, t);
            int y = artRect.y + i * bandH;
            int h = std::min(bandH, (artRect.y + artRect.h) - y);
            if (h > 0) {
                renderer.fillRect({artRect.x, y, artRect.w, h}, bandColor);
            }
        }
    }

    // 3. Playing badge
    if (playing_) {
        Rect badgeRect{artRect.x + artRect.w - 20, artRect.y + artRect.h - 16, 18, 14};
        renderer.fillRect(badgeRect, theme.accent);
        renderer.drawText("▶", {badgeRect.x + 4, badgeRect.y}, theme.fontSmall, theme.background);
    }

    // 4. Focus border
    if (cellFocused_ && !disabled) {
        renderer.drawRect(bounds_, theme.focusBorderColor, theme.focusBorderWidth);
    } else {
        renderer.drawRect(bounds_, theme.surfaceAlt, 1);
    }

    // 5. Label and Sublabel text
    int textY = artRect.y + artRect.h + 4;
    int textW = std::max(0, bounds_.w - 2 * pad);

    Color lblColor = disabled ? theme.textDisabled
                              : (playing_ ? theme.accent
                                          : (cellFocused_ ? theme.textPrimary : theme.textPrimary));
    Color subColor = disabled ? theme.textDisabled : theme.textSecondary;

    renderer.drawTextEllipsis(label_, {bounds_.x + pad, textY}, theme.fontSmall, lblColor, textW);

    if (!sublabel_.empty() && (bounds_.y + bounds_.h) >= (textY + 28)) {
        renderer.drawTextEllipsis(sublabel_, {bounds_.x + pad, textY + 14}, theme.fontSmall, subColor, textW);
    }
}

} // namespace hui
