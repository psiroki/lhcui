#pragma once

#include "hui/View.h"
#include "hui/ViewStack.h"
#include <string>
#include <vector>

namespace hui {

struct TrackMetadata {
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string year;
    std::string duration;
    std::string bitrate;
    std::string format;
};

// §12 TrackInfoPanelView
//
// Overlay read-only metadata table. B dismisses.
class TrackInfoPanelView : public View {
public:
    HUI_VIEW_TYPE(TrackInfoPanelView)

    explicit TrackInfoPanelView(ViewStack& stack, TrackMetadata metadata = {});

    bool dimsBelow() const override { return true; }

    void setMetadata(TrackMetadata metadata) { metadata_ = std::move(metadata); }

    void layout(Rect contentRect) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b, FocusManager& fm) override;

    std::vector<HintEntry> currentHints() const override;

private:
    ViewStack& stack_;
    TrackMetadata metadata_;
};

} // namespace hui
