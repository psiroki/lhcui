#pragma once

#include "hui/Widget.h"
#include "hui/types.h"
#include <string>
#include <string_view>

namespace hui {

// §12 ToastNotification
//
// Non-focusable widget owned by Shell (never pushed onto ViewStack).
// Self-timed via update(). Auto-dismissed with fade-out animation.
// Enforces replacement policy (new toast replaces old immediately, no stacking).
class ToastNotification : public Widget {
public:
    ToastNotification() = default;

    bool isFocusable() const override { return false; }

    void show(std::string_view message, float duration = 2.0f, float fadeDuration = 0.3f);
    void hide();

    bool isVisible() const { return visible_; }
    std::string_view message() const { return message_; }
    float remainingTime() const { return remainingTime_; }

    void update(float dt) override;
    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    std::string message_;
    float remainingTime_ = 0.0f;
    float totalDuration_ = 0.0f;
    float fadeDuration_ = 0.3f;
    bool visible_ = false;
};

} // namespace hui
