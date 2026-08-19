#pragma once

#include "hui/Widget.h"

namespace hui {

// §12 ShuffleToggle
//
// Non-focusable icon with on/off visual state.
class ShuffleToggle : public Widget {
public:
    explicit ShuffleToggle(bool shuffle = false);

    bool isFocusable() const override { return false; }

    void setShuffle(bool shuffle) { shuffle_ = shuffle; }
    bool isShuffle() const { return shuffle_; }

    void toggle() { shuffle_ = !shuffle_; }

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    bool shuffle_ = false;
};

} // namespace hui
