#pragma once

#include "hui/Widget.h"
#include <functional>
#include <string>
#include <vector>

namespace hui {

// §12 TabBarWidget
//
// Horizontal tab strip. L1/R1 switch tabs. Not reachable via D-pad Up/Down.
class TabBarWidget : public Widget {
public:
    TabBarWidget() = default;

    bool isFocusable() const override { return true; }

    void setTabs(std::vector<std::string> tabs);
    void addTab(std::string label);

    int selectedIndex() const { return selectedIndex_; }
    void setSelectedIndex(int index);

    void setOnTabChanged(std::function<void(int index)> cb) { onTabChanged_ = std::move(cb); }

    void layout(Rect r) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b) override;

private:
    void selectTab(int index);

    std::vector<std::string> tabs_;
    int selectedIndex_ = 0;
    std::function<void(int index)> onTabChanged_;
};

} // namespace hui
