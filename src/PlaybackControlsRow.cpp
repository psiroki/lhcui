#include "hui/PlaybackControlsRow.h"
#include <algorithm>

namespace hui {

PlaybackControlsRow::PlaybackControlsRow(PlaybackState state)
    : state_(state) {}

void PlaybackControlsRow::layoutSegments() {
    if (bounds_.w <= 0) {
        segmentWidth_ = 0;
        return;
    }
    segmentWidth_ = bounds_.w / 5;
}

Rect PlaybackControlsRow::segmentRect(int index) const {
    return {bounds_.x + index * segmentWidth_, bounds_.y, segmentWidth_, bounds_.h};
}

bool PlaybackControlsRow::onButtonDown(Button b) {
    if (isDisabled()) {
        return false;
    }

    if (b == Button::Left) {
        selectedSegment_ = (selectedSegment_ + 4) % 5;
        return true;
    }

    if (b == Button::Right) {
        selectedSegment_ = (selectedSegment_ + 1) % 5;
        return true;
    }

    if (b == Button::Up || b == Button::Down) {
        return false;
    }

    if (b == Button::A) {
        if (!isFocused()) {
            return false;
        }
        if (onActivate_) {
            TransportAction action = TransportAction::PlayPause;
            switch (selectedSegment_) {
                case 0: action = TransportAction::Previous; break;
                case 1: action = TransportAction::PlayPause; break;
                case 2: action = TransportAction::Next; break;
                case 3: action = TransportAction::Shuffle; break;
                case 4: action = TransportAction::Repeat; break;
            }
            onActivate_(action);
        }
        return true;
    }

    return false;
}

void PlaybackControlsRow::drawSegment(IRenderer& renderer, const Theme& theme,
                                      int index, const Rect& rect) {
    const bool selected = isFocused() && selectedSegment_ == index && !isDisabled();

    if (selected) {
        renderer.fillRect(rect, theme.focusFillColor);
        renderer.drawRect(rect, theme.focusBorderColor, theme.focusBorderWidth);
    }

    if (index == 3) {
        shuffleToggle_.layout(rect);
        shuffleToggle_.draw(renderer, theme);
        return;
    }

    if (index == 4) {
        repeatToggle_.layout(rect);
        repeatToggle_.draw(renderer, theme);
        return;
    }

    int centerX = rect.x + rect.w / 2;
    int centerY = rect.y + rect.h / 2;
    const int buttonSize = 24;

    if (index == 0) {
        Rect prevRect{centerX - buttonSize / 2, centerY - buttonSize / 2, buttonSize, buttonSize};
        renderer.fillRect(prevRect, theme.surface);
        renderer.drawRect(prevRect, selected ? theme.focusBorderColor : theme.surfaceAlt, 1);
        Size prevSz = renderer.measureText("|<", theme.fontSmall);
        renderer.drawText("|<", {prevRect.x + (prevRect.w - prevSz.w) / 2,
                                 prevRect.y + (prevRect.h - prevSz.h) / 2},
                          theme.fontSmall, theme.textSecondary);
        return;
    }

    if (index == 2) {
        Rect nextRect{centerX - buttonSize / 2, centerY - buttonSize / 2, buttonSize, buttonSize};
        renderer.fillRect(nextRect, theme.surface);
        renderer.drawRect(nextRect, selected ? theme.focusBorderColor : theme.surfaceAlt, 1);
        Size nextSz = renderer.measureText(">|", theme.fontSmall);
        renderer.drawText(">|", {nextRect.x + (nextRect.w - nextSz.w) / 2,
                                 nextRect.y + (nextRect.h - nextSz.h) / 2},
                          theme.fontSmall, theme.textSecondary);
        return;
    }

    // Play/pause segment
    Rect mainRect{centerX - buttonSize / 2 - 4, centerY - buttonSize / 2 - 2,
                  buttonSize + 8, buttonSize + 4};
    Color mainBg = theme.surface;
    Color mainColor = theme.textSecondary;
    std::string mainSymbol = "|>";

    if (state_ == PlaybackState::Playing) {
        mainBg = theme.accent.withAlpha(50);
        mainColor = theme.accent;
        mainSymbol = "||";
    } else {
        mainBg = theme.surfaceAlt;
        mainColor = theme.textPrimary;
        mainSymbol = "|>";
    }

    renderer.fillRect(mainRect, mainBg);
    renderer.drawRect(mainRect,
                      state_ == PlaybackState::Playing ? theme.accent
                                                       : (selected ? theme.focusBorderColor : theme.surfaceAlt),
                      1);
    Size mainSz = renderer.measureText(mainSymbol, theme.fontSmall);
    renderer.drawText(mainSymbol,
                      {mainRect.x + (mainRect.w - mainSz.w) / 2,
                       mainRect.y + (mainRect.h - mainSz.h) / 2},
                      theme.fontSmall, mainColor);
}

void PlaybackControlsRow::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    layoutSegments();
    for (int i = 0; i < 5; ++i) {
        drawSegment(renderer, theme, i, segmentRect(i));
    }
}

} // namespace hui
