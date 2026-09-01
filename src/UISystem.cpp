#include "hui/UISystem.h"
#include "hui/Shell.h"

namespace hui {

UISystem::UISystem(IRenderer& renderer, const Theme& theme)
    : renderer_(renderer),
      theme_(theme),
      focusManager_(),
      viewStack_(&focusManager_) {
}

void UISystem::onButtonDown(Button b) {
    auto chordOpt = chords_.onButtonDown(b);
    if (!chordOpt.has_value()) return;

    Button finalButton = *chordOpt;
    keyRepeat_.onButtonDown(finalButton);

    if (!suspended_) {
        viewStack_.dispatchButtonDown(finalButton, focusManager_);
    }
}

void UISystem::onButtonUp(Button b) {
    auto chordOpt = chords_.onButtonUp(b);
    if (!chordOpt.has_value()) return;

    Button finalButton = *chordOpt;
    keyRepeat_.onButtonUp(finalButton);

    if (!suspended_) {
        viewStack_.dispatchButtonUp(finalButton, focusManager_);
    }
}

void UISystem::update(float elapsedSeconds) {
    // Clamp dt to kMaxDelta (§10.1)
    float dt = elapsedSeconds;
    if (dt > kMaxDelta) dt = kMaxDelta;
    if (dt < 0.0f) dt = 0.0f;

    // Apply pending stack mutations at top of update and flush held keys if stack changed
    if (viewStack_.applyPendingMutations(focusManager_)) {
        keyRepeat_.flushHeld();
    }

    // Drive ChordDetector timer
    chords_.update(dt, [&](ButtonEvent e) {
        if (e.kind == ButtonEventKind::Down) {
            keyRepeat_.onButtonDown(e.button);
            if (!suspended_) {
                viewStack_.dispatchButtonDown(e.button, focusManager_);
            }
        }
    });

    // Drive KeyRepeatDriver (injects synthetic events)
    keyRepeat_.update(dt, [&](ButtonEvent e) {
        if (!suspended_) {
            viewStack_.dispatchButtonDown(e.button, focusManager_);
        }
    });

    // Apply pending mutations again in case an event handler or repeat triggered push/pop
    if (viewStack_.applyPendingMutations(focusManager_)) {
        keyRepeat_.flushHeld();
    }

    // Update ViewStack
    viewStack_.update(dt, focusManager_);

    if (shell_) {
        shell_->update(dt);
    }
}

void UISystem::draw() {
    if (suspended_) {
        if (clearPending_ > 0) {
            clearPending_--;
            Size screen = renderer_.screenSize();
            renderer_.fillRect({0, 0, screen.w, screen.h}, Color::black());
            renderer_.endFrame();
        }
        return;
    }

    if (shell_) {
        shell_->drawChrome(renderer_, theme_);
    }

    viewStack_.draw(renderer_, theme_);

    if (shell_) {
        shell_->drawOverlay(renderer_, theme_);
    }
}

void UISystem::setSuspended(bool s) {
    if (suspended_ == s) return;
    suspended_ = s;
    if (suspended_) {
        clearPending_ = 2;
    }
    keyRepeat_.flushHeld();
}

bool UISystem::isSuspended() const {
    return suspended_;
}

void UISystem::setShell(Shell* shell) {
    shell_ = shell;
    if (shell_) {
        viewStack_.setContentRect(shell_->contentRect());
    } else {
        Size screen = renderer_.screenSize();
        viewStack_.setContentRect({0, 0, screen.w, screen.h});
    }
}

Shell* UISystem::shell() const {
    return shell_;
}

void UISystem::setAnimationsEnabled(bool e) {
    animationsEnabled_ = e;
}

bool UISystem::animationsEnabled() const {
    return animationsEnabled_;
}

ViewStack& UISystem::viewStack() { return viewStack_; }
const ViewStack& UISystem::viewStack() const { return viewStack_; }

FocusManager& UISystem::focusManager() { return focusManager_; }
const FocusManager& UISystem::focusManager() const { return focusManager_; }

IRenderer& UISystem::renderer() { return renderer_; }
const Theme& UISystem::theme() const { return theme_; }

} // namespace hui
