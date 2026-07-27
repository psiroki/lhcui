#pragma once

#include "hui/types.h"
#include "hui/IRenderer.h"

namespace hui {

class FocusManager; // forward declaration for friend

// §6.1 Widget Base Class
//
// Widgets are immediate-mode at the draw level: they receive their bounding
// Rect on every draw() call. There is no retained layout tree.
//
// Focus state is managed exclusively by FocusManager, which is a friend class.
// Widgets must not call setFocused() themselves.
//
// Internal focus indices (e.g. in ListView) must NOT be reset on onBlur();
// they persist until explicitly reset by the owning View.
class Widget {
public:
    virtual ~Widget() = default;

    // Called once per frame. dt = elapsed seconds since last call.
    virtual void update(float dt) { (void)dt; }

    // Called once per frame after update(). r is the rect this widget occupies.
    virtual void draw(IRenderer& renderer, Rect r, const Theme& theme) = 0;

    // Event dispatch. Returns true if the event was consumed (stops propagation).
    virtual bool onButtonDown(Button b) { (void)b; return false; }
    virtual bool onButtonUp(Button b)   { (void)b; return false; }

    // Focus lifecycle callbacks. Subclasses override to react to focus changes.
    // Called by setFocused(), which is only invoked by FocusManager.
    virtual void onFocus() {}
    virtual void onBlur()  {}

    // --- Accessors ---

    bool isFocused()  const { return focused_; }
    bool isDisabled() const { return disabled_; }

    void setDisabled(bool d) { disabled_ = d; }

private:
    bool focused_  = false;
    bool disabled_ = false;

    // Only FocusManager may toggle focus state; keeps the lifecycle callbacks
    // from being called by accident from widget code.
    friend class FocusManager;

    void setFocused(bool f) {
        focused_ = f;
        if (f) onFocus();
        else   onBlur();
    }
};

} // namespace hui
