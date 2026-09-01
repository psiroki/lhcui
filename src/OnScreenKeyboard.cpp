#include "hui/OnScreenKeyboard.h"
#include <algorithm>

namespace hui {

OnScreenKeyboard::OnScreenKeyboard() {
    const char* rows[] = {
        "1234567890",
        "qwertyuiop",
        "asdfghjkl",
        "zxcvbnm"
    };
    for (const char* row : rows) {
        for (const char* p = row; *p; ++p) {
            keys_.push_back({std::string(1, *p), false});
        }
    }
    keys_.push_back({"⌫", true});
    keys_.push_back({"Cancel", true});
    keys_.push_back({"Confirm", true});
    columns_ = 10;
}

int OnScreenKeyboard::keyCount() const {
    return static_cast<int>(keys_.size());
}

void OnScreenKeyboard::buildLayout() {
    if (bounds_.w <= 0) {
        keyWidth_ = 0;
        rows_ = 0;
        return;
    }
    keyWidth_ = bounds_.w / columns_;
    rows_ = (keyCount() + columns_ - 1) / columns_;
}

void OnScreenKeyboard::layout(Rect r) {
    bounds_ = r;
    buildLayout();
}

void OnScreenKeyboard::pressCurrentKey() {
    if (focusedIndex_ < 0 || focusedIndex_ >= keyCount()) {
        return;
    }
    const KeyCell& key = keys_[focusedIndex_];
    if (key.label == "Confirm") {
        if (onCommit_) {
            onCommit_(text_);
        }
        return;
    }
    if (key.label == "Cancel") {
        if (onCancel_) {
            onCancel_();
        }
        return;
    }
    if (key.label == "⌫") {
        if (!text_.empty()) {
            text_.pop_back();
        }
        return;
    }
    if (!key.special && key.label.size() == 1) {
        text_ += key.label;
    }
}

void OnScreenKeyboard::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    buildLayout();

    // Text preview
    Rect preview{bounds_.x, bounds_.y, bounds_.w, 28};
    renderer.fillRect(preview, theme.surfaceAlt);
    renderer.drawTextEllipsis(text_.empty() ? " " : text_,
                              {preview.x + 8, preview.y + 6},
                              theme.fontBody, theme.textPrimary, preview.w - 16);

    int gridTop = bounds_.y + 32;
    for (int i = 0; i < keyCount(); ++i) {
        int row = i / columns_;
        int col = i % columns_;
        Rect cell{bounds_.x + col * keyWidth_, gridTop + row * keyHeight_, keyWidth_, keyHeight_};
        bool selected = isFocused() && i == focusedIndex_;
        renderer.fillRect(cell, selected ? theme.focusFillColor : theme.surface);
        renderer.drawRect(cell, selected ? theme.focusBorderColor : theme.surfaceAlt, 1);

        Color colr = keys_[i].special ? theme.accent : theme.textPrimary;
        Size sz = renderer.measureText(keys_[i].label, theme.fontSmall);
        renderer.drawText(keys_[i].label,
                          {cell.x + (cell.w - sz.w) / 2,
                           cell.y + (cell.h - sz.h) / 2},
                          theme.fontSmall, colr);
    }
}

bool OnScreenKeyboard::onButtonDown(Button b) {
    if (isDisabled()) {
        return false;
    }

    if (b == Button::Left) {
        if (focusedIndex_ % columns_ > 0) {
            --focusedIndex_;
        } else {
            focusedIndex_ = std::min(keyCount() - 1, focusedIndex_ + columns_ - 1);
        }
        return true;
    }
    if (b == Button::Right) {
        if (focusedIndex_ % columns_ < columns_ - 1 && focusedIndex_ + 1 < keyCount()) {
            ++focusedIndex_;
        } else {
            focusedIndex_ = (focusedIndex_ / columns_) * columns_;
        }
        return true;
    }
    if (b == Button::Up) {
        if (focusedIndex_ >= columns_) {
            focusedIndex_ -= columns_;
        }
        return true;
    }
    if (b == Button::Down) {
        if (focusedIndex_ + columns_ < keyCount()) {
            focusedIndex_ += columns_;
        }
        return true;
    }
    if (b == Button::A || b == Button::Start) {
        pressCurrentKey();
        return true;
    }
    if (b == Button::B) {
        if (onCancel_) {
            onCancel_();
        }
        return true;
    }

    return false;
}

} // namespace hui
