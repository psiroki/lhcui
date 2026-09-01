#pragma once

#include "hui/Widget.h"
#include "hui/ListSource.h"
#include "hui/GridCellWidget.h"
#include <functional>
#include <string>

namespace hui {

// §12 GridView
//
// 2D focusable grid with the same scroll/wrap/page/memory rules as ListView.
class GridView : public Widget {
public:
    explicit GridView(int cellWidth = 120, int cellHeight = 100, int columns = 3);

    bool isFocusable() const override { return true; }

    void setSource(IListSource* source) { source_ = source; }
    IListSource* source() const { return source_; }

    void setColumns(int columns) { columns_ = std::max(1, columns); }
    int columns() const { return columns_; }

    void setEmptyMessage(std::string message) { emptyMessage_ = std::move(message); }

    void setOnActivate(std::function<void(int index)> cb) { onActivate_ = std::move(cb); }

    void layout(Rect r) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b) override;

    int  getFocusIndex() const { return focusedIndex_; }
    void setFocusIndex(int index, bool scrollToIt = true);
    void resetFocus();
    void notifyRowsChanged();

    int cellWidth() const { return cellWidth_; }
    int cellHeight() const { return cellHeight_; }
    int scrollOffset() const { return scrollOffset_; }
    int pageRows() const { return pageRows_; }

private:
    int rowCount() const;
    int columnCount() const;
    int gridRows() const;
    int maxScroll() const;
    void indexToCell(int index, int& col, int& row) const;
    int cellToIndex(int col, int row) const;
    void moveFocus(int dCol, int dRow);
    void scrollToFocus(bool movingDown);
    void reclampScroll(int target = -1);
    void invalidateCacheOnNextDraw() { cacheInvalidated_ = true; }

    IListSource* source_ = nullptr;
    GridCellWidget stamp_;
    int cellWidth_ = 120;
    int cellHeight_ = 100;
    int columns_ = 3;
    int pageRows_ = 1;
    int focusedIndex_ = 0;
    int scrollOffset_ = 0;
    std::string emptyMessage_ = "No items";
    std::function<void(int index)> onActivate_;
    bool cacheInvalidated_ = false;
};

} // namespace hui
