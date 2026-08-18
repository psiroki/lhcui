#include "hui/sdl/ButtonMapping.h"

namespace hui {

ButtonMapping ButtonMapping::defaultXboxLayout() {
    ButtonMapping mapping;

    // Standard SDL_GameController button mappings
    mapping.controllerButtons[0]  = Button::A;          // SDL_CONTROLLER_BUTTON_A
    mapping.controllerButtons[1]  = Button::B;          // SDL_CONTROLLER_BUTTON_B
    mapping.controllerButtons[2]  = Button::X;          // SDL_CONTROLLER_BUTTON_X
    mapping.controllerButtons[3]  = Button::Y;          // SDL_CONTROLLER_BUTTON_Y
    mapping.controllerButtons[4]  = Button::Select;     // SDL_CONTROLLER_BUTTON_BACK
    mapping.controllerButtons[5]  = Button::Guide;      // SDL_CONTROLLER_BUTTON_GUIDE
    mapping.controllerButtons[6]  = Button::Start;      // SDL_CONTROLLER_BUTTON_START
    mapping.controllerButtons[9]  = Button::L1;         // SDL_CONTROLLER_BUTTON_LEFTSHOULDER
    mapping.controllerButtons[10] = Button::R1;         // SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
    mapping.controllerButtons[11] = Button::Up;         // SDL_CONTROLLER_BUTTON_DPAD_UP
    mapping.controllerButtons[12] = Button::Down;       // SDL_CONTROLLER_BUTTON_DPAD_DOWN
    mapping.controllerButtons[13] = Button::Left;       // SDL_CONTROLLER_BUTTON_DPAD_LEFT
    mapping.controllerButtons[14] = Button::Right;      // SDL_CONTROLLER_BUTTON_DPAD_RIGHT

    // Standard SDL_GameController axis bindings
    // Axis 0: Left Stick X
    mapping.axisBindings.push_back({0, Button::Right, Button::Left, 16384});
    // Axis 1: Left Stick Y
    mapping.axisBindings.push_back({1, Button::Down, Button::Up, 16384});
    // Axis 4: Left Trigger (L2)
    mapping.axisBindings.push_back({4, Button::L2, std::nullopt, 16384});
    // Axis 5: Right Trigger (R2)
    mapping.axisBindings.push_back({5, Button::R2, std::nullopt, 16384});

    return mapping;
}

ButtonMapping ButtonMapping::defaultNintendoLayout() {
    ButtonMapping mapping = defaultXboxLayout();

    // Swap A and B
    mapping.controllerButtons[0] = Button::B;
    mapping.controllerButtons[1] = Button::A;

    // Swap X and Y
    mapping.controllerButtons[2] = Button::Y;
    mapping.controllerButtons[3] = Button::X;

    return mapping;
}

std::optional<Button> buttonFromName(std::string_view name) {
    if (name == "Up")     return Button::Up;
    if (name == "Down")   return Button::Down;
    if (name == "Left")   return Button::Left;
    if (name == "Right")  return Button::Right;
    if (name == "A")      return Button::A;
    if (name == "B")      return Button::B;
    if (name == "X")      return Button::X;
    if (name == "Y")      return Button::Y;
    if (name == "L1")     return Button::L1;
    if (name == "L2")     return Button::L2;
    if (name == "R1")     return Button::R1;
    if (name == "R2")     return Button::R2;
    if (name == "Start")  return Button::Start;
    if (name == "Select") return Button::Select;
    if (name == "Guide")  return Button::Guide;

    return std::nullopt;
}

} // namespace hui
