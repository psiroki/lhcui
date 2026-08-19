#pragma once

#include "hui/Widget.h"
#include <string>
#include <string_view>

namespace hui {

// §12 SortModeIndicator
//
// Non-focusable badge showing the current sort mode label (e.g. "Title", "Artist", "Track #").
class SortModeIndicator : public Widget {
public:
    explicit SortModeIndicator(std::string mode = "Default");

    bool isFocusable() const override { return false; }

    void setMode(std::string mode) { mode_ = std::move(mode); }
    std::string_view mode() const { return mode_; }

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    std::string mode_;
};

} // namespace hui
