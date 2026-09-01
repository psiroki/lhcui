#pragma once

#include "hui/Widget.h"
#include "hui/ListView.h"
#include <functional>
#include <string>

namespace hui {

// §12 LetterWheel
//
// Character strip + results list with internal focus routing.
class LetterWheel : public Widget {
public:
    explicit LetterWheel(int itemHeight = 40);

    bool isFocusable() const override { return true; }

    void setResultsSource(IListSource* source) { results_.setSource(source); }
    ListView& results() { return results_; }
    const ListView& results() const { return results_; }

    char selectedChar() const;
    void setOnCharChanged(std::function<void(char)> cb) { onCharChanged_ = std::move(cb); }

    void layout(Rect r) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b) override;

    void notifyRowsChanged() { results_.notifyRowsChanged(); }

private:
    enum class FocusArea { Strip, List };

    void selectStripIndex(int index);

    ListView results_;
    std::string stripChars_ = "ABCDEFGHIJKLMNOPQRSTUVWXYZ#";
    int stripIndex_ = 0;
    FocusArea focusArea_ = FocusArea::Strip;
    int stripHeight_ = 32;
    std::function<void(char)> onCharChanged_;
};

} // namespace hui
