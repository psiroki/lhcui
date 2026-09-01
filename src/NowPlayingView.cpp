#include "hui/NowPlayingView.h"
#include "hui/ContextMenuView.h"

namespace hui {

NowPlayingView::NowPlayingView(ViewStack& stack)
    : stack_(stack)
    , queue_(36) {
    nav_.setAxis(Axis::Vertical);
    nav_.setWrap(true);
    nav_.add(&seekBar_);
    nav_.add(&transport_);
    nav_.add(&queue_);

    queue_.setOnActivate([this](int index) {
        if (onQueueActivate_) {
            onQueueActivate_(index);
        }
    });
}

void NowPlayingView::setQueueSource(IListSource* source) {
    queue_.setSource(source);
}

void NowPlayingView::setPlaybackState(PlaybackState state) {
    transport_.setPlaybackState(state);
}

void NowPlayingView::setProgress(float ratio, float elapsedSeconds, float totalSeconds) {
    seekBar_.setProgress(ratio);
    seekBar_.setTime(elapsedSeconds, totalSeconds);
}

void NowPlayingView::setOnSeek(std::function<void(int direction)> cb) {
    seekBar_.setOnSeek(std::move(cb));
}

void NowPlayingView::setOnTransport(std::function<void(TransportAction)> cb) {
    onTransport_ = std::move(cb);
    transport_.setOnActivate(onTransport_);
}

void NowPlayingView::layout(Rect contentRect) {
    bounds_ = contentRect;

    const int seekH = 28;
    const int transportH = 44;
    const int pad = 8;

    int y = bounds_.y + pad;
    seekBar_.layout({bounds_.x + pad, y, bounds_.w - 2 * pad, seekH});
    y += seekH + pad;
    transport_.layout({bounds_.x + pad, y, bounds_.w - 2 * pad, transportH});
    y += transportH + pad;
    queue_.layout({bounds_.x + pad, y, bounds_.w - 2 * pad, std::max(0, bounds_.h - (y - bounds_.y))});
}

void NowPlayingView::draw(IRenderer& renderer, const Theme& theme) {
    seekBar_.draw(renderer, theme);
    transport_.draw(renderer, theme);
    queue_.draw(renderer, theme);
}

void NowPlayingView::syncNavFocus(FocusManager& fm) {
    Widget* expected = nav_.current();
    if (!expected || !fm.hasFocus(expected)) {
        int idx = nav_.index();
        if (idx < 0) {
            idx = 2;
        }
        nav_.focusIndex(idx, fm);
    }
}

bool NowPlayingView::handleGlobalAccelerators(Button b) {
    if (b == Button::Start) {
        if (onTransport_) {
            onTransport_(TransportAction::PlayPause);
        }
        return true;
    }

    if (b == Button::L1) {
        if (onTransport_) {
            onTransport_(TransportAction::Previous);
        }
        return true;
    }

    if (b == Button::R1) {
        if (onTransport_) {
            onTransport_(TransportAction::Next);
        }
        return true;
    }

    if (b == Button::L2 || b == Button::R2) {
        seekBar_.onButtonDown(b);
        return true;
    }

    return false;
}

void NowPlayingView::openTrackInfo(int index) {
    TrackMetadata metadata;
    if (trackMetadataProvider_) {
        metadata = trackMetadataProvider_(index);
    }
    stack_.push(std::make_unique<TrackInfoPanelView>(stack_, std::move(metadata)));
}

void NowPlayingView::openContextMenu() {
    const int itemIndex = queue_.getFocusIndex();

    auto menu = std::make_unique<ContextMenuView>(stack_);
    if (onBuildContextMenu_) {
        onBuildContextMenu_(itemIndex, menu->source());
    } else {
        menu->source().add("Track Info");
        menu->source().add("Remove from Queue", {}, {}, ListItemVariant::Default,
                           0, false, false, true);
    }

    menu->setOnAction([this, itemIndex](int menuIndex) {
        if (onContextAction_) {
            onContextAction_(menuIndex, itemIndex);
        } else if (menuIndex == 0) {
            openTrackInfo(itemIndex);
        }
    });

    stack_.push(std::move(menu));
}

bool NowPlayingView::onButtonDown(Button b, FocusManager& fm) {
    if (handleGlobalAccelerators(b)) {
        return true;
    }

    if (b == Button::X) {
        openContextMenu();
        return true;
    }

    if (nav_.handleButton(b, fm)) {
        return true;
    }

    Widget* focused = fm.focused();
    if (focused == &seekBar_ && seekBar_.onButtonDown(b)) {
        return true;
    }
    if (focused == &transport_ && transport_.onButtonDown(b)) {
        return true;
    }
    if (focused == &queue_ && queue_.onButtonDown(b)) {
        return true;
    }

    syncNavFocus(fm);
    return false;
}

void NowPlayingView::restoreFocus(FocusManager& fm) {
    if (savedFocus_) {
        View::restoreFocus(fm);
        syncNavFocus(fm);
    } else {
        nav_.focusIndex(2, fm);
    }
}

std::vector<HintEntry> NowPlayingView::currentHints() const {
    return {
        {"A", "Activate", false, 1},
        {"X", "Menu", false, 2},
        {"START", "Play/Pause", false, 4},
        {"L1/R1", "Track", false, 5},
        {"L2/R2", "Seek", false, 6},
        {"B", "Back", false, 100},
    };
}

} // namespace hui
