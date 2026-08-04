#include "hui/ViewStack.h"

namespace hui {

void ViewStack::setContentRect(Rect r) {
    contentRect_ = r;
    for (auto& view : stack_) {
        if (view) {
            view->layout(contentRect_);
        }
    }
}

void ViewStack::push(std::unique_ptr<View> view) {
    if (!view) return;
    pendingMutations_.push_back({MutationType::Push, std::move(view), nullptr});
}

void ViewStack::pop() {
    pendingMutations_.push_back({MutationType::Pop, nullptr, nullptr});
}

void ViewStack::popToType(const void* typeId) {
    pendingMutations_.push_back({MutationType::PopTo, nullptr, typeId});
}

void ViewStack::replace(std::unique_ptr<View> view) {
    if (!view) return;
    pendingMutations_.push_back({MutationType::Replace, std::move(view), nullptr});
}

bool ViewStack::applyPendingMutations(FocusManager& fm) {
    fm_ = &fm;
    if (pendingMutations_.empty()) return false;

    std::vector<PendingMutation> queue = std::move(pendingMutations_);
    pendingMutations_.clear();

    bool changed = false;

    for (auto& m : queue) {
        switch (m.type) {
            case MutationType::Push: {
                if (!m.view) break;
                if (!stack_.empty()) {
                    stack_.back()->suspendFocus(fm);
                    stack_.back()->onSuspend();
                }
                m.view->onPush();
                m.view->layout(contentRect_);
                stack_.push_back(std::move(m.view));
                stack_.back()->restoreFocus(fm);
                changed = true;
                break;
            }
            case MutationType::Pop: {
                if (stack_.size() <= 1) break;
                stack_.back()->suspendFocus(fm);
                std::unique_ptr<View> oldTop = std::move(stack_.back());
                stack_.pop_back();
                oldTop->onPop();
                oldTop.reset();

                stack_.back()->onResume();
                stack_.back()->restoreFocus(fm);
                changed = true;
                break;
            }
            case MutationType::PopTo: {
                bool poppedAny = false;
                while (stack_.size() > 1 && stack_.back()->typeId() != m.targetTypeId) {
                    stack_.back()->suspendFocus(fm);
                    std::unique_ptr<View> oldTop = std::move(stack_.back());
                    stack_.pop_back();
                    oldTop->onPop();
                    oldTop.reset();
                    poppedAny = true;
                }
                if (poppedAny && !stack_.empty()) {
                    stack_.back()->onResume();
                    stack_.back()->restoreFocus(fm);
                    changed = true;
                }
                break;
            }
            case MutationType::Replace: {
                if (!m.view) break;
                if (!stack_.empty()) {
                    stack_.back()->suspendFocus(fm);
                    std::unique_ptr<View> oldTop = std::move(stack_.back());
                    stack_.pop_back();
                    oldTop->onPop();
                    oldTop.reset();
                }
                m.view->onPush();
                m.view->layout(contentRect_);
                stack_.push_back(std::move(m.view));
                stack_.back()->restoreFocus(fm);
                changed = true;
                break;
            }
        }
    }

    return changed;
}

bool ViewStack::applyPendingMutations() {
    if (fm_) {
        return applyPendingMutations(*fm_);
    }
    return false;
}

void ViewStack::update(float dt, FocusManager& fm) {
    fm_ = &fm;
    applyPendingMutations(fm);

    for (size_t i = 0; i < stack_.size(); ++i) {
        stack_[i]->update(dt, fm);
    }
}

void ViewStack::draw(IRenderer& renderer, const Theme& theme) {
    if (stack_.empty()) return;

    Size screen = renderer.screenSize();
    Rect fullScreen{0, 0, screen.w, screen.h};

    for (size_t i = 0; i < stack_.size(); ++i) {
        if (i > 0 && stack_[i]->dimsBelow()) {
            renderer.fillRect(fullScreen, theme.overlay);
        }
        stack_[i]->draw(renderer, theme);
    }
}

bool ViewStack::dispatchButtonDown(Button b, FocusManager& fm) {
    fm_ = &fm;
    if (stack_.empty()) return false;
    bool res = stack_.back()->onButtonDown(b, fm);
    applyPendingMutations(fm);
    return res;
}

bool ViewStack::dispatchButtonUp(Button b, FocusManager& fm) {
    fm_ = &fm;
    if (stack_.empty()) return false;
    bool res = stack_.back()->onButtonUp(b, fm);
    applyPendingMutations(fm);
    return res;
}

} // namespace hui
