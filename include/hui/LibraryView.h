#pragma once

#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/ListView.h"
#include "hui/GridView.h"
#include "hui/TabBarWidget.h"
#include "hui/ListSource.h"
#include "hui/TrackInfoPanelView.h"
#include <functional>
#include <string>

namespace hui {

// §12 LibraryView
//
// Dual-mode library browser with list and grid tabs.
class LibraryView : public View {
public:
    HUI_VIEW_TYPE(LibraryView)

    explicit LibraryView(ViewStack& stack);

    void setListSource(IListSource* source);
    void setGridSource(IListSource* source);

    ListView& list() { return list_; }
    GridView& grid() { return grid_; }
    TabBarWidget& tabBar() { return tabBar_; }

    int activeTab() const { return tabBar_.selectedIndex(); }

    void setOnActivate(std::function<void(int index, int tab)> cb) { onActivate_ = std::move(cb); }
    void setOnBuildContextMenu(std::function<void(int index, int tab, VectorListSource& menu)> cb) {
        onBuildContextMenu_ = std::move(cb);
    }
    void setOnContextAction(std::function<void(int menuIndex, int itemIndex, int tab)> cb) {
        onContextAction_ = std::move(cb);
    }
    void setTrackMetadataProvider(std::function<TrackMetadata(int index, int tab)> cb) {
        trackMetadataProvider_ = std::move(cb);
    }
    void setOnLetterChanged(std::function<void(char)> cb) { onLetterChanged_ = std::move(cb); }

    void layout(Rect contentRect) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b, FocusManager& fm) override;
    void restoreFocus(FocusManager& fm) override;

    std::vector<HintEntry> currentHints() const override;

private:
    enum class Tab { List = 0, Grid = 1 };

    Widget* activeWidget();
    const Widget* activeWidget() const;
    void onTabChanged(int index);
    void openContextMenu();
    void openTrackInfo(int index);
    void openLetterWheel();

    ViewStack& stack_;
    TabBarWidget tabBar_;
    ListView list_;
    GridView grid_;
    std::function<void(int index, int tab)> onActivate_;
    std::function<void(int index, int tab, VectorListSource& menu)> onBuildContextMenu_;
    std::function<void(int menuIndex, int itemIndex, int tab)> onContextAction_;
    std::function<TrackMetadata(int index, int tab)> trackMetadataProvider_;
    std::function<void(char)> onLetterChanged_;
    Rect bodyBounds_{0, 0, 0, 0};
};

} // namespace hui
