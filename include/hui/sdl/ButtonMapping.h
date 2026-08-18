#pragma once

#include "hui/types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace hui {

// Maps SDL gamepad/joystick button indices to hui::Button values.
struct ButtonMapping {
    static constexpr size_t kMaxControllerButtons = 32;

    std::array<std::optional<Button>, kMaxControllerButtons> controllerButtons{};

    // Axis-to-button thresholds for axes.
    // Positive axis crossing +threshold fires the positive button,
    // negative crossing -threshold fires the negative button.
    struct AxisBinding {
        int                   axis = 0;
        std::optional<Button> positiveButton = std::nullopt;
        std::optional<Button> negativeButton = std::nullopt;
        int16_t               threshold = 16384;  // ~50% of INT16_MAX
    };

    std::vector<AxisBinding> axisBindings;

    static ButtonMapping defaultXboxLayout();
    static ButtonMapping defaultNintendoLayout();  // swapped A/B and X/Y
};

// Maps button name strings (e.g. "Up", "L2", "A", "Start") to hui::Button.
// Returns std::nullopt for unknown names.
std::optional<Button> buttonFromName(std::string_view name);

} // namespace hui
