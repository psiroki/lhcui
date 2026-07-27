#pragma once

#include "hui/View.h"
#include "hui/FocusManager.h"
#include "hui/IRenderer.h"
#include "hui/types.h"

#include <memory>
#include <vector>

namespace hui {

// §8.4 Screen Transitions
enum class TransitionKind {
    None,
    SlideLeft,
    SlideRight,
    FadeThrough
};

struct SimpleTransition {
    TransitionKind kind = TransitionKind::None;
    float duration = 0.2f; // seconds
    float progress = 0.0f; // 0.0f to 1.0f

    bool isComplete() const { return progress >= 1.0f; }

    void update(float dt) {
        if (duration > 0.0f) {
            progress += dt / duration;
            if (progress > 1.0f) progress = 1.0f;
        } else {
            progress = 1.0f;
        }
    }

    void reset() {
        progress = 0.0f;
    }
};

// §8.2 ViewStack
//
// Manages a stack of View objects.
//
// All views in the stack are drawn bottom-to-top so that overlay views (context menus,
// guide panel) render above base screens.
//
// Alpha dimming is applied via setGlobalAlpha(128) to all non-top views when stack depth > 1,
// and setGlobalAlpha(255) is restored before drawing the top view.
//
// Only the topmost view receives input events via dispatchButtonDown/Up.
class ViewStack {
public:
    explicit ViewStack(FocusManager* fm = nullptr) : fm_(fm) {}
    ~ViewStack() = default;

    // ViewStack is move-only (owns unique_ptr)
    ViewStack(const ViewStack&) = delete;
    ViewStack& operator=(const ViewStack&) = delete;
    ViewStack(ViewStack&&) = default;
    ViewStack& operator=(ViewStack&&) = default;

    void setFocusManager(FocusManager* fm) { fm_ = fm; }
    FocusManager* focusManager() const { return fm_; }

    // Pushes a new view onto the stack.
    // Calls onSuspend() (and suspendFocus if FocusManager is available) on previous top view.
    // Takes unique_ptr ownership. Calls onPush() on the new view.
    void push(std::unique_ptr<View> view, FocusManager* fm = nullptr);

    // Pops the top view off the stack.
    // Calls onPop() on the removed view, then onResume() (and restoreFocus if FocusManager is available) on the view below.
    // No-op if the stack has 1 or 0 entries.
    void pop(FocusManager* fm = nullptr);

    // Pops views until a view of type T is at the top of the stack or only 1 view remains.
    template<typename T>
    void popTo(FocusManager* fm = nullptr) {
        FocusManager* activeFm = fm ? fm : fm_;
        while (stack_.size() > 1 && !stack_.back()->template isType<T>()) {
            pop(activeFm);
        }
    }

    // Replaces the current top view with a new view (atomic pop + push).
    // Clears focus on current view, calls onPop() on the old top view and onPush() on the new view.
    void replace(std::unique_ptr<View> view, FocusManager* fm = nullptr);

    // Returns pointer to the current top view, or nullptr if stack is empty.
    View* top() const {
        return stack_.empty() ? nullptr : stack_.back().get();
    }

    bool empty() const { return stack_.empty(); }
    size_t size() const { return stack_.size(); }

    // Drives update(dt, fm) on all views currently in the stack (bottom to top).
    void update(float dt, FocusManager& fm);

    // Draws all views bottom-to-top.
    // Applies setGlobalAlpha(128) and setDimmed(true) to non-top views when depth > 1,
    // and setGlobalAlpha(255) and setDimmed(false) to the top view.
    void draw(IRenderer& renderer, const Theme& theme);

    // Delivers button event strictly to top() view.
    bool dispatchButtonDown(Button b, FocusManager& fm);
    bool dispatchButtonUp  (Button b, FocusManager& fm);

private:
    std::vector<std::unique_ptr<View>> stack_;
    FocusManager* fm_ = nullptr;
};

} // namespace hui
