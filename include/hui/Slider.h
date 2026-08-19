#pragma once

#include "hui/Widget.h"
#include <functional>
#include <string>
#include <string_view>

namespace hui {

// §12 Slider
//
// Focusable horizontal value control driven by Left/Right buttons.
// Step size configurable (e.g. 5 for guide overlay). Calls onValueChanged callback.
class Slider : public Widget {
public:
    explicit Slider(int min = 0, int max = 100, int value = 0, int step = 1);

    bool isFocusable() const override { return true; }

    void setValue(int value);
    int  value() const { return value_; }

    void setRange(int min, int max, int step = 1);
    void setStep(int step) { step_ = step > 0 ? step : 1; }
    int  step() const { return step_; }

    int min() const { return min_; }
    int max() const { return max_; }

    void setLabel(std::string label) { label_ = std::move(label); }
    std::string_view label() const { return label_; }

    void setOnValueChanged(std::function<void(int)> cb) { onValueChanged_ = std::move(cb); }

    bool onButtonDown(Button b) override;

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    int min_ = 0;
    int max_ = 100;
    int value_ = 0;
    int step_ = 1;
    std::string label_;
    std::function<void(int)> onValueChanged_;
};

} // namespace hui
