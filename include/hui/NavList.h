#pragma once

#include "hui/types.h"
#include "hui/Widget.h"
#include "hui/FocusManager.h"

#include <vector>
#include <cstddef>

namespace hui {

enum class Axis {
    Horizontal,
    Vertical
};

// §7.3 NavList
// Helper for ordered focus traversal across a static list of widgets.
// Does not own widgets, does not draw, and imposes no layout.
class NavList {
public:
    NavList() = default;

    void setAxis(Axis axis) { axis_ = axis; }
    Axis axis() const { return axis_; }

    void setWrap(bool wrap) { wrap_ = wrap; }
    bool wrap() const { return wrap_; }

    void add(Widget* w) {
        if (w) {
            widgets_.push_back(w);
        }
    }

    void clear() {
        widgets_.clear();
        currentIndex_ = -1;
    }

    size_t size() const { return widgets_.size(); }

    int index() const { return currentIndex_; }

    Widget* current() const {
        if (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(widgets_.size())) {
            return widgets_[currentIndex_];
        }
        return nullptr;
    }

    bool focusIndex(int index, FocusManager& fm) {
        if (index < 0 || index >= static_cast<int>(widgets_.size())) {
            return false;
        }
        Widget* target = widgets_[index];
        if (!target || !target->isFocusable() || target->isDisabled()) {
            return false;
        }
        if (fm.setFocus(target)) {
            currentIndex_ = index;
            return true;
        }
        return false;
    }

    bool handleButton(Button b, FocusManager& fm) {
        if (widgets_.empty()) return false;

        Button nextBtn = (axis_ == Axis::Vertical) ? Button::Down : Button::Right;
        Button prevBtn = (axis_ == Axis::Vertical) ? Button::Up   : Button::Left;

        if (b != nextBtn && b != prevBtn) {
            return false;
        }

        int count = static_cast<int>(widgets_.size());
        int start = currentIndex_;
        if (start < 0 || start >= count) {
            // Find first valid widget
            start = (b == nextBtn) ? -1 : count;
        }

        int dir = (b == nextBtn) ? 1 : -1;
        int step = 0;
        int nextIdx = start;

        while (step < count) {
            nextIdx += dir;
            step++;

            if (wrap_) {
                if (nextIdx < 0) nextIdx = count - 1;
                else if (nextIdx >= count) nextIdx = 0;
            } else {
                if (nextIdx < 0 || nextIdx >= count) {
                    // Reached boundary without wrapping; consume button if we were on a valid index or return true
                    return true;
                }
            }

            Widget* candidate = widgets_[nextIdx];
            if (candidate && candidate->isFocusable() && !candidate->isDisabled()) {
                if (fm.setFocus(candidate)) {
                    currentIndex_ = nextIdx;
                    return true;
                }
            }
        }

        return false;
    }

private:
    Axis axis_ = Axis::Vertical;
    bool wrap_ = true;
    std::vector<Widget*> widgets_;
    int currentIndex_ = -1;
};

} // namespace hui
