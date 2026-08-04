#pragma once

#include "hui/types.h"
#include "hui/IRenderer.h"
#include "hui/FocusManager.h"
#include "hui/Widget.h"

#include <vector>

namespace hui {

// §8.1 View Base Class
//
// The application subclasses View to implement each screen (DirectoryView,
// LibraryView, NowPlayingView, ContextMenuView, etc.).
//
// Views own their child widgets and handle frame updates, rendering, and input routing.
// Focus state is saved and restored automatically via suspendFocus() / restoreFocus()
// when overlays are pushed and popped above this View.
class View {
public:
    virtual ~View() = default;

    // --- RTTI-free type identification for popTo<T>() ---
    template<typename T>
    static const void* typeIdFor() {
        static const char id = 0;
        return &id;
    }

    virtual const void* typeId() const {
        return typeIdFor<View>();
    }

    template<typename T>
    bool isType() const {
        return typeId() == typeIdFor<T>();
    }

    // --- Layout ---

    virtual void layout(Rect contentRect) { bounds_ = contentRect; }
    Rect bounds() const { return bounds_; }

    // --- Lifecycle Hooks ---

    // Called when this view becomes the new top of the ViewStack.
    virtual void onPush() {}

    // Called just before this view is destroyed or popped off the ViewStack.
    virtual void onPop() {}

    // Called when an overlay directly above this view is popped, restoring this view to the top.
    virtual void onResume() {}

    // Called when a new overlay view is pushed on top of this view.
    virtual void onSuspend() {}

    // --- Per-frame Methods ---

    // Called once per frame. dt = elapsed seconds since last call.
    virtual void update(float dt, FocusManager& fm) {
        (void)dt;
        (void)fm;
    }

    // Called once per frame to render the view.
    virtual void draw(IRenderer& renderer, const Theme& theme) = 0;

    // --- Input Handling ---

    // Returns true if the button press was consumed.
    virtual bool onButtonDown(Button b, FocusManager& fm) {
        (void)b;
        (void)fm;
        return false;
    }

    // Returns true if the button release was consumed.
    virtual bool onButtonUp(Button b, FocusManager& fm) {
        (void)b;
        (void)fm;
        return false;
    }

    // --- Dimming query ---

    // Returns true if pushing this view should cause ViewStack to draw a dark
    // scrim (theme.overlay) over all views beneath it.
    virtual bool dimsBelow() const { return false; }

    // --- Hint Bar Data ---

    // Reads current action hints published by this view for display on the HintBarWidget.
    virtual std::vector<HintEntry> currentHints() const { return {}; }

    // --- Focus Save / Restore ---

    // Records the currently focused widget before an overlay is pushed on top,
    // and clears focus on fm so the outgoing widget receives onBlur.
    void suspendFocus(FocusManager& fm) {
        savedFocus_ = fm.focused();
        fm.setFocus(nullptr);
    }

    // Restores focus to the saved widget when an overlay is popped.
    // Uses forceOwner so that no lifecycle callbacks are double-fired.
    virtual void restoreFocus(FocusManager& fm) {
        if (savedFocus_) {
            fm.forceOwner(savedFocus_);
        }
    }

    Widget* savedFocus() const { return savedFocus_; }

protected:
    Rect bounds_{0, 0, 0, 0};
    Widget* savedFocus_ = nullptr;
};

// Macro helper for subclass type identification (without RTTI / dynamic_cast)
#define HUI_VIEW_TYPE(Class) \
    const void* typeId() const override { return ::hui::View::typeIdFor<Class>(); }

} // namespace hui
