#pragma once

#include "hui/Widget.h"
#include <functional>
#include <string>
#include <vector>

namespace hui {

// §12 OnScreenKeyboard
//
// 2D QWERTY grid with Confirm, Cancel, and Backspace keys.
class OnScreenKeyboard : public Widget {
public:
    OnScreenKeyboard();

    bool isFocusable() const override { return true; }

    void setText(std::string text) { text_ = std::move(text); }
    std::string_view text() const { return text_; }

    void setOnCommit(std::function<void(std::string)> cb) { onCommit_ = std::move(cb); }
    void setOnCancel(std::function<void()> cb) { onCancel_ = std::move(cb); }

    void layout(Rect r) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b) override;

private:
    struct KeyCell {
        std::string label;
        bool special = false;
    };

    void buildLayout();
    int keyCount() const;
    void pressCurrentKey();

    std::vector<KeyCell> keys_;
    int columns_ = 10;
    int focusedIndex_ = 0;
    int keyWidth_ = 0;
    int keyHeight_ = 36;
    int rows_ = 0;
    std::string text_;
    std::function<void(std::string)> onCommit_;
    std::function<void()> onCancel_;
};

} // namespace hui
