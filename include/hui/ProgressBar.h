#pragma once

#include "hui/Widget.h"
#include <string>
#include <string_view>

namespace hui {

// §12 ProgressBar
//
// Read-only horizontal fill bar with elapsed and total timestamp labels.
// Non-focusable (isFocusable() == false).
class ProgressBar : public Widget {
public:
    ProgressBar() = default;

    bool isFocusable() const override { return false; }

    void setProgress(float ratio);
    float progress() const { return ratio_; }

    void setTime(float elapsedSeconds, float totalSeconds);
    void setTimestamps(std::string elapsed, std::string total);

    std::string_view elapsedText() const { return elapsed_; }
    std::string_view totalText() const { return total_; }

    void draw(IRenderer& renderer, const Theme& theme) override;

protected:
    float ratio_ = 0.0f;
    std::string elapsed_{"0:00"};
    std::string total_{"0:00"};

    static std::string formatTime(float seconds);
};

} // namespace hui
