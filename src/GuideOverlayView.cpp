#include "hui/GuideOverlayView.h"
#include <algorithm>

namespace hui {

void GuideOverlayView::ActionButton::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    bool selected = isFocused();
    renderer.fillRect(bounds_, selected ? theme.focusFillColor : theme.surface);
    renderer.drawRect(bounds_, selected ? theme.focusBorderColor : theme.surfaceAlt,
                      selected ? theme.focusBorderWidth : 1);

    Size sz = renderer.measureText(label_, theme.fontBody);
    renderer.drawText(label_,
                      {bounds_.x + 12, bounds_.y + (bounds_.h - sz.h) / 2},
                      theme.fontBody,
                      selected ? theme.textPrimary : theme.textSecondary);
}

GuideOverlayView::GuideOverlayView(ViewStack& stack, bool animationsEnabled)
    : stack_(stack)
    , animationsEnabled_(animationsEnabled) {
    masterVolume_.setLabel("Master Volume");
    brightness_.setLabel("Brightness");

    nav_.setAxis(Axis::Vertical);
    nav_.setWrap(true);
    nav_.add(&masterVolume_);
    nav_.add(&brightness_);
    nav_.add(&equalizerBtn_);
    nav_.add(&settingsBtn_);
    nav_.add(&closeBtn_);

    slideOffset_ = animationsEnabled_ ? 280.0f : 0.0f;
}

void GuideOverlayView::finishOpen() {
    slideOffset_ = 0.0f;
}

void GuideOverlayView::layout(Rect contentRect) {
    bounds_ = contentRect;
    panelWidth_ = std::min(280, bounds_.w * 2 / 3);
    int panelX = bounds_.x + bounds_.w - panelWidth_ + static_cast<int>(slideOffset_);
    panelBounds_ = {panelX, bounds_.y, panelWidth_, bounds_.h};

    int y = panelBounds_.y + 24;
    const int rowH = 44;
    const int pad = 12;

    masterVolume_.layout({panelBounds_.x + pad, y, panelBounds_.w - 2 * pad, rowH});
    y += rowH + 8;
    brightness_.layout({panelBounds_.x + pad, y, panelBounds_.w - 2 * pad, rowH});
    y += rowH + 16;

    equalizerBtn_.layout({panelBounds_.x + pad, y, panelBounds_.w - 2 * pad, rowH});
    y += rowH + 4;
    settingsBtn_.layout({panelBounds_.x + pad, y, panelBounds_.w - 2 * pad, rowH});
    y += rowH + 4;
    closeBtn_.layout({panelBounds_.x + pad, y, panelBounds_.w - 2 * pad, rowH});
}

bool GuideOverlayView::isActionItem(const Widget* w) const {
    return w == &equalizerBtn_ || w == &settingsBtn_ || w == &closeBtn_;
}

void GuideOverlayView::syncNavFocus(FocusManager& fm) {
    int idx = nav_.index();
    if (idx < 0) {
        idx = 0;
    }
    Widget* expected = nav_.current();
    if (!expected || !fm.hasFocus(expected)) {
        nav_.focusIndex(idx, fm);
    }
}

void GuideOverlayView::update(float dt, FocusManager& fm) {
    syncNavFocus(fm);

    if (!animationsEnabled_) {
        slideOffset_ = 0.0f;
        return;
    }
    if (slideOffset_ > 0.0f) {
        slideOffset_ = std::max(0.0f, slideOffset_ - dt * kSlideSpeed);
        layout(bounds_);
    }
}

void GuideOverlayView::draw(IRenderer& renderer, const Theme& theme) {
    layout(bounds_);

    renderer.fillRect(panelBounds_, theme.surface);
    renderer.drawRect(panelBounds_, theme.surfaceAlt, 2);

    Size titleSz = renderer.measureText("Guide", theme.fontBody);
    (void)titleSz;
    renderer.drawText("Guide",
                      {panelBounds_.x + 12, panelBounds_.y + 8},
                      theme.fontBody, theme.textPrimary);

    masterVolume_.draw(renderer, theme);
    brightness_.draw(renderer, theme);
    equalizerBtn_.draw(renderer, theme);
    settingsBtn_.draw(renderer, theme);
    closeBtn_.draw(renderer, theme);
}

void GuideOverlayView::activateFocused() {
    Widget* current = nav_.current();
    if (current == &equalizerBtn_) {
        if (onEqualizer_) {
            onEqualizer_();
        }
    } else if (current == &settingsBtn_) {
        if (onSettings_) {
            onSettings_();
        }
    } else if (current == &closeBtn_) {
        if (onClose_) {
            onClose_();
        }
        stack_.pop();
    }
}

bool GuideOverlayView::onButtonDown(Button b, FocusManager& fm) {
    if (b == Button::B) {
        if (onClose_) {
            onClose_();
        }
        stack_.pop();
        return true;
    }

    if (b == Button::A) {
        activateFocused();
        return true;
    }

    Widget* current = nav_.current();
    if (current == &masterVolume_ || current == &brightness_) {
        if (b == Button::Left || b == Button::Right) {
            return current->onButtonDown(b);
        }
    }

    if (current && isActionItem(current) && (b == Button::Left || b == Button::Right)) {
        return true;
    }

    if (nav_.handleButton(b, fm)) {
        return true;
    }

    syncNavFocus(fm);

    return false;
}

std::vector<HintEntry> GuideOverlayView::currentHints() const {
    return {
        {"A", "Select", false, 1},
        {"B", "Close", false, 100},
    };
}

void GuideOverlayView::restoreFocus(FocusManager& fm) {
    if (savedFocus_) {
        View::restoreFocus(fm);
    } else {
        nav_.focusIndex(0, fm);
    }
}

} // namespace hui
