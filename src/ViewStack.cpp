#include "hui/ViewStack.h"

namespace hui {

void ViewStack::push(std::unique_ptr<View> view, FocusManager* fm) {
    if (!view) return;
    FocusManager* activeFm = fm ? fm : fm_;

    if (!stack_.empty()) {
        stack_.back()->onSuspend();
        if (activeFm) {
            stack_.back()->suspendFocus(*activeFm);
        }
    }

    stack_.push_back(std::move(view));
    stack_.back()->onPush();
}

void ViewStack::pop(FocusManager* fm) {
    if (stack_.size() <= 1) {
        return; // no-op on single-entry or empty stack
    }
    FocusManager* activeFm = fm ? fm : fm_;

    std::unique_ptr<View> oldTop = std::move(stack_.back());
    stack_.pop_back();
    oldTop->onPop();

    View* newTop = stack_.back().get();
    newTop->onResume();
    if (activeFm) {
        newTop->restoreFocus(*activeFm);
    }
}

void ViewStack::replace(std::unique_ptr<View> view, FocusManager* fm) {
    if (!view) return;
    FocusManager* activeFm = fm ? fm : fm_;

    if (stack_.empty()) {
        push(std::move(view), activeFm);
        return;
    }

    if (activeFm) {
        activeFm->setFocus(nullptr);
    }

    std::unique_ptr<View> oldTop = std::move(stack_.back());
    stack_.pop_back();
    oldTop->onPop();

    stack_.push_back(std::move(view));
    stack_.back()->onPush();
}

void ViewStack::update(float dt, FocusManager& fm) {
    if (!fm_) {
        fm_ = &fm;
    }
    for (size_t i = 0; i < stack_.size(); ++i) {
        stack_[i]->update(dt, fm);
    }
}

void ViewStack::draw(IRenderer& renderer, const Theme& theme) {
    if (stack_.empty()) return;

    if (stack_.size() == 1) {
        stack_[0]->setDimmed(false);
        renderer.setGlobalAlpha(255);
        stack_[0]->draw(renderer, theme);
    } else {
        // Draw non-top views bottom-to-top with dimming (global alpha 128)
        renderer.setGlobalAlpha(128);
        for (size_t i = 0; i < stack_.size() - 1; ++i) {
            stack_[i]->setDimmed(true);
            stack_[i]->draw(renderer, theme);
        }

        // Draw top view at full opacity (global alpha 255)
        renderer.setGlobalAlpha(255);
        stack_.back()->setDimmed(false);
        stack_.back()->draw(renderer, theme);
    }
}

bool ViewStack::dispatchButtonDown(Button b, FocusManager& fm) {
    if (!fm_) {
        fm_ = &fm;
    }
    if (stack_.empty()) return false;
    return stack_.back()->onButtonDown(b, fm);
}

bool ViewStack::dispatchButtonUp(Button b, FocusManager& fm) {
    if (!fm_) {
        fm_ = &fm;
    }
    if (stack_.empty()) return false;
    return stack_.back()->onButtonUp(b, fm);
}

} // namespace hui
