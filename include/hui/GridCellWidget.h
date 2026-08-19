#pragma once

#include "hui/Widget.h"
#include "hui/ListSource.h"
#include <string_view>

namespace hui {

// §12, §13.2 GridCellWidget
//
// Single grid tile stamp widget:
// Thumbnail texture or gradient placeholder (hueToColor + labelHash), label, sublabel.
// Focused border; playing badge.
//
// Stamp contract: container owns ONE instance and calls setCell() / setRow() + setCellFocused()
// per visible tile. isFocusable() returns false.
class GridCellWidget : public Widget {
public:
    GridCellWidget() = default;

    bool isFocusable() const override { return false; }

    void setCell(std::string_view label,
                 std::string_view sublabel = {},
                 TextureHandle thumbnail = 0,
                 bool playing = false,
                 bool disabled = false);

    void setRow(const RowData& data);
    void setCellFocused(bool focused);

    bool isCellFocused() const { return cellFocused_; }

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    std::string_view label_;
    std::string_view sublabel_;
    TextureHandle thumbnail_ = 0;
    bool playing_ = false;
    bool disabled_ = false;
    bool cellFocused_ = false;
};

} // namespace hui
