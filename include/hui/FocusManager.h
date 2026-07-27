#pragma once

#include "hui/Widget.h"

namespace hui {

// §7.1 FocusManager
//
// Focus is always held by exactly one widget (or nullptr when nothing is focused).
//
// setFocus() is the normal path: it fires onBlur on the previous owner and
// onFocus on the new one.
//
// forceOwner() is used by ViewStack when a View resumes (comes back to the top
// of the stack after an overlay is popped). It sets the current focus pointer
// WITHOUT firing any lifecycle callbacks, because the View has already saved
// and recorded the focus state itself.
class FocusManager {
public:
    // Give focus to w.
    //   - If w == current_: no-op (does not double-fire callbacks).
    //   - If current_ != nullptr: calls current_->setFocused(false).
    //   - If w != nullptr: calls w->setFocused(true).
    //   - Passing nullptr clears focus (only onBlur is fired on the previous owner).
    void setFocus(Widget* w) {
        if (w == current_) return;

        if (current_) {
            current_->setFocused(false);
        }
        current_ = w;
        if (current_) {
            current_->setFocused(true);
        }
    }

    // Returns the currently focused widget, or nullptr if nothing is focused.
    Widget* focused() const { return current_; }

    // Convenience: returns true when w is the current focus owner.
    bool hasFocus(const Widget* w) const { return current_ == w; }

    // Sets the current focus pointer without firing any lifecycle callbacks.
    // Used by ViewStack::resume() so that a returning view can restore its
    // saved focus without triggering a spurious onFocus/onBlur pair.
    void forceOwner(Widget* w) {
        if (current_ && current_ != w) {
            current_->focused_ = false;
        }
        current_ = w;
        if (current_) {
            current_->focused_ = true;
        }
    }

private:
    Widget* current_ = nullptr;
};

} // namespace hui
