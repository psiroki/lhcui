#include "hui/ProgressBar.h"
#include <algorithm>
#include <cstdio>

namespace hui {

std::string ProgressBar::formatTime(float seconds) {
    if (seconds < 0.0f) seconds = 0.0f;
    int totalSecs = static_cast<int>(seconds);
    int mins = totalSecs / 60;
    int secs = totalSecs % 60;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
    return std::string(buf);
}

void ProgressBar::setProgress(float ratio) {
    ratio_ = std::clamp(ratio, 0.0f, 1.0f);
}

void ProgressBar::setTime(float elapsedSeconds, float totalSeconds) {
    elapsedSeconds = std::max(0.0f, elapsedSeconds);
    totalSeconds = std::max(0.0f, totalSeconds);
    ratio_ = (totalSeconds > 0.0f) ? std::clamp(elapsedSeconds / totalSeconds, 0.0f, 1.0f) : 0.0f;
    elapsed_ = formatTime(elapsedSeconds);
    total_ = formatTime(totalSeconds);
}

void ProgressBar::setTimestamps(std::string elapsed, std::string total) {
    elapsed_ = std::move(elapsed);
    total_ = std::move(total);
}

void ProgressBar::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    if (bounds_.h >= 24) {
        // Stacked layout: Bar on top, timestamp labels below
        int barH = 6;
        int barY = bounds_.y + 4;
        int barX = bounds_.x;
        int barW = bounds_.w;

        // Background track
        renderer.fillRect({barX, barY, barW, barH}, theme.surfaceAlt);

        // Fill bar
        int fillW = static_cast<int>(barW * ratio_);
        if (fillW > 0) {
            renderer.fillRect({barX, barY, fillW, barH}, theme.accent);
        }

        // Timestamp labels
        int textY = barY + barH + 4;
        renderer.drawText(elapsed_, {bounds_.x, textY}, theme.fontSmall, theme.textSecondary);

        Size totSz = renderer.measureText(total_, theme.fontSmall);
        int totX = bounds_.x + bounds_.w - totSz.w;
        renderer.drawText(total_, {totX, textY}, theme.fontSmall, theme.textSecondary);
    } else {
        // Inline layout: [Elapsed] [----Bar----] [Total]
        Size elSz = renderer.measureText(elapsed_, theme.fontSmall);
        Size totSz = renderer.measureText(total_, theme.fontSmall);

        int textY = bounds_.y + (bounds_.h - elSz.h) / 2;
        renderer.drawText(elapsed_, {bounds_.x, textY}, theme.fontSmall, theme.textSecondary);

        int totX = bounds_.x + bounds_.w - totSz.w;
        renderer.drawText(total_, {totX, textY}, theme.fontSmall, theme.textSecondary);

        int barX = bounds_.x + elSz.w + 8;
        int barW = std::max(0, bounds_.w - elSz.w - totSz.w - 16);
        int barH = std::min(bounds_.h, 6);
        int barY = bounds_.y + (bounds_.h - barH) / 2;

        if (barW > 0) {
            renderer.fillRect({barX, barY, barW, barH}, theme.surfaceAlt);
            int fillW = static_cast<int>(barW * ratio_);
            if (fillW > 0) {
                renderer.fillRect({barX, barY, fillW, barH}, theme.accent);
            }
        }
    }
}

} // namespace hui
