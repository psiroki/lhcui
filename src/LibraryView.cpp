#include "hui/LibraryView.h"
#include "hui/ContextMenuView.h"
#include "hui/LetterWheelView.h"

namespace hui {

LibraryView::LibraryView(ViewStack& stack)
    : stack_(stack)
    , list_(36)
    , grid_(120, 100, 3) {
    tabBar_.setTabs({"List", "Grid"});
    tabBar_.setOnTabChanged([this](int index) { onTabChanged(index); });

    list_.setEmptyMessage("No tracks in library");
    grid_.setEmptyMessage("No tracks in library");

    list_.setOnActivate([this](int index) {
        if (onActivate_) {
            onActivate_(index, static_cast<int>(Tab::List));
        }
    });
    grid_.setOnActivate([this](int index) {
        if (onActivate_) {
            onActivate_(index, static_cast<int>(Tab::Grid));
        }
    });
}

void LibraryView::setListSource(IListSource* source) {
    list_.setSource(source);
}

void LibraryView::setGridSource(IListSource* source) {
    grid_.setSource(source);
}

Widget* LibraryView::activeWidget() {
    return tabBar_.selectedIndex() == static_cast<int>(Tab::Grid)
        ? static_cast<Widget*>(&grid_)
        : static_cast<Widget*>(&list_);
}

const Widget* LibraryView::activeWidget() const {
    return tabBar_.selectedIndex() == static_cast<int>(Tab::Grid)
        ? static_cast<const Widget*>(&grid_)
        : static_cast<const Widget*>(&list_);
}

void LibraryView::onTabChanged(int index) {
    (void)index;
    layout(bounds_);
    // Focus memory is preserved inside each widget; do not call resetFocus().
}

void LibraryView::layout(Rect contentRect) {
    bounds_ = contentRect;

    constexpr int kTabBarHeight = 32;
    tabBar_.layout({bounds_.x, bounds_.y, bounds_.w, kTabBarHeight});
    bodyBounds_ = {bounds_.x, bounds_.y + kTabBarHeight, bounds_.w,
                   std::max(0, bounds_.h - kTabBarHeight)};

    list_.layout(bodyBounds_);
    grid_.layout(bodyBounds_);
}

void LibraryView::draw(IRenderer& renderer, const Theme& theme) {
    tabBar_.draw(renderer, theme);
    if (tabBar_.selectedIndex() == static_cast<int>(Tab::Grid)) {
        grid_.draw(renderer, theme);
    } else {
        list_.draw(renderer, theme);
    }
}

void LibraryView::openTrackInfo(int index) {
    TrackMetadata metadata;
    if (trackMetadataProvider_) {
        metadata = trackMetadataProvider_(index, tabBar_.selectedIndex());
    }
    stack_.push(std::make_unique<TrackInfoPanelView>(stack_, std::move(metadata)));
}

void LibraryView::openLetterWheel() {
    auto wheel = std::make_unique<LetterWheelView>(stack_);
    if (onLetterChanged_) {
        wheel->letterWheel().setOnCharChanged(onLetterChanged_);
    }
    stack_.push(std::move(wheel));
}

void LibraryView::openContextMenu() {
    const int tab = tabBar_.selectedIndex();
    const int itemIndex = (tab == static_cast<int>(Tab::Grid))
        ? grid_.getFocusIndex()
        : list_.getFocusIndex();

    auto menu = std::make_unique<ContextMenuView>(stack_);
    if (onBuildContextMenu_) {
        onBuildContextMenu_(itemIndex, tab, menu->source());
    } else {
        menu->source().add("Track Info");
        menu->source().add("Search by Letter");
        menu->source().add("Add to Queue");
    }

    menu->setOnAction([this, itemIndex, tab](int menuIndex) {
        if (onContextAction_) {
            onContextAction_(menuIndex, itemIndex, tab);
            return;
        }
        if (menuIndex == 0) {
            openTrackInfo(itemIndex);
        } else if (menuIndex == 1) {
            openLetterWheel();
        }
    });

    stack_.push(std::move(menu));
}

bool LibraryView::onButtonDown(Button b, FocusManager& fm) {
    if (tabBar_.onButtonDown(b)) {
        Widget* active = activeWidget();
        if (active && !fm.hasFocus(active)) {
            fm.setFocus(active);
        }
        return true;
    }

    if (b == Button::X) {
        openContextMenu();
        return true;
    }

    Widget* active = activeWidget();
    if (active && active->onButtonDown(b)) {
        return true;
    }

    if (active && !fm.hasFocus(active)) {
        fm.setFocus(active);
    }

    return false;
}

void LibraryView::restoreFocus(FocusManager& fm) {
    if (savedFocus_) {
        View::restoreFocus(fm);
    } else {
        Widget* active = activeWidget();
        if (active) {
            fm.setFocus(active);
        }
    }
}

std::vector<HintEntry> LibraryView::currentHints() const {
    return {
        {"A", "Play", false, 1},
        {"X", "Menu", false, 2},
        {"L1/R1", "Tab", false, 5},
        {"B", "Back", false, 100},
    };
}

} // namespace hui
