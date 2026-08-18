#include "doctest.h"
#include "hui/sdl/ButtonMapping.h"
#include "hui/sdl/SDLGamepadHelper.h"

#ifdef HUI_ENABLE_KEYBOARD_FALLBACK
#include "hui/sdl/KeyboardFallback.h"
#endif

#include <SDL.h>

TEST_CASE("ButtonMapping - default layouts and buttonFromName") {
    SUBCASE("buttonFromName") {
        CHECK(hui::buttonFromName("Up") == hui::Button::Up);
        CHECK(hui::buttonFromName("Down") == hui::Button::Down);
        CHECK(hui::buttonFromName("Left") == hui::Button::Left);
        CHECK(hui::buttonFromName("Right") == hui::Button::Right);
        CHECK(hui::buttonFromName("A") == hui::Button::A);
        CHECK(hui::buttonFromName("B") == hui::Button::B);
        CHECK(hui::buttonFromName("X") == hui::Button::X);
        CHECK(hui::buttonFromName("Y") == hui::Button::Y);
        CHECK(hui::buttonFromName("L1") == hui::Button::L1);
        CHECK(hui::buttonFromName("L2") == hui::Button::L2);
        CHECK(hui::buttonFromName("R1") == hui::Button::R1);
        CHECK(hui::buttonFromName("R2") == hui::Button::R2);
        CHECK(hui::buttonFromName("Start") == hui::Button::Start);
        CHECK(hui::buttonFromName("Select") == hui::Button::Select);
        CHECK(hui::buttonFromName("Guide") == hui::Button::Guide);

        CHECK_FALSE(hui::buttonFromName("Unknown").has_value());
        CHECK_FALSE(hui::buttonFromName("").has_value());
        CHECK_FALSE(hui::buttonFromName("invalid").has_value());
    }

    SUBCASE("defaultXboxLayout mappings") {
        auto xbox = hui::ButtonMapping::defaultXboxLayout();

        CHECK(xbox.controllerButtons[0] == hui::Button::A);
        CHECK(xbox.controllerButtons[1] == hui::Button::B);
        CHECK(xbox.controllerButtons[2] == hui::Button::X);
        CHECK(xbox.controllerButtons[3] == hui::Button::Y);
        CHECK(xbox.controllerButtons[4] == hui::Button::Select);
        CHECK(xbox.controllerButtons[5] == hui::Button::Guide);
        CHECK(xbox.controllerButtons[6] == hui::Button::Start);
        CHECK(xbox.controllerButtons[9] == hui::Button::L1);
        CHECK(xbox.controllerButtons[10] == hui::Button::R1);
        CHECK(xbox.controllerButtons[11] == hui::Button::Up);
        CHECK(xbox.controllerButtons[12] == hui::Button::Down);
        CHECK(xbox.controllerButtons[13] == hui::Button::Left);
        CHECK(xbox.controllerButtons[14] == hui::Button::Right);

        // Check axis bindings for L2, R2, Left Stick
        bool hasLeftX = false, hasLeftY = false, hasL2 = false, hasR2 = false;
        for (const auto& binding : xbox.axisBindings) {
            if (binding.axis == 0 && binding.positiveButton == hui::Button::Right && binding.negativeButton == hui::Button::Left) hasLeftX = true;
            if (binding.axis == 1 && binding.positiveButton == hui::Button::Down && binding.negativeButton == hui::Button::Up) hasLeftY = true;
            if (binding.axis == 4 && binding.positiveButton == hui::Button::L2) hasL2 = true;
            if (binding.axis == 5 && binding.positiveButton == hui::Button::R2) hasR2 = true;
        }
        CHECK(hasLeftX);
        CHECK(hasLeftY);
        CHECK(hasL2);
        CHECK(hasR2);
    }

    SUBCASE("defaultNintendoLayout swaps A/B and X/Y") {
        auto nintendo = hui::ButtonMapping::defaultNintendoLayout();
        auto xbox = hui::ButtonMapping::defaultXboxLayout();

        // Swapped
        CHECK(nintendo.controllerButtons[0] == hui::Button::B);
        CHECK(nintendo.controllerButtons[1] == hui::Button::A);
        CHECK(nintendo.controllerButtons[2] == hui::Button::Y);
        CHECK(nintendo.controllerButtons[3] == hui::Button::X);

        // All other buttons identical
        for (size_t i = 4; i < nintendo.controllerButtons.size(); ++i) {
            CHECK(nintendo.controllerButtons[i] == xbox.controllerButtons[i]);
        }

        // Axis bindings identical
        CHECK(nintendo.axisBindings.size() == xbox.axisBindings.size());
        for (size_t i = 0; i < nintendo.axisBindings.size(); ++i) {
            CHECK(nintendo.axisBindings[i].axis == xbox.axisBindings[i].axis);
            CHECK(nintendo.axisBindings[i].positiveButton == xbox.axisBindings[i].positiveButton);
            CHECK(nintendo.axisBindings[i].negativeButton == xbox.axisBindings[i].negativeButton);
            CHECK(nintendo.axisBindings[i].threshold == xbox.axisBindings[i].threshold);
        }
    }
}

TEST_CASE("SDLGamepadHelper - button and axis translation with hysteresis") {
    hui::SDLGamepadHelper helper(hui::ButtonMapping::defaultXboxLayout());

    SUBCASE("Button events") {
#ifndef HUI_USE_SDL1
        SDL_Event e{};
        e.type = SDL_CONTROLLERBUTTONDOWN;
        e.cbutton.button = 0; // Button A
        auto opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::A);
        CHECK(opt->kind == hui::ButtonEventKind::Down);
        CHECK_FALSE(opt->synthetic);

        e.type = SDL_CONTROLLERBUTTONUP;
        opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::A);
        CHECK(opt->kind == hui::ButtonEventKind::Up);

        // Unmapped button returns nullopt
        e.type = SDL_CONTROLLERBUTTONDOWN;
        e.cbutton.button = 25;
        CHECK_FALSE(helper.translate(e).has_value());
#else
        SDL_Event e{};
        e.type = SDL_JOYBUTTONDOWN;
        e.jbutton.button = 0; // Button A
        auto opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::A);
        CHECK(opt->kind == hui::ButtonEventKind::Down);

        e.type = SDL_JOYBUTTONUP;
        opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::A);
        CHECK(opt->kind == hui::ButtonEventKind::Up);
#endif
    }

    SUBCASE("Axis motion with threshold and hysteresis") {
        SDL_Event e{};
#ifndef HUI_USE_SDL1
        e.type = SDL_CONTROLLERAXISMOTION;
        e.caxis.axis = 0; // Left Stick X: Positive = Right, Negative = Left
        e.caxis.value = 20000; // Above 16384 threshold
#else
        e.type = SDL_JOYAXISMOTION;
        e.jaxis.axis = 0;
        e.jaxis.value = 20000;
#endif

        // 1. Initial push past threshold -> Down event for positive button (Right)
        auto opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::Right);
        CHECK(opt->kind == hui::ButtonEventKind::Down);

        // 2. Second event at +20000 -> No second event (hysteresis)
        opt = helper.translate(e);
        CHECK_FALSE(opt.has_value());

        // 3. Higher value +25000 -> Still active positive, no event
#ifndef HUI_USE_SDL1
        e.caxis.value = 25000;
#else
        e.jaxis.value = 25000;
#endif
        opt = helper.translate(e);
        CHECK_FALSE(opt.has_value());

        // 4. Return to deadzone (+100) -> Up event for positive button
#ifndef HUI_USE_SDL1
        e.caxis.value = 100;
#else
        e.jaxis.value = 100;
#endif
        opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::Right);
        CHECK(opt->kind == hui::ButtonEventKind::Up);

        // 5. Subsequent event inside deadzone (0) -> No event
#ifndef HUI_USE_SDL1
        e.caxis.value = 0;
#else
        e.jaxis.value = 0;
#endif
        opt = helper.translate(e);
        CHECK_FALSE(opt.has_value());

        // 6. Push negative past threshold (-20000) -> Down event for negative button (Left)
#ifndef HUI_USE_SDL1
        e.caxis.value = -20000;
#else
        e.jaxis.value = -20000;
#endif
        opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::Left);
        CHECK(opt->kind == hui::ButtonEventKind::Down);

        // 7. Second event at -20000 -> Hysteresis, no event
        opt = helper.translate(e);
        CHECK_FALSE(opt.has_value());

        // 8. Return to deadzone (-100) -> Up event for negative button (Left)
#ifndef HUI_USE_SDL1
        e.caxis.value = -100;
#else
        e.jaxis.value = -100;
#endif
        opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::Left);
        CHECK(opt->kind == hui::ButtonEventKind::Up);
    }

    SUBCASE("Triggers (L2 / R2) as positive-only axis") {
        SDL_Event e{};
#ifndef HUI_USE_SDL1
        e.type = SDL_CONTROLLERAXISMOTION;
        e.caxis.axis = 4; // Trigger Left (L2)
        e.caxis.value = 20000;
#else
        e.type = SDL_JOYAXISMOTION;
        e.jaxis.axis = 4;
        e.jaxis.value = 20000;
#endif
        auto opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::L2);
        CHECK(opt->kind == hui::ButtonEventKind::Down);

#ifndef HUI_USE_SDL1
        e.caxis.value = 0;
#else
        e.jaxis.value = 0;
#endif
        opt = helper.translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == hui::Button::L2);
        CHECK(opt->kind == hui::ButtonEventKind::Up);
    }
}

#ifdef HUI_ENABLE_KEYBOARD_FALLBACK
TEST_CASE("KeyboardFallback - translates desktop keyboard keys") {
    auto testKey = [](SDL_Keycode sym, hui::Button expected) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.sym = sym;
#ifndef HUI_USE_SDL1
        e.key.repeat = 0;
#endif
        auto opt = hui::KeyboardFallback::translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == expected);
        CHECK(opt->kind == hui::ButtonEventKind::Down);

        e.type = SDL_KEYUP;
        opt = hui::KeyboardFallback::translate(e);
        REQUIRE(opt.has_value());
        CHECK(opt->button == expected);
        CHECK(opt->kind == hui::ButtonEventKind::Up);
    };

    testKey(SDLK_UP, hui::Button::Up);
    testKey(SDLK_DOWN, hui::Button::Down);
    testKey(SDLK_LEFT, hui::Button::Left);
    testKey(SDLK_RIGHT, hui::Button::Right);
    testKey(SDLK_z, hui::Button::A);
    testKey(SDLK_x, hui::Button::B);
    testKey(SDLK_a, hui::Button::X);
    testKey(SDLK_c, hui::Button::X);
    testKey(SDLK_s, hui::Button::Y);
    testKey(SDLK_v, hui::Button::Y);
    testKey(SDLK_q, hui::Button::L1);
    testKey(SDLK_PAGEUP, hui::Button::L1);
    testKey(SDLK_e, hui::Button::R1);
    testKey(SDLK_PAGEDOWN, hui::Button::R1);
    testKey(SDLK_w, hui::Button::L2);
    testKey(SDLK_HOME, hui::Button::L2);
    testKey(SDLK_r, hui::Button::R2);
    testKey(SDLK_END, hui::Button::R2);
    testKey(SDLK_RETURN, hui::Button::Start);
    testKey(SDLK_TAB, hui::Button::Select);
    testKey(SDLK_ESCAPE, hui::Button::Guide);
    testKey(SDLK_g, hui::Button::Guide);

#ifndef HUI_USE_SDL1
    // Auto-repeat keydown events are ignored
    SDL_Event e{};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_UP;
    e.key.repeat = 1;
    CHECK_FALSE(hui::KeyboardFallback::translate(e).has_value());
#endif

    // Unmapped key returns nullopt
    SDL_Event unmapped{};
    unmapped.type = SDL_KEYDOWN;
    unmapped.key.keysym.sym = SDLK_7;
    CHECK_FALSE(hui::KeyboardFallback::translate(unmapped).has_value());
}
#endif
