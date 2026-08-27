#include "hui/StatusBarWidget.h"
#include <cmath>
#include <algorithm>

namespace hui {

void StatusBarWidget::update(float dt) {
    if (nowPlaying_) {
        pulseTime_ += dt;
    }
}

void StatusBarWidget::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    // Top bar background
    renderer.fillRect(bounds_, theme.surface);
    renderer.drawLine({bounds_.x, bounds_.y + bounds_.h - 1}, {bounds_.x + bounds_.w, bounds_.y + bounds_.h - 1}, theme.surfaceAlt);

    const int pad = 8;
    int currentLeft = bounds_.x + pad;
    int currentRight = bounds_.x + bounds_.w - pad;

    // View mode (left)
    if (!viewMode_.empty()) {
        Size sz = renderer.measureText(viewMode_, theme.fontSmall);
        int textY = bounds_.y + (bounds_.h - sz.h) / 2;
        renderer.drawText(viewMode_, {currentLeft, textY}, theme.fontSmall, theme.accent);
        currentLeft += sz.w + pad;
    }

    // Context label (left, next to view mode)
    if (!contextLabel_.empty()) {
        std::string text = "• " + contextLabel_;
        Size sz = renderer.measureText(text, theme.fontSmall);
        int textY = bounds_.y + (bounds_.h - sz.h) / 2;
        renderer.drawText(text, {currentLeft, textY}, theme.fontSmall, theme.textSecondary);
        currentLeft += sz.w + pad;
    }

    // Clock (right)
    if (!clock_.empty()) {
        Size sz = renderer.measureText(clock_, theme.fontSmall);
        currentRight -= sz.w;
        int textY = bounds_.y + (bounds_.h - sz.h) / 2;
        renderer.drawText(clock_, {currentRight, textY}, theme.fontSmall, theme.textPrimary);
        currentRight -= pad;
    }

    // Battery (right, before clock)
    if (batteryLevel_ >= 0) {
        std::string batText = std::to_string(batteryLevel_) + "%" + (batteryCharging_ ? "+" : "");
        Size sz = renderer.measureText(batText, theme.fontSmall);
        currentRight -= sz.w;
        int textY = bounds_.y + (bounds_.h - sz.h) / 2;
        renderer.drawText(batText, {currentRight, textY}, theme.fontSmall, theme.textSecondary);
        currentRight -= pad;
    }

    // Now-playing pulse indicator (middle)
    if (nowPlaying_) {
        // Compute oscillating pulse alpha
        float pulse = (std::sin(pulseTime_ * 5.0f) + 1.0f) * 0.5f; // 0.0 .. 1.0
        uint8_t alpha = static_cast<uint8_t>(80 + pulse * 175);
        Color pulseColor = theme.accent.withAlpha(alpha);

        int centerX = bounds_.x + bounds_.w / 2;
        int dotSize = 8;
        int dotY = bounds_.y + (bounds_.h - dotSize) / 2;

        renderer.fillRect({centerX - dotSize / 2, dotY, dotSize, dotSize}, pulseColor);
    }
}

} // namespace hui
