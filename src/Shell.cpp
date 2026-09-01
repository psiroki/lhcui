#include "hui/Shell.h"

namespace hui {

Shell::Shell(ViewStack& stack)
    : stack_(stack)
    , hintBar_(&stack) {}

void Shell::layout(Rect screen) {
    screenRect_ = screen;

    int contentTop = screen.y + kStatusBarHeight;
    if (tabBarVisible_) {
        contentTop += kTabBarHeight;
    }

    const int contentBottom = screen.y + screen.h - kHintBarHeight;
    contentRect_ = {
        screen.x,
        contentTop,
        screen.w,
        std::max(0, contentBottom - contentTop)
    };

    statusBar_.layout({screen.x, screen.y, screen.w, kStatusBarHeight});

    if (tabBarVisible_) {
        tabBar_.layout({screen.x, screen.y + kStatusBarHeight, screen.w, kTabBarHeight});
    }

    hintBar_.layout({screen.x, screen.y + screen.h - kHintBarHeight, screen.w, kHintBarHeight});
    toast_.layout(screen);

    stack_.setContentRect(contentRect_);
}

void Shell::update(float dt) {
    statusBar_.update(dt);
    toast_.update(dt);
}

void Shell::drawChrome(IRenderer& r, const Theme& theme) {
    statusBar_.draw(r, theme);
    if (tabBarVisible_) {
        tabBar_.draw(r, theme);
    }
    hintBar_.draw(r, theme);
}

void Shell::drawOverlay(IRenderer& r, const Theme& theme) {
    toast_.draw(r, theme);
}

void Shell::showToast(std::string_view message, float seconds) {
    toast_.show(message, seconds);
}

void Shell::setTabBarVisible(bool visible) {
    if (tabBarVisible_ == visible) {
        return;
    }
    tabBarVisible_ = visible;
    layout(screenRect_);
}

void Shell::draw(IRenderer& renderer, const Theme& theme) {
    drawChrome(renderer, theme);
    drawOverlay(renderer, theme);
}

} // namespace hui
