#include "hui/sdl/SDLGamepadHelper.h"

#ifdef HUI_ENABLE_KEYBOARD_FALLBACK
#include "hui/sdl/KeyboardFallback.h"
#endif

#include <SDL.h>
#include <utility>

namespace hui {

SDLGamepadHelper::SDLGamepadHelper(ButtonMapping mapping)
    : mapping_(std::move(mapping)) {
    axisStates_.resize(mapping_.axisBindings.size(), AxisState::Neutral);
}

SDLGamepadHelper::~SDLGamepadHelper() {
    closeController();
}

SDLGamepadHelper::SDLGamepadHelper(SDLGamepadHelper&& other) noexcept
    : mapping_(std::move(other.mapping_)),
      controller_(other.controller_),
      joystick_(other.joystick_),
      axisStates_(std::move(other.axisStates_)) {
    other.controller_ = nullptr;
    other.joystick_ = nullptr;
}

SDLGamepadHelper& SDLGamepadHelper::operator=(SDLGamepadHelper&& other) noexcept {
    if (this != &other) {
        closeController();
        mapping_ = std::move(other.mapping_);
        controller_ = other.controller_;
        joystick_ = other.joystick_;
        axisStates_ = std::move(other.axisStates_);
        other.controller_ = nullptr;
        other.joystick_ = nullptr;
    }
    return *this;
}

bool SDLGamepadHelper::openController(int deviceIndex) {
    closeController();

#ifndef HUI_USE_SDL1
    if (SDL_IsGameController(deviceIndex)) {
        controller_ = SDL_GameControllerOpen(deviceIndex);
        return controller_ != nullptr;
    }
#endif

    joystick_ = SDL_JoystickOpen(deviceIndex);
    return joystick_ != nullptr;
}

void SDLGamepadHelper::closeController() {
#ifndef HUI_USE_SDL1
    if (controller_) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
    }
#endif
    if (joystick_) {
        SDL_JoystickClose(joystick_);
        joystick_ = nullptr;
    }
}

bool SDLGamepadHelper::isControllerOpen() const {
    return controller_ != nullptr || joystick_ != nullptr;
}

void SDLGamepadHelper::setMapping(ButtonMapping mapping) {
    mapping_ = std::move(mapping);
    axisStates_.assign(mapping_.axisBindings.size(), AxisState::Neutral);
}

std::optional<ButtonEvent> SDLGamepadHelper::translate(const SDL_Event& e) {
#ifdef HUI_ENABLE_KEYBOARD_FALLBACK
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
        return KeyboardFallback::translate(e);
    }
#endif

#ifndef HUI_USE_SDL1
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        size_t idx = static_cast<size_t>(e.cbutton.button);
        if (idx < mapping_.controllerButtons.size() && mapping_.controllerButtons[idx].has_value()) {
            return ButtonEvent{ *mapping_.controllerButtons[idx], ButtonEventKind::Down, false };
        }
        return std::nullopt;
    }

    if (e.type == SDL_CONTROLLERBUTTONUP) {
        size_t idx = static_cast<size_t>(e.cbutton.button);
        if (idx < mapping_.controllerButtons.size() && mapping_.controllerButtons[idx].has_value()) {
            return ButtonEvent{ *mapping_.controllerButtons[idx], ButtonEventKind::Up, false };
        }
        return std::nullopt;
    }
#endif

    if (e.type == SDL_JOYBUTTONDOWN) {
        size_t idx = static_cast<size_t>(e.jbutton.button);
        if (idx < mapping_.controllerButtons.size() && mapping_.controllerButtons[idx].has_value()) {
            return ButtonEvent{ *mapping_.controllerButtons[idx], ButtonEventKind::Down, false };
        }
        return std::nullopt;
    }

    if (e.type == SDL_JOYBUTTONUP) {
        size_t idx = static_cast<size_t>(e.jbutton.button);
        if (idx < mapping_.controllerButtons.size() && mapping_.controllerButtons[idx].has_value()) {
            return ButtonEvent{ *mapping_.controllerButtons[idx], ButtonEventKind::Up, false };
        }
        return std::nullopt;
    }

    int axis = -1;
    int16_t value = 0;

#ifndef HUI_USE_SDL1
    if (e.type == SDL_CONTROLLERAXISMOTION) {
        axis = e.caxis.axis;
        value = e.caxis.value;
    } else
#endif
    if (e.type == SDL_JOYAXISMOTION) {
        axis = e.jaxis.axis;
        value = e.jaxis.value;
    }

    if (axis != -1) {
        for (size_t i = 0; i < mapping_.axisBindings.size(); ++i) {
            const auto& binding = mapping_.axisBindings[i];
            if (binding.axis != axis) continue;

            int16_t threshold = binding.threshold;
            AxisState prevState = (i < axisStates_.size()) ? axisStates_[i] : AxisState::Neutral;

            if (value >= threshold) {
                if (prevState != AxisState::Positive) {
                    if (i < axisStates_.size()) axisStates_[i] = AxisState::Positive;
                    if (binding.positiveButton) {
                        return ButtonEvent{ *binding.positiveButton, ButtonEventKind::Down, false };
                    }
                }
            } else if (value <= -threshold) {
                if (prevState != AxisState::Negative) {
                    if (i < axisStates_.size()) axisStates_[i] = AxisState::Negative;
                    if (binding.negativeButton) {
                        return ButtonEvent{ *binding.negativeButton, ButtonEventKind::Down, false };
                    }
                }
            } else {
                // In neutral deadzone
                if (prevState == AxisState::Positive) {
                    if (i < axisStates_.size()) axisStates_[i] = AxisState::Neutral;
                    if (binding.positiveButton) {
                        return ButtonEvent{ *binding.positiveButton, ButtonEventKind::Up, false };
                    }
                } else if (prevState == AxisState::Negative) {
                    if (i < axisStates_.size()) axisStates_[i] = AxisState::Neutral;
                    if (binding.negativeButton) {
                        return ButtonEvent{ *binding.negativeButton, ButtonEventKind::Up, false };
                    }
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace hui
