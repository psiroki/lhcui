#include "hui/Slider.h"
#include <algorithm>

namespace hui {

Slider::Slider(int min, int max, int value, int step) {
    setRange(min, max, step);
    setValue(value);
}

void Slider::setValue(int value) {
    value_ = std::clamp(value, min_, max_);
}

void Slider::setRange(int min, int max, int step) {
    min_ = min;
    max_ = std::max(min, max);
    step_ = step > 0 ? step : 1;
    value_ = std::clamp(value_, min_, max_);
}

bool Slider::onButtonDown(Button b) {
    if (isDisabled()) {
        return false;
    }

    if (b == Button::Left) {
        int next = std::clamp(value_ - step_, min_, max_);
        if (next != value_) {
            value_ = next;
            if (onValueChanged_) {
                onValueChanged_(value_);
            }
        }
        return true;
    }

    if (b == Button::Right) {
        int next = std::clamp(value_ + step_, min_, max_);
        if (next != value_) {
            value_ = next;
            if (onValueChanged_) {
                onValueChanged_(value_);
            }
        }
        return true;
    }

    return false;
}

void Slider::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    const bool disabled = isDisabled();

    // Focus highlight
    if (isFocused() && !disabled) {
        renderer.fillRect(bounds_, theme.focusFillColor);
        renderer.drawRect(bounds_, theme.focusBorderColor, theme.focusBorderWidth);
    }

    const int pad = 8;
    int curX = bounds_.x + pad;

    // Label text on left
    if (!label_.empty()) {
        Size sz = renderer.measureText(label_, theme.fontBody);
        int textY = bounds_.y + (bounds_.h - sz.h) / 2;
        renderer.drawText(label_, {curX, textY}, theme.fontBody, disabled ? theme.textDisabled : theme.textPrimary);
        curX += sz.w + 12;
    }

    // Value text on right — reserve width for max() so the track does not resize
    // when the digit count changes (e.g. 95 → 100).
    const std::string maxValStr = std::to_string(max_);
    const Size valSlotSz = renderer.measureText(maxValStr, theme.fontSmall);
    const int valSlotX = bounds_.x + bounds_.w - pad - valSlotSz.w;

    const std::string valStr = std::to_string(value_);
    const Size valSz = renderer.measureText(valStr, theme.fontSmall);
    const int valX = valSlotX + (valSlotSz.w - valSz.w);
    const int valY = bounds_.y + (bounds_.h - valSz.h) / 2;
    renderer.drawText(valStr, {valX, valY}, theme.fontSmall,
                      disabled ? theme.textDisabled : theme.textSecondary);

    // Track in middle
    int trackX = curX;
    int trackW = std::max(0, valSlotX - 12 - trackX);
    int trackH = 6;
    int trackY = bounds_.y + (bounds_.h - trackH) / 2;

    if (trackW > 0) {
        renderer.fillRect({trackX, trackY, trackW, trackH}, theme.surfaceAlt);

        const int thumbW = 8;
        const int thumbH = 14;
        const int thumbTravel = std::max(0, trackW - thumbW);

        int thumbX = trackX;
        if (thumbTravel > 0 && max_ > min_) {
            thumbX = trackX + (value_ - min_) * thumbTravel / (max_ - min_);
        }

        int fillW = thumbX + thumbW / 2 - trackX;
        fillW = std::clamp(fillW, 0, trackW);

        Color fillCol = disabled ? theme.textDisabled : (isFocused() ? theme.accent : theme.textSecondary);
        if (fillW > 0) {
            renderer.fillRect({trackX, trackY, fillW, trackH}, fillCol);
        }

        int thumbY = bounds_.y + (bounds_.h - thumbH) / 2;
        renderer.fillRect({thumbX, thumbY, thumbW, thumbH},
                          (isFocused() && !disabled) ? theme.textPrimary : theme.textSecondary);
    }
}

} // namespace hui
