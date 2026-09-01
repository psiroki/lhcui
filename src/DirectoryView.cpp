#include "hui/DirectoryView.h"
#include "hui/Shell.h"
#include "hui/ContextMenuView.h"

namespace hui {

DirectoryView::DirectoryView(ViewStack& stack, Shell* shell)
    : stack_(stack)
    , shell_(shell)
    , list_(36) {
    list_.setHeaderVisible(true);
    list_.setEmptyMessage("No files in this folder");
    list_.setOnActivate([this](int index) {
        if (onActivate_) {
            onActivate_(index);
        }
    });
}

void DirectoryView::setSource(IListSource* source) {
    list_.setSource(source);
    refreshHeader();
}

void DirectoryView::setHeaderPath(std::string path) {
    headerPath_ = std::move(path);
    refreshHeader();
}

void DirectoryView::setSortBadge(std::string badge) {
    sortBadge_ = std::move(badge);
    refreshHeader();
}

void DirectoryView::refreshHeader() {
    list_.header().setLabel(headerPath_);
    list_.header().setSortBadge(sortBadge_);
    list_.header().setItemCount(list_.source() ? list_.source()->rowCount() : 0);
}

void DirectoryView::layout(Rect contentRect) {
    bounds_ = contentRect;
    list_.layout(bounds_);
    refreshHeader();
}

void DirectoryView::draw(IRenderer& renderer, const Theme& theme) {
    list_.draw(renderer, theme);
}

void DirectoryView::openTrackInfo(int index) {
    TrackMetadata metadata;
    if (trackMetadataProvider_) {
        metadata = trackMetadataProvider_(index);
    }
    stack_.push(std::make_unique<TrackInfoPanelView>(stack_, std::move(metadata)));
}

void DirectoryView::openContextMenu() {
    const int itemIndex = list_.getFocusIndex();

    auto menu = std::make_unique<ContextMenuView>(stack_);
    if (onBuildContextMenu_) {
        onBuildContextMenu_(itemIndex, menu->source());
    } else {
        menu->source().add("Track Info");
        menu->source().add("Add to Queue");
    }

    menu->setOnAction([this, itemIndex](int menuIndex) {
        if (onContextAction_) {
            onContextAction_(menuIndex, itemIndex);
        } else if (menuIndex == 0) {
            openTrackInfo(itemIndex);
        } else if (menuIndex == 1 && shell_) {
            shell_->showToast("Added to queue", 2.0f);
        }
    });

    stack_.push(std::move(menu));
}

bool DirectoryView::onButtonDown(Button b, FocusManager& fm) {
    if (b == Button::X) {
        openContextMenu();
        return true;
    }

    if (list_.onButtonDown(b)) {
        return true;
    }

    if (!fm.hasFocus(&list_)) {
        fm.setFocus(&list_);
    }

    return false;
}

void DirectoryView::restoreFocus(FocusManager& fm) {
    if (savedFocus_) {
        View::restoreFocus(fm);
    } else {
        fm.setFocus(&list_);
    }
}

std::vector<HintEntry> DirectoryView::currentHints() const {
    return {
        {"A", "Open", false, 1},
        {"X", "Menu", false, 2},
        {"B", "Back", false, 100},
    };
}

} // namespace hui
