#include "hui/ListView.h"
#include <algorithm>

namespace hui {

ListView::ListView(int itemHeight)
    : itemHeight_(itemHeight > 0 ? itemHeight : 40) {}

void ListView::layout(Rect r) {
    bounds_ = r;
    if (headerVisible_) {
        const int headerH = 28;
        header_.layout({r.x, r.y, r.w, headerH});
        listBodyBounds_ = {r.x, r.y + headerH, r.w, std::max(0, r.h - headerH)};
    } else {
        listBodyBounds_ = r;
    }
    pageRows_ = std::max(1, listBodyBounds_.h / itemHeight_);
    reclampScroll();
}

int ListView::rowCount() const {
    return source_ ? source_->rowCount() : 0;
}

int ListView::maxScroll() const {
    const int totalContentHeight = rowCount() * itemHeight_;
    return std::max(0, totalContentHeight - listBodyBounds_.h);
}

void ListView::reclampScroll(int target) {
    if (target < 0) {
        target = scrollOffset_;
    }
    scrollOffset_ = std::clamp(target, 0, maxScroll());
}

void ListView::scrollToFocus(bool movingDown) {
    const int focusedItemTop = focusedIndex_ * itemHeight_;
    const int target = movingDown
        ? (focusedItemTop + itemHeight_) - listBodyBounds_.h * 2 / 3
        : focusedItemTop - listBodyBounds_.h / 3;
    reclampScroll(target);
}

void ListView::moveFocus(int delta) {
    const int count = rowCount();
    if (count == 0) {
        return;
    }

    int newIndex = focusedIndex_ + delta;
    bool wrapped = false;

    if (newIndex < 0) {
        newIndex = count - 1;
        wrapped = true;
    } else if (newIndex >= count) {
        newIndex = 0;
        wrapped = true;
    }

    focusedIndex_ = newIndex;

    if (wrapped) {
        scrollOffset_ = (delta > 0) ? 0 : maxScroll();
    } else {
        scrollToFocus(delta > 0);
    }
}

void ListView::setFocusIndex(int index, bool scrollToIt) {
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

void ListView::resetFocus() {
    focusedIndex_ = 0;
    scrollOffset_ = 0;
}

void ListView::notifyRowsChanged() {
    const int count = rowCount();
    if (count == 0) {
        focusedIndex_ = 0;
    } else {
        focusedIndex_ = std::clamp(focusedIndex_, 0, count - 1);
    }
    reclampScroll();
    invalidateCacheOnNextDraw();
}

void ListView::draw(IRenderer& renderer, const Theme& theme) {
    if (cacheInvalidated_) {
        renderer.invalidateTextCache();
        cacheInvalidated_ = false;
    }

    if (headerVisible_) {
        header_.draw(renderer, theme);
    }

    const int count = rowCount();
    if (count == 0) {
        if (!emptyMessage_.empty() && listBodyBounds_.w > 0 && listBodyBounds_.h > 0) {
            Size sz = renderer.measureText(emptyMessage_, theme.fontBody);
            int x = listBodyBounds_.x + (listBodyBounds_.w - sz.w) / 2;
            int y = listBodyBounds_.y + (listBodyBounds_.h - sz.h) / 2;
            renderer.drawText(emptyMessage_, {x, y}, theme.fontBody, theme.textSecondary);
        }
        return;
    }

    const int first = std::max(0, scrollOffset_ / itemHeight_);
    const int last = std::min(count - 1,
                              (scrollOffset_ + listBodyBounds_.h - 1) / itemHeight_);

    renderer.pushClip(listBodyBounds_);
    int y = listBodyBounds_.y + first * itemHeight_ - scrollOffset_;
    RowData row;
    for (int i = first; i <= last; ++i) {
        source_->rowAt(i, row);
        stamp_.setRow(row);
        stamp_.setRowFocused(i == focusedIndex_ && (isFocused() || paintAsFocused_));
        stamp_.layout({listBodyBounds_.x, y, listBodyBounds_.w, itemHeight_});
        stamp_.draw(renderer, theme);
        y += itemHeight_;
    }
    renderer.popClip();

    // Scroll indicator
    const int totalHeight = count * itemHeight_;
    if (totalHeight > listBodyBounds_.h && listBodyBounds_.h > 0) {
        const int barW = 4;
        const int barH = std::max(8, listBodyBounds_.h * listBodyBounds_.h / totalHeight);
        const int maxBarTravel = listBodyBounds_.h - barH;
        const int barY = listBodyBounds_.y +
            (maxScroll() > 0 ? (scrollOffset_ * maxBarTravel / maxScroll()) : 0);
        renderer.fillRect({listBodyBounds_.x + listBodyBounds_.w - barW - 2, barY,
                           barW, barH},
                          theme.textDisabled.withAlpha(120));
    }
}

bool ListView::onButtonDown(Button b) {
    if (isDisabled()) {
        return false;
    }

    const int count = rowCount();
    if (count == 0) {
        return false;
    }

    if (b == Button::Up) {
        moveFocus(-1);
        return true;
    }
    if (b == Button::Down) {
        moveFocus(1);
        return true;
    }
    if (b == Button::L1) {
        focusedIndex_ = std::max(0, focusedIndex_ - pageRows_);
        scrollToFocus(false);
        return true;
    }
    if (b == Button::R1) {
        focusedIndex_ = std::min(count - 1, focusedIndex_ + pageRows_);
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
