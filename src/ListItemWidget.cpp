#include "hui/ListItemWidget.h"
#include <algorithm>

namespace hui {

void ListItemWidget::setRow(const RowData& data) {
    row_ = data;
    setDisabled(data.disabled);
}

void ListItemWidget::setRowFocused(bool focused) {
    rowFocused_ = focused;
}

void ListItemWidget::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    const bool disabled = row_.disabled || isDisabled();

    // 1. Focus background & border
    if (rowFocused_ && !disabled) {
        renderer.fillRect(bounds_, theme.focusFillColor);
        renderer.drawRect(bounds_, theme.focusBorderColor, theme.focusBorderWidth);
        // Accent indicator strip on the left edge
        renderer.fillRect({bounds_.x + 2, bounds_.y + 2, 3, bounds_.h - 4}, theme.accent);
    }

    const int padX = 8;
    int curX = bounds_.x + padX;

    // 2. Icon / Variant / Playing indicator
    if (row_.icon != 0) {
        int iconSize = std::min(bounds_.h - 6, 24);
        int iconY = bounds_.y + (bounds_.h - iconSize) / 2;
        renderer.drawTexture(row_.icon, {curX, iconY, iconSize, iconSize}, disabled ? 100 : 255);
        curX += iconSize + 6;
    } else {
        // Variant glyph / prefix tag
        std::string_view variantPrefix;
        switch (row_.variant) {
            case ListItemVariant::Track:
                variantPrefix = row_.playing ? "▶ " : "♪ ";
                break;
            case ListItemVariant::Folder:
                variantPrefix = "📁 ";
                break;
            case ListItemVariant::Playlist:
                variantPrefix = "≡ ";
                break;
            case ListItemVariant::Default:
            default:
                if (row_.playing) {
                    variantPrefix = "▶ ";
                }
                break;
        }

        if (!variantPrefix.empty()) {
            Color prefixColor = disabled ? theme.textDisabled : (row_.playing ? theme.accent : theme.textSecondary);
            int textY = bounds_.y + (bounds_.h - 16) / 2;
            int adv = renderer.drawText(variantPrefix, {curX, textY}, theme.fontSmall, prefixColor);
            curX += adv;
        }
    }

    // 3. Right Meta text
    int rightLimit = bounds_.x + bounds_.w - padX;
    Color metaColor = disabled ? theme.textDisabled : theme.textSecondary;

    if (!row_.rightMeta.empty()) {
        Size metaSize = renderer.measureText(row_.rightMeta, theme.fontSmall);
        int metaX = bounds_.x + bounds_.w - padX - metaSize.w;
        int metaY = bounds_.y + (bounds_.h - 14) / 2;
        if (metaX > curX + 20) {
            renderer.drawText(row_.rightMeta, {metaX, metaY}, theme.fontSmall, metaColor);
            rightLimit = metaX - 8;
        }
    }

    // 4. Primary and Secondary labels
    int availWidth = std::max(0, rightLimit - curX);
    if (availWidth <= 0) return;

    Color primaryColor = disabled ? theme.textDisabled
                                  : (row_.playing ? theme.accent
                                                  : (rowFocused_ ? theme.textPrimary : theme.textPrimary));
    Color secondaryColor = disabled ? theme.textDisabled : theme.textSecondary;

    if (!row_.secondary.empty() && bounds_.h >= 36) {
        // Two-line layout
        int primY = bounds_.y + 4;
        int secY = bounds_.y + bounds_.h - 16;
        renderer.drawTextEllipsis(row_.primary, {curX, primY}, theme.fontBody, primaryColor, availWidth);
        renderer.drawTextEllipsis(row_.secondary, {curX, secY}, theme.fontSmall, secondaryColor, availWidth);
    } else {
        // Single-line layout
        int primY = bounds_.y + (bounds_.h - 16) / 2;
        renderer.drawTextEllipsis(row_.primary, {curX, primY}, theme.fontBody, primaryColor, availWidth);
    }
}

} // namespace hui
