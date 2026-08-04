#pragma once

#include "hui/types.h"
#include <array>
#include <initializer_list>
#include <optional>
#include <vector>

namespace hui {

class ChordDetector {
public:
    static constexpr float kChordWindow = 0.150f;  // seconds

    ChordDetector();

    // Register: when all of `inputs` are held simultaneously within kChordWindow, emit `output`
    // and suppress the individual inputs.
    void addChord(std::initializer_list<Button> inputs, Button output);
    void addChord(const std::vector<Button>& inputs, Button output);

    // Returns the substituted button if a chord completed, otherwise the
    // original button or nullopt (if pending/suppressed). Called by UISystem.
    std::optional<Button> onButtonDown(Button b);
    std::optional<Button> onButtonUp(Button b);

    // Drives timer for pending chord inputs. When window expires for a pending button
    // without forming a chord, calls sink(ButtonEvent{ button, ButtonEventKind::Down, false }).
    template<typename Sink>
    void update(float dt, Sink&& sink) {
        for (const auto& evt : pendingEvents_) {
            sink(evt);
        }
        pendingEvents_.clear();

        if (dt <= 0.0f) return;
        for (size_t i = 0; i < static_cast<size_t>(Button::COUNT); ++i) {
            if (pending_[i]) {
                pendingTimer_[i] += dt;
                if (pendingTimer_[i] >= kChordWindow) {
                    pending_[i] = false;
                    pendingTimer_[i] = 0.0f;
                    Button b = static_cast<Button>(i);
                    sink(ButtonEvent{ b, ButtonEventKind::Down, false });
                }
            }
        }
    }

    void reset();

private:
    struct Chord {
        std::vector<Button> inputs;
        Button output;
    };

    std::vector<Chord> chords_;
    std::array<bool, static_cast<size_t>(Button::COUNT)> held_{};
    std::array<bool, static_cast<size_t>(Button::COUNT)> pending_{};
    std::array<float, static_cast<size_t>(Button::COUNT)> pendingTimer_{};
    std::array<bool, static_cast<size_t>(Button::COUNT)> suppressed_{};
    std::array<bool, static_cast<size_t>(Button::COUNT)> activeChordOutputs_{};
    std::vector<ButtonEvent> pendingEvents_;
};

} // namespace hui
