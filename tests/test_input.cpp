#include "doctest.h"
#include "hui/KeyRepeatDriver.h"
#include "hui/ChordDetector.h"
#include <vector>
#include <cmath>

using namespace hui;

TEST_CASE("KeyRepeatDriver - Initial delay and repeat timing") {
    KeyRepeatDriver driver;
    std::vector<ButtonEvent> events;
    auto sink = [&](ButtonEvent e) { events.push_back(e); };

    driver.onButtonDown(Button::Down);

    // 0.25s: zero synthetic events (within 300ms initial delay)
    driver.update(0.250f, sink);
    CHECK(events.empty());

    // 0.10s more (total 0.35s): exactly one synthetic repeat
    driver.update(0.100f, sink);
    CHECK(events.size() == 1);
    CHECK(events[0].button == Button::Down);
    CHECK(events[0].kind == ButtonEventKind::Down);
    CHECK(events[0].synthetic == true);

    // Next 5 repeats at 100ms intervals
    events.clear();
    for (int i = 0; i < 5; ++i) {
        driver.update(0.100f, sink);
    }
    CHECK(events.size() == 5);
    for (const auto& e : events) {
        CHECK(e.button == Button::Down);
        CHECK(e.kind == ButtonEventKind::Down);
        CHECK(e.synthetic == true);
    }
}

TEST_CASE("KeyRepeatDriver - Fast repeat after 1.0s") {
    KeyRepeatDriver driver;
    std::vector<ButtonEvent> events;
    auto sink = [&](ButtonEvent e) { events.push_back(e); };

    driver.onButtonDown(Button::Right);

    // Advance to > 1.0s (e.g. 1.05s total in steps of 0.05s)
    for (int i = 0; i < 21; ++i) {
        driver.update(0.050f, sink);
    }

    // Now test fast repeats (30ms interval).
    events.clear();
    // 5 fast repeats should occur within 5 * 0.030s = 0.150s
    driver.update(0.150f, sink);
    CHECK(events.size() >= 5);
    for (const auto& e : events) {
        CHECK(e.button == Button::Right);
        CHECK(e.kind == ButtonEventKind::Down);
        CHECK(e.synthetic == true);
    }
}

TEST_CASE("KeyRepeatDriver - Reset countdown on release/re-press") {
    KeyRepeatDriver driver;
    std::vector<ButtonEvent> events;
    auto sink = [&](ButtonEvent e) { events.push_back(e); };

    driver.onButtonDown(Button::Up);
    driver.update(0.250f, sink);
    CHECK(events.empty());

    // Release and re-press
    driver.onButtonUp(Button::Up);
    driver.onButtonDown(Button::Up);

    // 0.25s again: still zero synthetic events because countdown reset
    driver.update(0.250f, sink);
    CHECK(events.empty());
}

TEST_CASE("KeyRepeatDriver - Non-repeating buttons") {
    KeyRepeatDriver driver;
    std::vector<ButtonEvent> events;
    auto sink = [&](ButtonEvent e) { events.push_back(e); };

    const Button nonRepeating[] = {
        Button::A, Button::B, Button::X, Button::Y,
        Button::Start, Button::Select, Button::Guide
    };

    for (Button b : nonRepeating) {
        CHECK_FALSE(KeyRepeatDriver::shouldRepeat(b));
        driver.onButtonDown(b);
        driver.update(2.000f, sink);
        CHECK(events.empty());
    }
}

TEST_CASE("KeyRepeatDriver - Synthetic events are always Down and synthetic==true") {
    KeyRepeatDriver driver;
    std::vector<ButtonEvent> events;
    auto sink = [&](ButtonEvent e) { events.push_back(e); };

    driver.onButtonDown(Button::L1);
    driver.update(2.000f, sink);

    CHECK_FALSE(events.empty());
    for (const auto& e : events) {
        CHECK(e.synthetic == true);
        CHECK(e.kind == ButtonEventKind::Down);
    }
}

TEST_CASE("KeyRepeatDriver - flushHeld()") {
    KeyRepeatDriver driver;
    std::vector<ButtonEvent> events;
    auto sink = [&](ButtonEvent e) { events.push_back(e); };

    driver.onButtonDown(Button::Down);
    driver.update(0.350f, sink);
    CHECK(events.size() == 1);

    driver.flushHeld();
    events.clear();

    // Holding further yields 0 repeats after flushHeld
    driver.update(1.000f, sink);
    CHECK(events.empty());

    // Harmless subsequent onButtonUp
    driver.onButtonUp(Button::Down);
    CHECK(events.empty());
}

TEST_CASE("KeyRepeatDriver - Large dt clamp safety") {
    KeyRepeatDriver driver;
    std::vector<ButtonEvent> events;
    auto sink = [&](ButtonEvent e) { events.push_back(e); };

    driver.onButtonDown(Button::Down);
    // Simulating screen-off wake or lag spike: update with dt = 1200.0s
    driver.update(1200.0f, sink);
    // Should emit a handful of repeats, NOT tens of thousands
    CHECK(events.size() <= 25);
}

TEST_CASE("ChordDetector - Start + Select -> Guide") {
    ChordDetector chords;

    // Press Start (pending chord)
    auto r1 = chords.onButtonDown(Button::Start);
    CHECK_FALSE(r1.has_value());

    // Press Select within 150ms -> fires Guide
    auto r2 = chords.onButtonDown(Button::Select);
    REQUIRE(r2.has_value());
    CHECK(*r2 == Button::Guide);

    // Release Start and Select -> suppressed, neither Start nor Select emitted
    auto u1 = chords.onButtonUp(Button::Start);
    auto u2 = chords.onButtonUp(Button::Select);

    // At least one releases Guide, neither releases Start or Select
    bool releasedGuide = (u1.has_value() && *u1 == Button::Guide) || (u2.has_value() && *u2 == Button::Guide);
    bool releasedStartOrSelect = (u1.has_value() && (*u1 == Button::Start || *u1 == Button::Select)) ||
                                 (u2.has_value() && (*u2 == Button::Start || *u2 == Button::Select));

    CHECK(releasedGuide);
    CHECK_FALSE(releasedStartOrSelect);
}

TEST_CASE("ChordDetector - Start alone past window") {
    ChordDetector chords;
    std::vector<ButtonEvent> events;
    auto sink = [&](ButtonEvent e) { events.push_back(e); };

    auto r1 = chords.onButtonDown(Button::Start);
    CHECK_FALSE(r1.has_value());

    // Wait past chord window (150ms)
    chords.update(0.160f, sink);

    REQUIRE(events.size() == 1);
    CHECK(events[0].button == Button::Start);
    CHECK(events[0].kind == ButtonEventKind::Down);
    CHECK(events[0].synthetic == false);

    // ButtonUp for Start returns Start
    auto u1 = chords.onButtonUp(Button::Start);
    REQUIRE(u1.has_value());
    CHECK(*u1 == Button::Start);
}
