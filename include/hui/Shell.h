#pragma once

#include "hui/Widget.h"
#include "hui/ViewStack.h"
#include "hui/IRenderer.h"
#include "hui/types.h"
#include <string_view>

namespace hui {

class Shell : public Widget {
public:
    explicit Shell(ViewStack& stack) : stack_(stack) {}
    ~Shell() override = default;

    void layout(Rect screen) override {
        screenRect_ = screen;
        contentRect_ = screen;
        stack_.setContentRect(contentRect_);
    }

    Rect contentRect() const { return contentRect_; }

    virtual void drawChrome(IRenderer& r, const Theme& theme) {
        (void)r; (void)theme;
    }

    virtual void drawOverlay(IRenderer& r, const Theme& theme) {
        (void)r; (void)theme;
    }

    void showToast(std::string_view message, float seconds) {
        (void)message; (void)seconds;
    }

    void setTabBarVisible(bool v) {
        (void)v;
    }

protected:
    ViewStack& stack_;
    Rect screenRect_{0, 0, 640, 480};
    Rect contentRect_{0, 0, 640, 480};
};

} // namespace hui
