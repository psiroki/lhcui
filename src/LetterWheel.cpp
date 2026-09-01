#include "hui/LetterWheel.h"
#include <algorithm>

namespace hui {

LetterWheel::LetterWheel(int itemHeight)
    : results_(itemHeight) {}

char LetterWheel::selectedChar() const {
    if (stripIndex_ >= 0 && stripIndex_ < static_cast<int>(stripChars_.size())) {
        return stripChars_[stripIndex_];
    }
    return 'A';
}

void LetterWheel::selectStripIndex(int index) {
    if (stripChars_.empty()) {
        return;
    }
    int next = index;
    if (next < 0) {
        next = static_cast<int>(stripChars_.size()) - 1;
    } else if (next >= static_cast<int>(stripChars_.size())) {
        next = 0;
    }
    if (next != stripIndex_) {
        stripIndex_ = next;
        if (onCharChanged_) {
            onCharChanged_(selectedChar());
        }
    }
}

void LetterWheel::layout(Rect r) {
    bounds_ = r;
    Rect stripRect{bounds_.x, bounds_.y, bounds_.w, stripHeight_};
    (void)stripRect;
    results_.layout({bounds_.x, bounds_.y + stripHeight_, bounds_.w,
                     std::max(0, bounds_.h - stripHeight_)});
}

void LetterWheel::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    Rect stripRect{bounds_.x, bounds_.y, bounds_.w, stripHeight_};
    renderer.fillRect(stripRect, theme.surface);

    if (!stripChars_.empty()) {
        int cellW = std::max(1, bounds_.w / static_cast<int>(stripChars_.size()));
        for (int i = 0; i < static_cast<int>(stripChars_.size()); ++i) {
            Rect cell{stripRect.x + i * cellW, stripRect.y, cellW, stripRect.h};
            bool selected = (focusArea_ == FocusArea::Strip && i == stripIndex_);
            if (selected) {
                renderer.fillRect(cell, theme.focusFillColor);
                renderer.drawRect(cell, theme.focusBorderColor, theme.focusBorderWidth);
            }
            std::string ch(1, stripChars_[i]);
            Size sz = renderer.measureText(ch, theme.fontBody);
            Color col = selected ? theme.accent : theme.textSecondary;
            renderer.drawText(ch,
                              {cell.x + (cell.w - sz.w) / 2,
                               cell.y + (cell.h - sz.h) / 2},
                              theme.fontBody, col);
        }
    }

    results_.setPaintAsFocused(focusArea_ == FocusArea::List && isFocused());
    results_.draw(renderer, theme);
}

bool LetterWheel::onButtonDown(Button b) {
    if (isDisabled()) {
        return false;
    }

    if (focusArea_ == FocusArea::Strip) {
        if (b == Button::Left) {
            selectStripIndex(stripIndex_ - 1);
            return true;
        }
        if (b == Button::Right) {
            selectStripIndex(stripIndex_ + 1);
            return true;
        }
        if (b == Button::Down) {
            focusArea_ = FocusArea::List;
            return true;
        }
        if (b == Button::Up) {
            return false;
        }
        if (b == Button::A) {
            selectStripIndex(stripIndex_);
            return true;
        }
        return false;
    }

    // List focus area — delegate to results, with Up-at-top returning to strip
    if (b == Button::Up && results_.getFocusIndex() == 0) {
        focusArea_ = FocusArea::Strip;
        return true;
    }

    return results_.onButtonDown(b);
}

} // namespace hui
