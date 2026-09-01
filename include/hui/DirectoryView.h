#pragma once

#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/ListView.h"
#include "hui/ListSource.h"
#include "hui/TrackInfoPanelView.h"
#include <functional>
#include <string>

namespace hui {

class Shell;

// §12 DirectoryView
//
// File browser screen backed by an application-owned IListSource.
class DirectoryView : public View {
public:
    HUI_VIEW_TYPE(DirectoryView)

    DirectoryView(ViewStack& stack, Shell* shell = nullptr);

    void setSource(IListSource* source);
    IListSource* source() const { return list_.source(); }

    ListView& list() { return list_; }
    const ListView& list() const { return list_; }

    void setHeaderPath(std::string path);
    void setSortBadge(std::string badge);

    void setOnActivate(std::function<void(int index)> cb) { onActivate_ = std::move(cb); }
    void setOnBuildContextMenu(std::function<void(int index, VectorListSource& menu)> cb) {
        onBuildContextMenu_ = std::move(cb);
    }
    void setOnContextAction(std::function<void(int menuIndex, int itemIndex)> cb) {
        onContextAction_ = std::move(cb);
    }
    void setTrackMetadataProvider(std::function<TrackMetadata(int index)> cb) {
        trackMetadataProvider_ = std::move(cb);
    }

    void layout(Rect contentRect) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b, FocusManager& fm) override;
    void restoreFocus(FocusManager& fm) override;

    std::vector<HintEntry> currentHints() const override;

private:
    void openContextMenu();
    void openTrackInfo(int index);
    void refreshHeader();

    ViewStack& stack_;
    Shell* shell_ = nullptr;
    ListView list_;
    std::function<void(int index)> onActivate_;
    std::function<void(int index, VectorListSource& menu)> onBuildContextMenu_;
    std::function<void(int menuIndex, int itemIndex)> onContextAction_;
    std::function<TrackMetadata(int index)> trackMetadataProvider_;
    std::string headerPath_;
    std::string sortBadge_;
};

} // namespace hui
