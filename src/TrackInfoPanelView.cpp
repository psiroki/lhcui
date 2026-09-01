#include "hui/TrackInfoPanelView.h"

namespace hui {

TrackInfoPanelView::TrackInfoPanelView(ViewStack& stack, TrackMetadata metadata)
    : stack_(stack)
    , metadata_(std::move(metadata)) {}

void TrackInfoPanelView::layout(Rect contentRect) {
    bounds_ = contentRect;
}

void TrackInfoPanelView::draw(IRenderer& renderer, const Theme& theme) {
    const int panelW = std::min(420, bounds_.w * 4 / 5);
    const int panelH = std::min(300, bounds_.h * 2 / 3);
    Rect panel{
        bounds_.x + (bounds_.w - panelW) / 2,
        bounds_.y + (bounds_.h - panelH) / 2,
        panelW,
        panelH
    };

    renderer.fillRect(panel, theme.surface);
    renderer.drawRect(panel, theme.surfaceAlt, 2);

    struct Field { const char* label; const std::string& value; };
    Field fields[] = {
        {"Title", metadata_.title},
        {"Artist", metadata_.artist},
        {"Album", metadata_.album},
        {"Genre", metadata_.genre},
        {"Year", metadata_.year},
        {"Duration", metadata_.duration},
        {"Bitrate", metadata_.bitrate},
        {"Format", metadata_.format},
    };

    int y = panel.y + 16;
    for (const auto& field : fields) {
        if (field.value.empty()) {
            continue;
        }
        std::string line = std::string(field.label) + ": " + field.value;
        renderer.drawTextEllipsis(line, {panel.x + 16, y}, theme.fontSmall,
                                  theme.textPrimary, panel.w - 32);
        y += 22;
        if (y > panel.y + panel.h - 16) {
            break;
        }
    }
}

bool TrackInfoPanelView::onButtonDown(Button b, FocusManager& fm) {
    (void)fm;
    if (b == Button::B) {
        stack_.pop();
        return true;
    }
    return false;
}

std::vector<HintEntry> TrackInfoPanelView::currentHints() const {
    return {
        {"B", "Close", false, 100},
    };
}

} // namespace hui
