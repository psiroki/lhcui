#pragma once

#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/ListView.h"
#include "hui/ListSource.h"
#include <functional>
#include <string>

namespace hui {

// §12 ContextMenuView
//
// Overlay action list backed by VectorListSource. B dismisses.
class ContextMenuView : public View {
public:
    HUI_VIEW_TYPE(ContextMenuView)

    explicit ContextMenuView(ViewStack& stack);

    bool dimsBelow() const override { return true; }

    VectorListSource& source() { return source_; }

    void setOnAction(std::function<void(int index)> cb) { onAction_ = std::move(cb); }
    void setOnCancel(std::function<void()> cb) { onCancel_ = std::move(cb); }

    void layout(Rect contentRect) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b, FocusManager& fm) override;
    void restoreFocus(FocusManager& fm) override;

    std::vector<HintEntry> currentHints() const override;

private:
    ViewStack& stack_;
    VectorListSource source_;
    ListView list_;
    std::function<void(int index)> onAction_;
    std::function<void()> onCancel_;
};

} // namespace hui
