#pragma once

#include "hui/Widget.h"
#include "hui/types.h"
#include <cstdint>

namespace hui {

// Playback state representation
enum class PlaybackState : uint8_t {
    Stopped,
    Playing,
    Paused
};

// §12 PlaybackControlsRow
//
// Non-focusable visual row of transport icons reflecting playback state passed in.
class PlaybackControlsRow : public Widget {
public:
    explicit PlaybackControlsRow(PlaybackState state = PlaybackState::Stopped);

    bool isFocusable() const override { return false; }

    void setPlaybackState(PlaybackState state) { state_ = state; }
    PlaybackState playbackState() const { return state_; }

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    PlaybackState state_ = PlaybackState::Stopped;
};

} // namespace hui
