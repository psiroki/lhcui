#pragma once

#include "hui/Widget.h"
#include "hui/ViewStack.h"
#include "hui/StatusBarWidget.h"
#include "hui/HintBarWidget.h"
#include "hui/TabBarWidget.h"
#include "hui/ToastNotification.h"
#include "hui/IRenderer.h"
#include "hui/types.h"
#include <string>
#include <string_view>

namespace hui {

// §12 Shell
//
// Composite chrome widget — not a View and never on the ViewStack.
// drawChrome() sits beneath the stack; drawOverlay() draws the toast above everything.
class Shell : public Widget {
public:
    static constexpr int kStatusBarHeight = 24;
    static constexpr int kHintBarHeight = 28;
    static constexpr int kTabBarHeight = 32;

    explicit Shell(ViewStack& stack);

    void layout(Rect screen) override;
    Rect contentRect() const { return contentRect_; }

    void update(float dt) override;

    virtual void drawChrome(IRenderer& r, const Theme& theme);
    virtual void drawOverlay(IRenderer& r, const Theme& theme);

    void showToast(std::string_view message, float seconds);

    void setTabBarVisible(bool visible);
    bool tabBarVisible() const { return tabBarVisible_; }

    StatusBarWidget& statusBar() { return statusBar_; }
    const StatusBarWidget& statusBar() const { return statusBar_; }

    HintBarWidget& hintBar() { return hintBar_; }
    const HintBarWidget& hintBar() const { return hintBar_; }

    TabBarWidget& tabBar() { return tabBar_; }
    const TabBarWidget& tabBar() const { return tabBar_; }

    ToastNotification& toast() { return toast_; }
    const ToastNotification& toast() const { return toast_; }

    void setViewMode(std::string mode) { statusBar_.setViewMode(std::move(mode)); }
    void setContextLabel(std::string label) { statusBar_.setContextLabel(std::move(label)); }
    void setNowPlaying(bool playing) { statusBar_.setNowPlaying(playing); }
    void setClock(std::string clock) { statusBar_.setClock(std::move(clock)); }
    void setBatteryLevel(int percent) { statusBar_.setBatteryLevel(percent); }
    void setBatteryCharging(bool charging) { statusBar_.setBatteryCharging(charging); }

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    ViewStack& stack_;
    StatusBarWidget statusBar_;
    HintBarWidget hintBar_;
    TabBarWidget tabBar_;
    ToastNotification toast_;
    bool tabBarVisible_ = false;
    Rect screenRect_{0, 0, 640, 480};
    Rect contentRect_{0, 0, 640, 480};
};

} // namespace hui
