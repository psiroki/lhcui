#pragma once

#include "hui/ChordDetector.h"
#include "hui/FocusManager.h"
#include "hui/IRenderer.h"
#include "hui/KeyRepeatDriver.h"
#include "hui/ViewStack.h"
#include "hui/types.h"

#include <functional>

namespace hui {

class Shell;

class UISystem {
public:
    static constexpr float kMaxDelta = 0.100f; // Seconds (§10.1)

    UISystem(IRenderer& renderer, const Theme& theme);
    ~UISystem() = default;

    // UISystem is non-copyable
    UISystem(const UISystem&) = delete;
    UISystem& operator=(const UISystem&) = delete;
    UISystem(UISystem&&) = delete;
    UISystem& operator=(UISystem&&) = delete;

    // Called by the application once per frame.
    // elapsedSeconds: wall-clock time since the previous call, CLAMPED
    // internally to kMaxDelta before anything sees it.
    // Drives: pending stack mutations, KeyRepeatDriver, ViewStack::update.
    void update(float elapsedSeconds);

    // Called by the application once per frame, after update().
    // Draw order: Shell chrome -> view stack -> Shell overlay layer.
    void draw();

    // Input — called by the application from its SDL event loop.
    void onButtonDown(Button b);
    void onButtonUp(Button b);

    // --- Global accelerators (§9.3, §9.5) ---
    // Last-resort handler for buttons the top View left unhandled. Suppressed
    // while an overlay (dimsBelow() == true) is on top: an overlay owns every
    // button for as long as it is up, including the ones it chooses to ignore.
    // A pushed screen is not an overlay and does not suppress it.
    // Returns true if the accelerator consumed the button.
    void setGlobalAccelerator(std::function<bool(Button)> cb);

    // --- Screen-off support (§10.1) ---
    void setSuspended(bool s);
    bool isSuspended() const;

    // --- Shell chrome (§12) ---
    // Optional. Nullable. Not owned. Setting it also wires the Shell's content
    // rect into the view stack.
    void setShell(Shell* shell);
    Shell* shell() const;

    void setAnimationsEnabled(bool e);
    bool animationsEnabled() const;

    ViewStack& viewStack();
    const ViewStack& viewStack() const;

    FocusManager& focusManager();
    const FocusManager& focusManager() const;

    IRenderer& renderer();
    const Theme& theme() const;

private:
    // Routes a Down through the stack, falling back to the global accelerator.
    void dispatchDown(Button b);

    IRenderer& renderer_;
    const Theme& theme_;
    FocusManager focusManager_;
    ViewStack viewStack_;
    std::function<bool(Button)> globalAccelerator_;
    KeyRepeatDriver keyRepeat_;
    ChordDetector chords_;
    Shell* shell_ = nullptr;
    bool suspended_ = false;
    int clearPending_ = 0;
    bool animationsEnabled_ = true;
};

} // namespace hui
