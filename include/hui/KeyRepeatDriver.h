#pragma once

#include "hui/types.h"
#include <array>
#include <optional>
#include <cstddef>

namespace hui {

class KeyRepeatDriver {
public:
    // Timing constants (seconds)
    static constexpr float kInitialDelay   = 0.300f;
    static constexpr float kRepeatInterval = 0.100f;
    static constexpr float kFastInterval   = 0.030f;
    static constexpr float kFastThreshold  = 1.000f;  // after 1s, use fast interval

    // Called by UISystem on real hardware events.
    void onButtonDown(Button b);
    void onButtonUp(Button b);

    // Called by UISystem::update(). Calls sink for each synthetic repeat event.
    // Sink signature: void(ButtonEvent)
    // dt is clamped by UISystem before it gets here (§10).
    template<typename Sink>
    void update(float dt, Sink&& sink) {
        if (dt <= 0.0f) return;

        // Cap dt per update to avoid massive iterations if unclamped dt is passed directly
        constexpr float kMaxDtPerUpdate = 1.000f;
        float effectiveDt = (dt > kMaxDtPerUpdate) ? kMaxDtPerUpdate : dt;

        for (auto& slot : held_) {
            if (!slot.has_value()) continue;
            HeldButton& hb = *slot;
            hb.heldFor += effectiveDt;
            hb.timeSinceRepeat += effectiveDt;

            if (!hb.repeatStarted) {
                if (hb.heldFor >= kInitialDelay) {
                    hb.repeatStarted = true;
                    sink(ButtonEvent{ hb.button, ButtonEventKind::Down, true });
                    hb.timeSinceRepeat = hb.heldFor - kInitialDelay;
                }
            }

            if (hb.repeatStarted) {
                float interval = (hb.heldFor >= kFastThreshold) ? kFastInterval : kRepeatInterval;
                int maxIterations = 20; // safety ceiling per frame
                while (hb.timeSinceRepeat >= interval && maxIterations-- > 0) {
                    hb.timeSinceRepeat -= interval;
                    sink(ButtonEvent{ hb.button, ButtonEventKind::Down, true });
                    interval = (hb.heldFor >= kFastThreshold) ? kFastInterval : kRepeatInterval;
                }
            }
        }
    }

    // Drop all held state without emitting anything. Called by UISystem
    // whenever the view stack changes, and on suspend/resume.
    void flushHeld();

    // Only directional and shoulder buttons repeat; face buttons do not.
    static bool shouldRepeat(Button b);

private:
    struct HeldButton {
        Button button;
        float  heldFor         = 0.0f;
        float  timeSinceRepeat = 0.0f;
        bool   repeatStarted   = false;
    };

    std::array<std::optional<HeldButton>, static_cast<size_t>(Button::COUNT)> held_{};
};

} // namespace hui
