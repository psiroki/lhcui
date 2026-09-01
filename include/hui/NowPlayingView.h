#pragma once

#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/NavList.h"
#include "hui/SeekableProgressBar.h"
#include "hui/PlaybackControlsRow.h"
#include "hui/QueueList.h"
#include "hui/ListSource.h"
#include "hui/TrackInfoPanelView.h"
#include <functional>

namespace hui {

// §12 NowPlayingView
//
// Playback screen with seek bar, transport row, and queue list.
class NowPlayingView : public View {
public:
    HUI_VIEW_TYPE(NowPlayingView)

    explicit NowPlayingView(ViewStack& stack);

    void setQueueSource(IListSource* source);

    SeekableProgressBar& seekBar() { return seekBar_; }
    PlaybackControlsRow& transport() { return transport_; }
    QueueList& queue() { return queue_; }

    void setPlaybackState(PlaybackState state);
    PlaybackState playbackState() const { return transport_.playbackState(); }

    void setProgress(float ratio, float elapsedSeconds, float totalSeconds);
    void setShuffle(bool shuffle) { transport_.setShuffle(shuffle); }
    void setRepeatMode(RepeatMode mode) { transport_.setRepeatMode(mode); }

    void setOnSeek(std::function<void(int direction)> cb);
    void setOnTransport(std::function<void(TransportAction)> cb);
    void setOnQueueActivate(std::function<void(int index)> cb) { onQueueActivate_ = std::move(cb); }
    void setOnBuildContextMenu(std::function<void(int index, VectorListSource& menu)> cb) {
        onBuildContextMenu_ = std::move(cb);
    }
    void setOnContextAction(std::function<void(int menuIndex, int itemIndex)> cb) {
        onContextAction_ = std::move(cb);
    }
    void setTrackMetadataProvider(std::function<TrackMetadata(int index)> cb) {
        trackMetadataProvider_ = std::move(cb);
    }

    void layout(Rect contentRect) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b, FocusManager& fm) override;
    void restoreFocus(FocusManager& fm) override;

    std::vector<HintEntry> currentHints() const override;

private:
    void openContextMenu();
    void openTrackInfo(int index);
    void syncNavFocus(FocusManager& fm);
    bool handleGlobalAccelerators(Button b);

    ViewStack& stack_;
    SeekableProgressBar seekBar_;
    PlaybackControlsRow transport_;
    QueueList queue_;
    NavList nav_;
    std::function<void(int index)> onQueueActivate_;
    std::function<void(int index, VectorListSource& menu)> onBuildContextMenu_;
    std::function<void(int menuIndex, int itemIndex)> onContextAction_;
    std::function<TrackMetadata(int index)> trackMetadataProvider_;
    std::function<void(TransportAction)> onTransport_;
};

} // namespace hui
