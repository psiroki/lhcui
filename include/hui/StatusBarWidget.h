#pragma once

#include "hui/Widget.h"
#include "hui/types.h"
#include <string>
#include <string_view>

namespace hui {

// §12 StatusBarWidget
//
// Non-focusable top bar: view mode label, context label, now-playing pulse indicator
// (animated via update()), clock, battery.
class StatusBarWidget : public Widget {
public:
    StatusBarWidget() = default;

    bool isFocusable() const override { return false; }

    void setViewMode(std::string mode) { viewMode_ = std::move(mode); }
    std::string_view viewMode() const { return viewMode_; }

    void setContextLabel(std::string context) { contextLabel_ = std::move(context); }
    std::string_view contextLabel() const { return contextLabel_; }

    void setNowPlaying(bool playing) { nowPlaying_ = playing; }
    bool isNowPlaying() const { return nowPlaying_; }

    void setClock(std::string clock) { clock_ = std::move(clock); }
    std::string_view clock() const { return clock_; }

    void setBatteryLevel(int percent) { batteryLevel_ = percent; }
    int batteryLevel() const { return batteryLevel_; }

    void setBatteryCharging(bool charging) { batteryCharging_ = charging; }
    bool isBatteryCharging() const { return batteryCharging_; }

    void update(float dt) override;
    void draw(IRenderer& renderer, const Theme& theme) override;

    float pulseTime() const { return pulseTime_; }

private:
    std::string viewMode_{"HOME"};
    std::string contextLabel_;
    bool nowPlaying_ = false;
    float pulseTime_ = 0.0f;
    std::string clock_{"12:00"};
    int batteryLevel_ = 100;
    bool batteryCharging_ = false;
};

} // namespace hui
