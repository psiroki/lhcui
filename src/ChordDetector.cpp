#include "hui/ChordDetector.h"

namespace hui {

ChordDetector::ChordDetector() {
    addChord({Button::Start, Button::Select}, Button::Guide);
}

void ChordDetector::addChord(std::initializer_list<Button> inputs, Button output) {
    addChord(std::vector<Button>(inputs), output);
}

void ChordDetector::addChord(const std::vector<Button>& inputs, Button output) {
    chords_.push_back(Chord{ inputs, output });
}

void ChordDetector::reset() {
    held_.fill(false);
    pending_.fill(false);
    pendingTimer_.fill(0.0f);
    suppressed_.fill(false);
    activeChordOutputs_.fill(false);
    pendingEvents_.clear();
}

std::optional<Button> ChordDetector::onButtonDown(Button b) {
    size_t idx = static_cast<size_t>(b);
    if (idx >= held_.size()) return b;

    held_[idx] = true;

    // Check if b is an input in any registered chord
    bool isChordInput = false;
    for (const auto& chord : chords_) {
        for (Button input : chord.inputs) {
            if (input == b) {
                isChordInput = true;
                break;
            }
        }
        if (isChordInput) break;
    }

    if (!isChordInput) {
        pending_[idx] = false;
        pendingTimer_[idx] = 0.0f;
        suppressed_[idx] = false;
        return b;
    }

    // Check if b completes any chord
    for (const auto& chord : chords_) {
        bool containsB = false;
        for (Button input : chord.inputs) {
            if (input == b) {
                containsB = true;
                break;
            }
        }
        if (!containsB) continue;

        bool chordComplete = true;
        for (Button input : chord.inputs) {
            size_t inIdx = static_cast<size_t>(input);
            if (input == b) continue;
            if (!held_[inIdx] || suppressed_[inIdx] || pendingTimer_[inIdx] > kChordWindow) {
                chordComplete = false;
                break;
            }
        }

        if (chordComplete) {
            // Chord matched! Suppress all chord input buttons
            for (Button input : chord.inputs) {
                size_t inIdx = static_cast<size_t>(input);
                suppressed_[inIdx] = true;
                pending_[inIdx] = false;
                pendingTimer_[inIdx] = 0.0f;
            }
            size_t outIdx = static_cast<size_t>(chord.output);
            if (outIdx < activeChordOutputs_.size()) {
                activeChordOutputs_[outIdx] = true;
            }
            return chord.output;
        }
    }

    // No chord completed yet, mark b as pending
    pending_[idx] = true;
    pendingTimer_[idx] = 0.0f;
    suppressed_[idx] = false;
    return std::nullopt;
}

std::optional<Button> ChordDetector::onButtonUp(Button b) {
    size_t idx = static_cast<size_t>(b);
    if (idx >= held_.size()) return b;

    held_[idx] = false;

    if (suppressed_[idx]) {
        suppressed_[idx] = false;
        pending_[idx] = false;
        pendingTimer_[idx] = 0.0f;

        // Check if this was part of an active chord output
        for (const auto& chord : chords_) {
            bool containsB = false;
            for (Button input : chord.inputs) {
                if (input == b) {
                    containsB = true;
                    break;
                }
            }
            if (containsB) {
                size_t outIdx = static_cast<size_t>(chord.output);
                if (outIdx < activeChordOutputs_.size() && activeChordOutputs_[outIdx]) {
                    activeChordOutputs_[outIdx] = false;
                    return chord.output;
                }
            }
        }
        return std::nullopt;
    }

    if (pending_[idx]) {
        pending_[idx] = false;
        pendingTimer_[idx] = 0.0f;
        pendingEvents_.push_back(ButtonEvent{ b, ButtonEventKind::Down, false });
        return b;
    }

    return b;
}

} // namespace hui
