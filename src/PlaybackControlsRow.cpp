#include "hui/PlaybackControlsRow.h"

namespace hui {

PlaybackControlsRow::PlaybackControlsRow(PlaybackState state)
    : state_(state) {}

void PlaybackControlsRow::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    // Centered row of transport symbols: [|<]  [Play/Pause/Stop]  [>|]
    int centerX = bounds_.x + bounds_.w / 2;
    int centerY = bounds_.y + bounds_.h / 2;

    const int buttonSize = 24;
    const int spacing = 16;

    // Previous button on left
    Rect prevRect{centerX - buttonSize - spacing - buttonSize / 2, centerY - buttonSize / 2, buttonSize, buttonSize};
    renderer.fillRect(prevRect, theme.surface);
    renderer.drawRect(prevRect, theme.surfaceAlt, 1);
    Size prevSz = renderer.measureText("|<", theme.fontSmall);
    renderer.drawText("|<", {prevRect.x + (prevRect.w - prevSz.w) / 2, prevRect.y + (prevRect.h - prevSz.h) / 2}, theme.fontSmall, theme.textSecondary);

    // Center Main Playback Button
    Rect mainRect{centerX - buttonSize / 2 - 4, centerY - buttonSize / 2 - 2, buttonSize + 8, buttonSize + 4};
    Color mainBg = theme.surface;
    Color mainColor = theme.textSecondary;
    std::string mainSymbol = "[]";

    switch (state_) {
        case PlaybackState::Playing:
            mainBg = theme.accent.withAlpha(50);
            mainColor = theme.accent;
            mainSymbol = "||";
            break;
        case PlaybackState::Paused:
            mainBg = theme.surfaceAlt;
            mainColor = theme.textPrimary;
            mainSymbol = "|>";
            break;
        case PlaybackState::Stopped:
            mainBg = theme.surface;
            mainColor = theme.textDisabled;
            mainSymbol = "[]";
            break;
    }

    renderer.fillRect(mainRect, mainBg);
    renderer.drawRect(mainRect, state_ == PlaybackState::Playing ? theme.accent : theme.surfaceAlt, 1);
    Size mainSz = renderer.measureText(mainSymbol, theme.fontSmall);
    renderer.drawText(mainSymbol, {mainRect.x + (mainRect.w - mainSz.w) / 2, mainRect.y + (mainRect.h - mainSz.h) / 2}, theme.fontSmall, mainColor);

    // Next button on right
    Rect nextRect{centerX + spacing + buttonSize / 2, centerY - buttonSize / 2, buttonSize, buttonSize};
    renderer.fillRect(nextRect, theme.surface);
    renderer.drawRect(nextRect, theme.surfaceAlt, 1);
    Size nextSz = renderer.measureText(">|", theme.fontSmall);
    renderer.drawText(">|", {nextRect.x + (nextRect.w - nextSz.w) / 2, nextRect.y + (nextRect.h - nextSz.h) / 2}, theme.fontSmall, theme.textSecondary);
}

} // namespace hui
