#pragma once

#include "hui/ProgressBar.h"
#include <functional>

namespace hui {

// §12 SeekableProgressBar
//
// Extends ProgressBar; consumes Left/Right while focused and L2/R2 whenever
// the owning view routes them, calling onSeek in both cases.
// Returns false for Up/Down so the view can move focus off it.
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
