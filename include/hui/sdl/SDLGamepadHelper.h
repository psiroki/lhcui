#pragma once

#include "hui/sdl/ButtonMapping.h"
#include "hui/types.h"
#include <optional>
#include <vector>

// Forward declarations so SDL headers are not leaked into include/hui/
union SDL_Event;
struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;
struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;

namespace hui {

class SDLGamepadHelper {
public:
    explicit SDLGamepadHelper(ButtonMapping mapping = ButtonMapping::defaultXboxLayout());
    ~SDLGamepadHelper();

    // Non-copyable (owns SDL controller/joystick resource)
    SDLGamepadHelper(const SDLGamepadHelper&) = delete;
    SDLGamepadHelper& operator=(const SDLGamepadHelper&) = delete;

    // Movable
    SDLGamepadHelper(SDLGamepadHelper&& other) noexcept;
    SDLGamepadHelper& operator=(SDLGamepadHelper&& other) noexcept;

    // Opens the controller/joystick at deviceIndex. Returns true if opened successfully.
    bool openController(int deviceIndex = 0);
    void closeController();
    bool isControllerOpen() const;

    // Translates an SDL_Event to a ButtonEvent.
    // Returns std::nullopt for unmapped events or axis movements within hysteresis deadzone.
    std::optional<ButtonEvent> translate(const SDL_Event& e);

    const ButtonMapping& mapping() const { return mapping_; }
    void setMapping(ButtonMapping mapping);

private:
    enum class AxisState : int8_t {
        Neutral = 0,
        Positive = 1,
        Negative = -1
    };

    ButtonMapping mapping_;
    SDL_GameController* controller_ = nullptr;
    SDL_Joystick* joystick_ = nullptr;

    std::vector<AxisState> axisStates_;
};

} // namespace hui
