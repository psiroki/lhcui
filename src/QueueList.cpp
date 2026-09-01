#include "hui/QueueList.h"

namespace hui {

QueueList::QueueList(int itemHeight)
    : ListView(itemHeight) {}

void QueueList::enterGrabMode() {
    grabMode_ = true;
    grabIndex_ = getFocusIndex();
    originalGrabIndex_ = grabIndex_;
}

void QueueList::exitGrabMode(bool commit) {
    if (commit && onReorder_ && grabIndex_ >= 0 && originalGrabIndex_ >= 0 &&
        grabIndex_ != originalGrabIndex_) {
        onReorder_(originalGrabIndex_, grabIndex_);
    }
    grabMode_ = false;
    grabIndex_ = -1;
    originalGrabIndex_ = -1;
}

void QueueList::draw(IRenderer& renderer, const Theme& theme) {
    ListView::draw(renderer, theme);

    if (grabMode_ && grabIndex_ >= 0) {
        int y = listBodyBounds_.y + grabIndex_ * itemHeight() - scrollOffset();
        Rect grabRect{listBodyBounds_.x, y, listBodyBounds_.w, itemHeight()};
        renderer.drawRect(grabRect, theme.warning, 2);
        Size sz = renderer.measureText("GRAB", theme.fontSmall);
        renderer.drawText("GRAB",
                          {grabRect.x + grabRect.w - sz.w - 8,
                           grabRect.y + (grabRect.h - sz.h) / 2},
                          theme.fontSmall, theme.warning);
    }
}

bool QueueList::onButtonDown(Button b) {
    if (isDisabled()) {
        return false;
    }

    if (grabMode_) {
        if (b == Button::Up) {
            if (grabIndex_ > 0) {
                --grabIndex_;
                setFocusIndex(grabIndex_, true);
            }
            return true;
        }
        if (b == Button::Down) {
            const int count = rowCount();
            if (grabIndex_ < count - 1) {
                ++grabIndex_;
                setFocusIndex(grabIndex_, true);
            }
            return true;
        }
        if (b == Button::A) {
            exitGrabMode(true);
            return true;
        }
        if (b == Button::B) {
            setFocusIndex(originalGrabIndex_, true);
            exitGrabMode(false);
            return true;
        }
        return true;
    }

    if (b == Button::Y) {
        if (rowCount() > 0) {
            enterGrabMode();
        }
        return true;
    }

    return ListView::onButtonDown(b);
}

} // namespace hui
