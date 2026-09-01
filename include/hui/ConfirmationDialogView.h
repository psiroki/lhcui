#pragma once

#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/NavList.h"
#include "hui/Widget.h"
#include <functional>
#include <string>

namespace hui {

// §12 ConfirmationDialogView
//
// Overlay with Cancel / Confirm buttons. NavList horizontal, wrap off.
class ConfirmationDialogView : public View {
public:
    HUI_VIEW_TYPE(ConfirmationDialogView)

    explicit ConfirmationDialogView(ViewStack& stack,
                                    std::string message = {});

    bool dimsBelow() const override { return true; }

    void setOnConfirm(std::function<void()> cb) { onConfirm_ = std::move(cb); }
    void setOnCancel(std::function<void()> cb) { onCancel_ = std::move(cb); }

    void layout(Rect contentRect) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b, FocusManager& fm) override;
    void restoreFocus(FocusManager& fm) override;

    std::vector<HintEntry> currentHints() const override;

private:
    class DialogButton : public Widget {
    public:
        explicit DialogButton(std::string label) : label_(std::move(label)) {}

        bool isFocusable() const override { return true; }

        void setLabel(std::string label) { label_ = std::move(label); }

        void draw(IRenderer& renderer, const Theme& theme) override;

    private:
        std::string label_;
    };

    void activateFocused(FocusManager& fm);

    ViewStack& stack_;
    std::string message_;
    DialogButton cancelButton_{"Cancel"};
    DialogButton confirmButton_{"Confirm"};
    NavList nav_;
    std::function<void()> onConfirm_;
    std::function<void()> onCancel_;
};

} // namespace hui
