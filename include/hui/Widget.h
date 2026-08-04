#pragma once

#include "hui/types.h"
#include "hui/IRenderer.h"

namespace hui {

class FocusManager; // forward declaration for friend

// §6.1 Widget Base Class
//
// Retained layout: Widget stores its bounds_ set by layout(Rect).
// Draw receives no Rect parameter.
//
// Focus state is managed exclusively by FocusManager, which is a friend class.
// Widgets must not call focus setters themselves.
class Widget {
public:
    virtual ~Widget() = default;

    // Layout assignment. Sets bounds_ for drawing and hit/input queries.
    virtual void layout(Rect r) { bounds_ = r; }
    Rect bounds() const { return bounds_; }

    // Called once per frame. dt = elapsed seconds since last call.
    virtual void update(float dt) { (void)dt; }

    // Called once per frame after update(). Draws within bounds_.
    virtual void draw(IRenderer& renderer, const Theme& theme) = 0;

    // Focus eligibility query. Default is false (non-focusable).
    virtual bool isFocusable() const { return false; }

    // Event dispatch. Returns true if the event was consumed (stops propagation).
    virtual bool onButtonDown(Button b) { (void)b; return false; }
    virtual bool onButtonUp(Button b)   { (void)b; return false; }

    // Focus lifecycle callbacks. Subclasses override to react to focus changes.
    virtual void onFocus() {}
    virtual void onBlur()  {}

    // --- Accessors ---

    bool isFocused()  const { return focused_; }
    bool isDisabled() const { return disabled_; }

    void setDisabled(bool d) { disabled_ = d; }

protected:
    Rect bounds_{0, 0, 0, 0};

private:
    bool focused_  = false;
    bool disabled_ = false;

    // Only FocusManager may toggle focus state; keeps the lifecycle callbacks
    // from being called by accident from widget code.
    friend class FocusManager;

    void setFocusedFlag(bool f) {
        focused_ = f;
    }

    void setFocusedAndNotify(bool f) {
        focused_ = f;
        if (f) onFocus();
        else   onBlur();
    }
};

} // namespace hui
