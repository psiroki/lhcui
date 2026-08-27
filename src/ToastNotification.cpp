#include "hui/ToastNotification.h"
#include <algorithm>

namespace hui {

void ToastNotification::show(std::string_view message, float duration, float fadeDuration) {
    message_ = std::string(message);
    totalDuration_ = duration > 0.0f ? duration : 0.1f;
    remainingTime_ = totalDuration_;
    fadeDuration_ = fadeDuration >= 0.0f ? fadeDuration : 0.0f;
    visible_ = true;
}

void ToastNotification::hide() {
    visible_ = false;
    remainingTime_ = 0.0f;
}

void ToastNotification::update(float dt) {
    if (!visible_) {
        return;
    }

    remainingTime_ -= dt;
    if (remainingTime_ <= 0.0f) {
        visible_ = false;
        remainingTime_ = 0.0f;
    }
}

void ToastNotification::draw(IRenderer& renderer, const Theme& theme) {
    if (!visible_ || bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    uint8_t alpha = 255;
    if (fadeDuration_ > 0.0f && remainingTime_ < fadeDuration_) {
        float t = remainingTime_ / fadeDuration_;
        alpha = static_cast<uint8_t>(std::clamp(t, 0.0f, 1.0f) * 255.0f);
    }

    if (alpha == 0) {
        return;
    }

    Size textSz = renderer.measureText(message_, theme.fontSmall);
    int toastW = textSz.w + 24;
    int toastH = textSz.h + 12;
    int toastX = bounds_.x + (bounds_.w - toastW) / 2;
    int toastY = bounds_.y + bounds_.h - toastH - 24;
    if (toastY < bounds_.y) {
        toastY = bounds_.y + (bounds_.h - toastH) / 2;
    }

    Rect toastRect{toastX, toastY, toastW, toastH};

    // Draw semi-transparent toast background & border
    renderer.fillRect(toastRect, theme.surface.withAlpha(alpha));
    renderer.drawRect(toastRect, theme.accent.withAlpha(alpha), 1);

    // Draw toast text
    renderer.drawText(message_, {toastX + 12, toastY + 6}, theme.fontSmall, theme.textPrimary.withAlpha(alpha));
}

} // namespace hui
