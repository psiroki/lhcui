#pragma once

#include "hui/Widget.h"

namespace hui {

// §7.1 FocusManager
//
// Focus is always held by at most one widget (or nullptr when nothing is focused).
//
// setFocus() is the normal path: it fires onBlur on the previous owner and
// onFocus on the new one via setFocusedAndNotify().
//
// forceOwner() is used by ViewStack when a View resumes. It sets the focus flag
// on both outgoing and incoming widgets via setFocusedFlag() WITHOUT firing callbacks.
class FocusManager {
public:
    // Give focus to w. Returns true on success, false if w is non-focusable or disabled.
    // Passing nullptr always succeeds (clears focus).
    bool setFocus(Widget* w) {
        if (w != nullptr && (!w->isFocusable() || w->isDisabled())) {
            return false;
        }
        if (w == current_) return true;

        if (current_) {
            current_->setFocusedAndNotify(false);
        }
        current_ = w;
        if (current_) {
            current_->setFocusedAndNotify(true);
        }
        return true;
    }

    // Returns the currently focused widget, or nullptr if nothing is focused.
    Widget* focused() const { return current_; }

    // Convenience: returns true when w is the current focus owner.
    bool hasFocus(const Widget* w) const { return current_ == w; }

    // Sets the focus owner without firing any lifecycle callbacks.
    // Returns true on success, false if w is non-focusable or disabled.
    bool forceOwner(Widget* w) {
        if (w != nullptr && (!w->isFocusable() || w->isDisabled())) {
            return false;
        }
        if (w == current_) return true;

        if (current_) {
            current_->setFocusedFlag(false);
        }
        current_ = w;
        if (current_) {
            current_->setFocusedFlag(true);
        }
        return true;
    }

private:
    Widget* current_ = nullptr;
};

} // namespace hui
