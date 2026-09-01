#include "hui/SeekableProgressBar.h"

namespace hui {

bool SeekableProgressBar::onButtonDown(Button b) {
    if (isDisabled()) {
        return false;
    }

    if (b == Button::L2) {
        if (onSeek_) {
            onSeek_(-1);
        }
        return true;
    }

    if (b == Button::R2) {
        if (onSeek_) {
            onSeek_(1);
        }
        return true;
    }

    if (isFocused()) {
        if (b == Button::Left) {
            if (onSeek_) {
                onSeek_(-1);
            }
            return true;
        }
        if (b == Button::Right) {
            if (onSeek_) {
                onSeek_(1);
            }
            return true;
        }
    }

    if (b == Button::Up || b == Button::Down) {
        return false;
    }

    return false;
}

void SeekableProgressBar::draw(IRenderer& renderer, const Theme& theme) {
    // Draw the base progress bar
    ProgressBar::draw(renderer, theme);

    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    // Draw focus indicator if focused
    if (isFocused() && !isDisabled()) {
        renderer.drawRect(bounds_, theme.focusBorderColor, theme.focusBorderWidth);
    }
}

} // namespace hui
