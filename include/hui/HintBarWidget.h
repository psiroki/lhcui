#pragma once

#include "hui/Widget.h"
#include "hui/ViewStack.h"
#include "hui/types.h"
#include <vector>

namespace hui {

// §12 HintBarWidget
//
// Non-focusable bar at screen bottom. Reads currentHints() from the active View each frame.
// Enforces display ordering (sorted by sortOrder), middle-truncation (cap at 5), and
// color-coded button glyphs.
class HintBarWidget : public Widget {
public:
    explicit HintBarWidget(const ViewStack* stack = nullptr);

    bool isFocusable() const override { return false; }

    void setViewStack(const ViewStack* stack) { stack_ = stack; }
    const ViewStack* viewStack() const { return stack_; }

    void setHints(std::vector<HintEntry> hints) {
        explicitHints_ = std::move(hints);
        useExplicitHints_ = true;
    }

    void clearExplicitHints() {
        explicitHints_.clear();
        useExplicitHints_ = false;
    }

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    const ViewStack* stack_ = nullptr;
    std::vector<HintEntry> explicitHints_;
    bool useExplicitHints_ = false;
};

} // namespace hui
