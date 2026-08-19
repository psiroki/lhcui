#pragma once

#include "hui/Widget.h"
#include "hui/ListSource.h"

namespace hui {

// §6.2, §6.5, §12 ListItemWidget
//
// Single row stamp widget: icon, primary label, secondary label, right meta.
// States: default / focused / playing / disabled.
// Variants: default, track, folder, playlist.
//
// Stamp contract: container owns ONE instance and calls setRow() + setRowFocused()
// per visible row. Holds no owning std::string memory.
// isFocusable() returns false because focus highlight is driven by the container.
class ListItemWidget : public Widget {
public:
    ListItemWidget() = default;

    bool isFocusable() const override { return false; }

    void setRow(const RowData& data);
    void setRowFocused(bool focused);

    bool isRowFocused() const { return rowFocused_; }
    const RowData& row() const { return row_; }

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    RowData row_{};
    bool rowFocused_ = false;
};

} // namespace hui
