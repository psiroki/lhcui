#ifdef HUI_ENABLE_KEYBOARD_FALLBACK

#include "hui/sdl/KeyboardFallback.h"
#include <SDL.h>

namespace hui {

std::optional<ButtonEvent> KeyboardFallback::translate(const SDL_Event& e) {
    if (e.type != SDL_KEYDOWN && e.type != SDL_KEYUP) {
        return std::nullopt;
    }

#ifndef HUI_USE_SDL1
    // In SDL2, ignore auto-repeated keydown events to let KeyRepeatDriver manage timing
    if (e.type == SDL_KEYDOWN && e.key.repeat != 0) {
        return std::nullopt;
    }
#endif

    std::optional<Button> btn = std::nullopt;

    switch (e.key.keysym.sym) {
        // D-pad
        case SDLK_UP:        btn = Button::Up;     break;
        case SDLK_DOWN:      btn = Button::Down;   break;
        case SDLK_LEFT:      btn = Button::Left;   break;
        case SDLK_RIGHT:     btn = Button::Right;  break;

        // Face buttons
        case SDLK_z:         btn = Button::A;      break;
        case SDLK_x:         btn = Button::B;      break;
        case SDLK_a:
        case SDLK_c:         btn = Button::X;      break;
        case SDLK_s:
        case SDLK_v:         btn = Button::Y;      break;

        // Shoulders & Triggers
        case SDLK_q:
        case SDLK_PAGEUP:    btn = Button::L1;     break;
        case SDLK_e:
        case SDLK_PAGEDOWN:  btn = Button::R1;     break;
        case SDLK_w:
        case SDLK_HOME:      btn = Button::L2;     break;
        case SDLK_r:
        case SDLK_END:       btn = Button::R2;     break;

        // System & Menu
        case SDLK_RETURN:
        case SDLK_KP_ENTER:  btn = Button::Start;  break;
        case SDLK_TAB:       btn = Button::Select; break;
        case SDLK_ESCAPE:
        case SDLK_g:
        case SDLK_F12:       btn = Button::Guide;  break;

        default:
            return std::nullopt;
    }

    if (!btn) {
        return std::nullopt;
    }

    ButtonEventKind kind = (e.type == SDL_KEYDOWN) ? ButtonEventKind::Down : ButtonEventKind::Up;
    return ButtonEvent{ *btn, kind, false };
}

} // namespace hui

#endif // HUI_ENABLE_KEYBOARD_FALLBACK
