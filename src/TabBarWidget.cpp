#include "hui/TabBarWidget.h"
#include <algorithm>

namespace hui {

void TabBarWidget::setTabs(std::vector<std::string> tabs) {
    tabs_ = std::move(tabs);
    selectedIndex_ = tabs_.empty() ? 0 : std::clamp(selectedIndex_, 0, static_cast<int>(tabs_.size()) - 1);
}

void TabBarWidget::addTab(std::string label) {
    tabs_.push_back(std::move(label));
}

void TabBarWidget::setSelectedIndex(int index) {
    if (tabs_.empty()) {
        selectedIndex_ = 0;
        return;
    }
    selectedIndex_ = std::clamp(index, 0, static_cast<int>(tabs_.size()) - 1);
}

void TabBarWidget::selectTab(int index) {
    if (tabs_.empty()) {
        return;
    }
    int next = index;
    if (next < 0) {
        next = static_cast<int>(tabs_.size()) - 1;
    } else if (next >= static_cast<int>(tabs_.size())) {
        next = 0;
    }
    if (next != selectedIndex_) {
        selectedIndex_ = next;
        if (onTabChanged_) {
            onTabChanged_(selectedIndex_);
        }
    }
}

void TabBarWidget::layout(Rect r) {
    bounds_ = r;
}

void TabBarWidget::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0 || tabs_.empty()) {
        return;
    }

    int tabW = bounds_.w / static_cast<int>(tabs_.size());
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i) {
        Rect tabRect{bounds_.x + i * tabW, bounds_.y, tabW, bounds_.h};
        bool selected = (i == selectedIndex_);
        Color bg = selected ? theme.surfaceAlt : theme.surface;
        renderer.fillRect(tabRect, bg);
        if (selected) {
            renderer.fillRect({tabRect.x, tabRect.y + tabRect.h - 3, tabRect.w, 3}, theme.accent);
        }
        renderer.drawRect(tabRect, theme.surfaceAlt, 1);

        Color textCol = selected ? theme.textPrimary : theme.textSecondary;
        Size sz = renderer.measureText(tabs_[i], theme.fontSmall);
        int tx = tabRect.x + (tabRect.w - sz.w) / 2;
        int ty = tabRect.y + (tabRect.h - sz.h) / 2;
        renderer.drawText(tabs_[i], {tx, ty}, theme.fontSmall, textCol);
    }
}

bool TabBarWidget::onButtonDown(Button b) {
    if (isDisabled() || tabs_.empty()) {
        return false;
    }

    if (b == Button::L1) {
        selectTab(selectedIndex_ - 1);
        return true;
    }
    if (b == Button::R1) {
        selectTab(selectedIndex_ + 1);
        return true;
    }

    if (b == Button::Up || b == Button::Down) {
        return false;
    }

    return false;
}

} // namespace hui
