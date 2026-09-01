#pragma once

#include "hui/Widget.h"
#include "hui/ListSource.h"
#include "hui/ListItemWidget.h"
#include "hui/ListHeaderWidget.h"
#include <functional>
#include <string>

namespace hui {

// §12 ListView
//
// Scrollable vertical list backed by IListSource. Owns one ListItemWidget stamp.
class ListView : public Widget {
public:
    explicit ListView(int itemHeight = 40);

    bool isFocusable() const override { return true; }

    void setSource(IListSource* source) { source_ = source; }
    IListSource* source() const { return source_; }

    void setEmptyMessage(std::string message) { emptyMessage_ = std::move(message); }
    std::string_view emptyMessage() const { return emptyMessage_; }

    void setHeaderVisible(bool visible) { headerVisible_ = visible; }
    bool headerVisible() const { return headerVisible_; }
    ListHeaderWidget& header() { return header_; }
    const ListHeaderWidget& header() const { return header_; }

    void setOnActivate(std::function<void(int index)> cb) { onActivate_ = std::move(cb); }

    void layout(Rect r) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b) override;

    int  getFocusIndex() const { return focusedIndex_; }
    void setFocusIndex(int index, bool scrollToIt = true);
    void resetFocus();
    void notifyRowsChanged();

    int itemHeight() const { return itemHeight_; }
    int scrollOffset() const { return scrollOffset_; }
    int pageRows() const { return pageRows_; }

    void setPaintAsFocused(bool paint) { paintAsFocused_ = paint; }

protected:
    int rowCount() const;
    int maxScroll() const;
    void moveFocus(int delta);
    void scrollToFocus(bool movingDown);
    void reclampScroll(int target = -1);
    void invalidateCacheOnNextDraw() { cacheInvalidated_ = true; }

    IListSource* source_ = nullptr;
    ListItemWidget stamp_;
    ListHeaderWidget header_;
    bool headerVisible_ = false;
    int itemHeight_ = 40;
    int pageRows_ = 1;
    int focusedIndex_ = 0;
    int scrollOffset_ = 0;
    std::string emptyMessage_ = "No items";
    std::function<void(int index)> onActivate_;
    bool cacheInvalidated_ = false;
    bool paintAsFocused_ = false;
    Rect listBodyBounds_{0, 0, 0, 0};
};

} // namespace hui
