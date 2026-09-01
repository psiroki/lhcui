#pragma once

#include "hui/ListView.h"
#include <functional>

namespace hui {

// §12 QueueList
//
// Extends ListView with grab-mode reorder: Y to grab, Up/Down to reorder,
// A to drop, B to cancel.
class QueueList : public ListView {
public:
    explicit QueueList(int itemHeight = 40);

    bool isFocusable() const override { return true; }

    void setOnReorder(std::function<void(int from, int to)> cb) { onReorder_ = std::move(cb); }

    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b) override;

    bool isGrabMode() const { return grabMode_; }
    int  grabIndex() const { return grabIndex_; }

private:
    void enterGrabMode();
    void exitGrabMode(bool commit);

    bool grabMode_ = false;
    int grabIndex_ = -1;
    int originalGrabIndex_ = -1;
    std::function<void(int from, int to)> onReorder_;
};

} // namespace hui
