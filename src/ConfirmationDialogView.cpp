#include "hui/ConfirmationDialogView.h"

namespace hui {

void ConfirmationDialogView::DialogButton::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    bool selected = isFocused();
    renderer.fillRect(bounds_, selected ? theme.focusFillColor : theme.surface);
    renderer.drawRect(bounds_, selected ? theme.focusBorderColor : theme.surfaceAlt,
                      selected ? theme.focusBorderWidth : 1);

    Size sz = renderer.measureText(label_, theme.fontBody);
    renderer.drawText(label_,
                      {bounds_.x + (bounds_.w - sz.w) / 2,
                       bounds_.y + (bounds_.h - sz.h) / 2},
                      theme.fontBody,
                      selected ? theme.textPrimary : theme.textSecondary);
}

ConfirmationDialogView::ConfirmationDialogView(ViewStack& stack, std::string message)
    : stack_(stack)
    , message_(std::move(message)) {
    nav_.setAxis(Axis::Horizontal);
    nav_.setWrap(false);
    nav_.add(&cancelButton_);
    nav_.add(&confirmButton_);
}

void ConfirmationDialogView::layout(Rect contentRect) {
    bounds_ = contentRect;
    const int dialogW = std::min(400, bounds_.w * 4 / 5);
    const int dialogH = 160;
    int dx = bounds_.x + (bounds_.w - dialogW) / 2;
    int dy = bounds_.y + (bounds_.h - dialogH) / 2;

    int btnW = (dialogW - 48) / 2;
    int btnH = 36;
    int btnY = dy + dialogH - btnH - 20;
    cancelButton_.layout({dx + 16, btnY, btnW, btnH});
    confirmButton_.layout({dx + dialogW - 16 - btnW, btnY, btnW, btnH});
}

void ConfirmationDialogView::draw(IRenderer& renderer, const Theme& theme) {
    const int dialogW = std::min(400, bounds_.w * 4 / 5);
    const int dialogH = 160;
    Rect dialog{
        bounds_.x + (bounds_.w - dialogW) / 2,
        bounds_.y + (bounds_.h - dialogH) / 2,
        dialogW,
        dialogH
    };

    renderer.fillRect(dialog, theme.surface);
    renderer.drawRect(dialog, theme.surfaceAlt, 2);

    if (!message_.empty()) {
        renderer.drawTextEllipsis(message_,
                                  {dialog.x + 16, dialog.y + 24},
                                  theme.fontBody, theme.textPrimary, dialog.w - 32);
    }

    cancelButton_.draw(renderer, theme);
    confirmButton_.draw(renderer, theme);
}

void ConfirmationDialogView::activateFocused(FocusManager& fm) {
    if (nav_.current() == &confirmButton_) {
        if (onConfirm_) {
            onConfirm_();
        }
        stack_.pop();
    } else if (nav_.current() == &cancelButton_) {
        if (onCancel_) {
            onCancel_();
        }
        stack_.pop();
    } else {
        nav_.focusIndex(0, fm);
    }
}

bool ConfirmationDialogView::onButtonDown(Button b, FocusManager& fm) {
    if (b == Button::B) {
        if (onCancel_) {
            onCancel_();
        }
        stack_.pop();
        return true;
    }

    if (b == Button::A) {
        activateFocused(fm);
        return true;
    }

    if (nav_.handleButton(b, fm)) {
        return true;
    }

    if (!fm.hasFocus(nav_.current())) {
        nav_.focusIndex(0, fm);
    }

    return false;
}

std::vector<HintEntry> ConfirmationDialogView::currentHints() const {
    return {
        {"A", "Select", false, 1},
        {"B", "Cancel", false, 100},
    };
}

void ConfirmationDialogView::restoreFocus(FocusManager& fm) {
    if (savedFocus_) {
        View::restoreFocus(fm);
    } else {
        nav_.focusIndex(0, fm);
    }
}

} // namespace hui
