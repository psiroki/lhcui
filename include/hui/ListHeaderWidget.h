#pragma once

#include "hui/Widget.h"
#include "hui/types.h"
#include <string>
#include <string_view>

namespace hui {

// §12 ListHeaderWidget
//
// Non-focusable context row: icon, label (left-truncated for paths), item count, sort badge.
class ListHeaderWidget : public Widget {
public:
    ListHeaderWidget() = default;

    bool isFocusable() const override { return false; }

    void setIcon(TextureHandle icon) { icon_ = icon; }
    TextureHandle icon() const { return icon_; }

    void setLabel(std::string label) { label_ = std::move(label); }
    std::string_view label() const { return label_; }

    void setItemCount(int count) { itemCount_ = count; }
    int itemCount() const { return itemCount_; }

    void setSortBadge(std::string badge) { sortBadge_ = std::move(badge); }
    std::string_view sortBadge() const { return sortBadge_; }

    void draw(IRenderer& renderer, const Theme& theme) override;

private:
    TextureHandle icon_ = 0;
    std::string label_;
    int itemCount_ = -1;
    std::string sortBadge_;
};

} // namespace hui
