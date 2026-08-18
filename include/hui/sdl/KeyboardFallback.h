#pragma once

#ifdef HUI_ENABLE_KEYBOARD_FALLBACK

#include "hui/types.h"
#include <optional>

// Forward declaration so SDL headers are not leaked into include/hui/
union SDL_Event;

namespace hui {

/**
 * KeyboardFallback provides desktop keyboard-to-gamepad mapping for development and testing.
 *
 * Mappings:
 *  - D-pad:        Arrow Keys (Up, Down, Left, Right)
 *  - A:            Z, Return, Space
 *  - B:            X, Escape, Backspace
 *  - X:            A, C
 *  - Y:            S, V
 *  - L1:           Q, 1, PageUp
 *  - R1:           E, 2, PageDown
 *  - L2:           W, 3, Home
 *  - R2:           R, 4, End
 *  - Start:        Return, F1
 *  - Select:       Tab, Backspace, F2
 *  - Guide:        F12, F5, G
 */
class KeyboardFallback {
public:
    static std::optional<ButtonEvent> translate(const SDL_Event& e);
};

} // namespace hui

#endif // HUI_ENABLE_KEYBOARD_FALLBACK
