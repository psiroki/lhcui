#pragma once

#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/NavList.h"
#include "hui/Slider.h"
#include "hui/Widget.h"
#include <functional>
#include <string>

namespace hui {

// §12 GuideOverlayView
//
// Slides in from the right. NavList vertical over sliders and action items.
class GuideOverlayView : public View {
public:
    HUI_VIEW_TYPE(GuideOverlayView)

    explicit GuideOverlayView(ViewStack& stack, bool animationsEnabled = true);

    bool dimsBelow() const override { return true; }

    Slider& masterVolumeSlider() { return masterVolume_; }
    Slider& brightnessSlider() { return brightness_; }

    void setOnEqualizer(std::function<void()> cb) { onEqualizer_ = std::move(cb); }
    void setOnSettings(std::function<void()> cb) { onSettings_ = std::move(cb); }
    void setOnClose(std::function<void()> cb) { onClose_ = std::move(cb); }

    void layout(Rect contentRect) override;
    void update(float dt, FocusManager& fm) override;
    void draw(IRenderer& renderer, const Theme& theme) override;
    bool onButtonDown(Button b, FocusManager& fm) override;
    void restoreFocus(FocusManager& fm) override;

    std::vector<HintEntry> currentHints() const override;

    float slideOffset() const { return slideOffset_; }

private:
    class ActionButton : public Widget {
    public:
        explicit ActionButton(std::string label) : label_(std::move(label)) {}

        bool isFocusable() const override { return true; }

        void draw(IRenderer& renderer, const Theme& theme) override;

    private:
        std::string label_;
    };

    void finishOpen();
    void activateFocused();
    void syncNavFocus(FocusManager& fm);
    bool isActionItem(const Widget* w) const;

    ViewStack& stack_;
    bool animationsEnabled_;
    float slideOffset_ = 0.0f;
    static constexpr float kSlideSpeed = 800.0f;
    int panelWidth_ = 280;
    Rect panelBounds_{0, 0, 0, 0};

    Slider masterVolume_{0, 100, 75, 5};
    Slider brightness_{0, 100, 80, 5};
    ActionButton equalizerBtn_{"Equalizer"};
    ActionButton settingsBtn_{"Settings"};
    ActionButton closeBtn_{"Close"};
    NavList nav_;
    std::function<void()> onEqualizer_;
    std::function<void()> onSettings_;
    std::function<void()> onClose_;
};

} // namespace hui
