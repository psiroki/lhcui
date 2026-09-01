#pragma once

#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/LetterWheel.h"
#include <functional>

namespace hui {

// Overlay wrapper for LetterWheel (§12).
class LetterWheelView : public View {
public:
    HUI_VIEW_TYPE(LetterWheelView)

    explicit LetterWheelView(ViewStack& stack);

    bool dimsBelow() const override { return true; }

    LetterWheel& letterWheel() { return letterWheel_; }
    const LetterWheel& letterWheel() const { return letterWheel_; }

    void setOnDismiss(std::function<void()> cb) { onDismiss_ = std::move(cb); }

    void layout(Rect contentRect) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b, FocusManager& fm) override;
    void restoreFocus(FocusManager& fm) override;

    std::vector<HintEntry> currentHints() const override;

private:
    ViewStack& stack_;
    LetterWheel letterWheel_;
    std::function<void()> onDismiss_;
};

} // namespace hui
