#pragma once

#include "hui/Widget.h"
#include <cstdint>

namespace hui {

// §12 RepeatModeToggle
//
// Non-focusable icon cycling off -> all -> one -> off.
enum class RepeatMode : uint8_t {
    Off,
    All,
    One
};

class RepeatModeToggle : public Widget {
public:
    explicit RepeatModeToggle(RepeatMode mode = RepeatMode::Off);

    bool isFocusable() const override { return false; }

    void setMode(RepeatMode mode) { mode_ = mode; }
    RepeatMode mode() const { return mode_; }

    void cycle();

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    RepeatMode mode_ = RepeatMode::Off;
};

} // namespace hui
