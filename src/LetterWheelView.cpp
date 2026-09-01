#include "hui/LetterWheelView.h"

namespace hui {

LetterWheelView::LetterWheelView(ViewStack& stack)
    : stack_(stack) {}

void LetterWheelView::layout(Rect contentRect) {
    bounds_ = contentRect;
    letterWheel_.layout(bounds_);
}

void LetterWheelView::draw(IRenderer& renderer, const Theme& theme) {
    letterWheel_.draw(renderer, theme);
}

bool LetterWheelView::onButtonDown(Button b, FocusManager& fm) {
    if (b == Button::B) {
        stack_.pop();
        if (onDismiss_) {
            onDismiss_();
        }
        return true;
    }

    if (letterWheel_.onButtonDown(b)) {
        return true;
    }

    if (!fm.hasFocus(&letterWheel_)) {
        fm.setFocus(&letterWheel_);
    }

    return false;
}

void LetterWheelView::restoreFocus(FocusManager& fm) {
    if (savedFocus_) {
        View::restoreFocus(fm);
    } else {
        fm.setFocus(&letterWheel_);
    }
}

std::vector<HintEntry> LetterWheelView::currentHints() const {
    return {
        {"A", "Select", false, 1},
        {"B", "Close", false, 100},
    };
}

} // namespace hui
