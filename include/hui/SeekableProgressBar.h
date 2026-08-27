#pragma once

#include "hui/ProgressBar.h"
#include <functional>

namespace hui {

// §12 SeekableProgressBar
//
// Extends ProgressBar; consumes L2/R2 button events and calls onSeek callback.
// Focusable (isFocusable() == true). Returns false for all non-seek buttons.
class SeekableProgressBar : public ProgressBar {
public:
    SeekableProgressBar() = default;

    bool isFocusable() const override { return true; }

    void setOnSeek(std::function<void(int direction)> cb) { onSeek_ = std::move(cb); }

    bool onButtonDown(Button b) override;

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    std::function<void(int direction)> onSeek_;
};

} // namespace hui
