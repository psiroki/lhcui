#include "hui/GridView.h"
#include <algorithm>

namespace hui {

GridView::GridView(int cellWidth, int cellHeight, int columns)
    : cellWidth_(cellWidth > 0 ? cellWidth : 120)
    , cellHeight_(cellHeight > 0 ? cellHeight : 100)
    , columns_(columns > 0 ? columns : 3) {}

void GridView::layout(Rect r) {
    bounds_ = r;
    pageRows_ = std::max(1, bounds_.h / cellHeight_);
    reclampScroll();
}

int GridView::rowCount() const {
    return source_ ? source_->rowCount() : 0;
}

int GridView::columnCount() const {
    return columns_;
}

int GridView::gridRows() const {
    const int count = rowCount();
    if (count == 0) return 0;
    return (count + columns_ - 1) / columns_;
}

int GridView::maxScroll() const {
    const int totalContentHeight = gridRows() * cellHeight_;
    return std::max(0, totalContentHeight - bounds_.h);
}

void GridView::indexToCell(int index, int& col, int& row) const {
    col = index % columns_;
    row = index / columns_;
}

int GridView::cellToIndex(int col, int row) const {
    return row * columns_ + col;
}

void GridView::reclampScroll(int target) {
    if (target < 0) {
        target = scrollOffset_;
    }
    scrollOffset_ = std::clamp(target, 0, maxScroll());
}

void GridView::scrollToFocus(bool movingDown) {
    int col = 0, row = 0;
    indexToCell(focusedIndex_, col, row);
    const int focusedRowTop = row * cellHeight_;
    const int target = movingDown
        ? (focusedRowTop + cellHeight_) - bounds_.h * 2 / 3
        : focusedRowTop - bounds_.h / 3;
    reclampScroll(target);
}

void GridView::moveFocus(int dCol, int dRow) {
    const int count = rowCount();
    if (count == 0) {
        return;
    }

    int col = 0, row = 0;
    indexToCell(focusedIndex_, col, row);
    int newCol = col + dCol;
    int newRow = row + dRow;
    bool wrapped = false;

    if (dCol != 0) {
        if (newCol < 0) {
            if (row > 0) {
                newCol = columns_ - 1;
                newRow = row - 1;
            } else {
                newCol = (count - 1) % columns_;
                newRow = (count - 1) / columns_;
                wrapped = true;
            }
        } else if (newCol >= columns_) {
            if (row < gridRows() - 1) {
                newCol = 0;
                newRow = row + 1;
            } else {
                newCol = 0;
                newRow = 0;
                wrapped = true;
            }
        }
    }

    if (dRow != 0) {
        if (newRow < 0) {
            newRow = gridRows() - 1;
            newCol = std::min(col, (count - 1) % columns_);
            int lastRowStart = (gridRows() - 1) * columns_;
            int lastRowCount = count - lastRowStart;
            newCol = std::min(col, lastRowCount - 1);
            wrapped = true;
        } else if (newRow >= gridRows()) {
            newRow = 0;
            newCol = 0;
            wrapped = true;
        }
    }

    int newIndex = cellToIndex(newCol, newRow);
    if (newIndex >= count) {
        if (dRow > 0 || dCol > 0) {
            newIndex = 0;
            wrapped = true;
        } else {
            newIndex = count - 1;
            wrapped = true;
        }
    }

    focusedIndex_ = newIndex;

    if (wrapped) {
        scrollOffset_ = (dRow > 0 || dCol > 0) ? 0 : maxScroll();
    } else {
        scrollToFocus(dRow > 0 || (dRow == 0 && dCol > 0));
    }
}

void GridView::setFocusIndex(int index, bool scrollToIt) {
    const int count = rowCount();
    if (count == 0) {
        focusedIndex_ = 0;
        scrollOffset_ = 0;
        return;
    }
    focusedIndex_ = std::clamp(index, 0, count - 1);
    if (scrollToIt) {
        scrollToFocus(true);
    }
}

void GridView::resetFocus() {
    focusedIndex_ = 0;
    scrollOffset_ = 0;
}

void GridView::notifyRowsChanged() {
    const int count = rowCount();
    if (count == 0) {
        focusedIndex_ = 0;
    } else {
        focusedIndex_ = std::clamp(focusedIndex_, 0, count - 1);
    }
    reclampScroll();
    invalidateCacheOnNextDraw();
}

void GridView::draw(IRenderer& renderer, const Theme& theme) {
    if (cacheInvalidated_) {
        renderer.invalidateTextCache();
        cacheInvalidated_ = false;
    }

    const int count = rowCount();
    if (count == 0) {
        if (!emptyMessage_.empty() && bounds_.w > 0 && bounds_.h > 0) {
            Size sz = renderer.measureText(emptyMessage_, theme.fontBody);
            int x = bounds_.x + (bounds_.w - sz.w) / 2;
            int y = bounds_.y + (bounds_.h - sz.h) / 2;
            renderer.drawText(emptyMessage_, {x, y}, theme.fontBody, theme.textSecondary);
        }
        return;
    }

    const int firstRow = std::max(0, scrollOffset_ / cellHeight_);
    const int lastRow = std::min(gridRows() - 1,
                                 (scrollOffset_ + bounds_.h - 1) / cellHeight_);

    renderer.pushClip(bounds_);
    int y = bounds_.y + firstRow * cellHeight_ - scrollOffset_;
    RowData row;
    for (int gr = firstRow; gr <= lastRow; ++gr) {
        for (int c = 0; c < columns_; ++c) {
            int index = cellToIndex(c, gr);
            if (index >= count) {
                break;
            }
            source_->rowAt(index, row);
            stamp_.setRow(row);
            stamp_.setCellFocused(index == focusedIndex_ && isFocused());
            int cellW = bounds_.w / columns_;
            int cellX = bounds_.x + c * cellW;
            stamp_.layout({cellX, y, cellW, cellHeight_});
            stamp_.draw(renderer, theme);
        }
        y += cellHeight_;
    }
    renderer.popClip();

    const int totalHeight = gridRows() * cellHeight_;
    if (totalHeight > bounds_.h && bounds_.h > 0) {
        const int barW = 4;
        const int barH = std::max(8, bounds_.h * bounds_.h / totalHeight);
        const int maxBarTravel = bounds_.h - barH;
        const int barY = bounds_.y +
            (maxScroll() > 0 ? (scrollOffset_ * maxBarTravel / maxScroll()) : 0);
        renderer.fillRect({bounds_.x + bounds_.w - barW - 2, barY, barW, barH},
                          theme.textDisabled.withAlpha(120));
    }
}

bool GridView::onButtonDown(Button b) {
    if (isDisabled()) {
        return false;
    }

    const int count = rowCount();
    if (count == 0) {
        return false;
    }

    if (b == Button::Left) {
        moveFocus(-1, 0);
        return true;
    }
    if (b == Button::Right) {
        moveFocus(1, 0);
        return true;
    }
    if (b == Button::Up) {
        moveFocus(0, -1);
        return true;
    }
    if (b == Button::Down) {
        moveFocus(0, 1);
        return true;
    }
    if (b == Button::L1) {
        int col = 0, row = 0;
        indexToCell(focusedIndex_, col, row);
        row = std::max(0, row - pageRows_);
        int newIndex = cellToIndex(col, row);
        focusedIndex_ = std::min(newIndex, count - 1);
        scrollToFocus(false);
        return true;
    }
    if (b == Button::R1) {
        int col = 0, row = 0;
        indexToCell(focusedIndex_, col, row);
        row = std::min(gridRows() - 1, row + pageRows_);
        int newIndex = cellToIndex(col, row);
        focusedIndex_ = std::min(newIndex, count - 1);
        scrollToFocus(true);
        return true;
    }
    if (b == Button::L2) {
        focusedIndex_ = 0;
        scrollOffset_ = 0;
        return true;
    }
    if (b == Button::R2) {
        focusedIndex_ = count - 1;
        scrollOffset_ = maxScroll();
        return true;
    }
    if (b == Button::A) {
        if (onActivate_ && isFocused()) {
            onActivate_(focusedIndex_);
            return true;
        }
        return false;
    }

    return false;
}

} // namespace hui
