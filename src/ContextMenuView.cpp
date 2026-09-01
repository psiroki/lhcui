#include "hui/ContextMenuView.h"

namespace hui {

ContextMenuView::ContextMenuView(ViewStack& stack)
    : stack_(stack)
    , list_(36) {
    list_.setSource(&source_);
    list_.setOnActivate([this](int index) {
        if (onAction_) {
            onAction_(index);
        }
        stack_.pop();
    });
}

void ContextMenuView::layout(Rect contentRect) {
    bounds_ = contentRect;
    const int menuW = std::min(320, bounds_.w * 2 / 3);
    const int menuH = std::min(360, bounds_.h * 3 / 4);
    Rect menuRect{
        bounds_.x + (bounds_.w - menuW) / 2,
        bounds_.y + (bounds_.h - menuH) / 2,
        menuW,
        menuH
    };
    list_.layout(menuRect);
}

void ContextMenuView::draw(IRenderer& renderer, const Theme& theme) {
    Rect menuRect = list_.bounds();
    renderer.fillRect(menuRect, theme.surface);
    renderer.drawRect(menuRect, theme.surfaceAlt, 2);
    list_.draw(renderer, theme);
}

bool ContextMenuView::onButtonDown(Button b, FocusManager& fm) {
    if (b == Button::B) {
        if (onCancel_) {
            onCancel_();
        }
        stack_.pop();
        return true;
    }

    if (list_.onButtonDown(b)) {
        return true;
    }

    if (!fm.hasFocus(&list_)) {
        fm.setFocus(&list_);
    }

    return false;
}

std::vector<HintEntry> ContextMenuView::currentHints() const {
    return {
        {"A", "Select", false, 1},
        {"B", "Cancel", false, 100},
    };
}

void ContextMenuView::restoreFocus(FocusManager& fm) {
    if (savedFocus_) {
        View::restoreFocus(fm);
    } else {
        fm.setFocus(&list_);
    }
}

} // namespace hui
