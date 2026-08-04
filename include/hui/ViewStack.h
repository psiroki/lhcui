#pragma once

#include "hui/View.h"
#include "hui/FocusManager.h"
#include "hui/IRenderer.h"
#include "hui/types.h"

#include <memory>
#include <vector>

namespace hui {

// §8.2 ViewStack
//
// Manages a stack of View objects.
//
// All mutations (push, pop, popTo, replace) are deferred until applyPendingMutations()
// is called. This prevents use-after-free defects when views trigger navigation
// from inside event handlers.
//
// Views are drawn bottom-to-top. Any view returning dimsBelow() == true triggers
// a single full-screen fill (theme.overlay) drawn immediately before that view.
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

    // Content area rect passed to stacked views via layout(contentRect_)
    void setContentRect(Rect r);
    Rect contentRect() const { return contentRect_; }

    // Enqueues a push mutation.
    void push(std::unique_ptr<View> view);

    // Enqueues a pop mutation.
    void pop();

    // Enqueues a popTo mutation.
    template<typename T>
    void popTo() {
        popToType(View::typeIdFor<T>());
    }

    void popToType(const void* typeId);

    // Enqueues a replace mutation.
    void replace(std::unique_ptr<View> view);

    // Applies all queued mutations. Returns true if the stack changed.
    bool applyPendingMutations(FocusManager& fm);
    bool applyPendingMutations();
    bool hasPendingMutations() const { return !pendingMutations_.empty(); }

    // Returns pointer to the current top view, or nullptr if stack is empty.
    View* top() const {
        return stack_.empty() ? nullptr : stack_.back().get();
    }

    bool empty() const { return stack_.empty(); }
    size_t size() const { return stack_.size(); }
    size_t depth() const { return stack_.size(); }

    // Drives update(dt, fm) on all views currently in the stack (bottom to top).
    void update(float dt, FocusManager& fm);

    // Draws all views bottom-to-top.
    void draw(IRenderer& renderer, const Theme& theme);

    // Delivers button event strictly to top() view.
    bool dispatchButtonDown(Button b, FocusManager& fm);
    bool dispatchButtonUp  (Button b, FocusManager& fm);

private:
    enum class MutationType { Push, Pop, PopTo, Replace };

    struct PendingMutation {
        MutationType type;
        std::unique_ptr<View> view;
        const void* targetTypeId = nullptr;
    };

    std::vector<std::unique_ptr<View>> stack_;
    std::vector<PendingMutation> pendingMutations_;
    FocusManager* fm_ = nullptr;
    Rect contentRect_{0, 0, 640, 480};
};

} // namespace hui
