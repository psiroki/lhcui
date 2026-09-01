#pragma once

#include "hui/Widget.h"
#include "hui/ShuffleToggle.h"
#include "hui/RepeatModeToggle.h"
#include "hui/types.h"
#include <cstdint>
#include <functional>

namespace hui {

enum class PlaybackState : uint8_t {
    Paused,
    Playing
};

enum class TransportAction : uint8_t {
    Previous,
    PlayPause,
    Next,
    Shuffle,
    Repeat
};

// §12 PlaybackControlsRow
//
// Focusable horizontal group of five transport segments: previous, play/pause,
// next, shuffle, repeat. Left/Right moves the selected segment; A activates it.
class PlaybackControlsRow : public Widget {
public:
    explicit PlaybackControlsRow(PlaybackState state = PlaybackState::Paused);

    bool isFocusable() const override { return true; }

    void setPlaybackState(PlaybackState state) { state_ = state; }
    PlaybackState playbackState() const { return state_; }

    void setShuffle(bool shuffle) { shuffleToggle_.setShuffle(shuffle); }
    void setRepeatMode(RepeatMode mode) { repeatToggle_.setMode(mode); }

    void setOnActivate(std::function<void(TransportAction)> cb) { onActivate_ = std::move(cb); }

    bool onButtonDown(Button b) override;

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    void layoutSegments();
    Rect segmentRect(int index) const;
    void drawSegment(IRenderer& renderer, const Theme& theme, int index, const Rect& rect);

    PlaybackState state_ = PlaybackState::Paused;
    int selectedSegment_ = 1;
    ShuffleToggle shuffleToggle_;
    RepeatModeToggle repeatToggle_;
    std::function<void(TransportAction)> onActivate_;
    int segmentWidth_ = 0;
};

} // namespace hui
