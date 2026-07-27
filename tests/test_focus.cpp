#include "doctest.h"
#include "hui/FocusManager.h"

#include <string>

using namespace hui;

// ---------------------------------------------------------------------------
// Minimal concrete Widget subclass for testing.
// Counts how many times onFocus / onBlur have been called.
// ---------------------------------------------------------------------------
class TestWidget : public Widget {
public:
    int focusCount = 0;
    int blurCount  = 0;

    void onFocus() override { ++focusCount; }
    void onBlur()  override { ++blurCount;  }

    // draw() is pure-virtual in Widget; provide a no-op.
    void draw(IRenderer& /*renderer*/, Rect /*r*/, const Theme& /*theme*/) override {}
};

// ---------------------------------------------------------------------------
// Tests covering the Phase 4 QA sign-off checklist:
//
//  1. setFocus(A) then setFocus(B): A gets one onBlur, B gets one onFocus.
//  2. setFocus on the already-focused widget: no second onBlur / onFocus.
//  3. setFocus(nullptr): onBlur fires on current; focused() returns nullptr.
//  4. forceOwner(W): sets focused() to W without any callbacks.
//  5. isFocused() is true only while the widget is the current owner.
//  6. setDisabled / isDisabled round-trip.
// ---------------------------------------------------------------------------

TEST_CASE("FocusManager — setFocus A then B") {
    FocusManager fm;
    TestWidget a, b;

    fm.setFocus(&a);
    REQUIRE(a.isFocused());
    REQUIRE(!b.isFocused());
    REQUIRE(a.focusCount == 1);
    REQUIRE(a.blurCount  == 0);
    REQUIRE(b.focusCount == 0);

    fm.setFocus(&b);
    CHECK(a.isFocused() == false);
    CHECK(b.isFocused() == true);
    // A received exactly one onBlur
    CHECK(a.blurCount  == 1);
    CHECK(a.focusCount == 1);
    // B received exactly one onFocus
    CHECK(b.focusCount == 1);
    CHECK(b.blurCount  == 0);
}

TEST_CASE("FocusManager — setFocus on already-focused widget is a no-op") {
    FocusManager fm;
    TestWidget a;

    fm.setFocus(&a);
    CHECK(a.focusCount == 1);
    CHECK(a.blurCount  == 0);

    // Call again with the same widget — must NOT fire callbacks a second time.
    fm.setFocus(&a);
    CHECK(a.focusCount == 1);
    CHECK(a.blurCount  == 0);
    CHECK(a.isFocused() == true);
}

TEST_CASE("FocusManager — setFocus(nullptr) clears focus") {
    FocusManager fm;
    TestWidget a;

    fm.setFocus(&a);
    REQUIRE(a.isFocused());

    fm.setFocus(nullptr);
    CHECK(fm.focused() == nullptr);
    CHECK(a.isFocused() == false);
    CHECK(a.blurCount == 1);
}

TEST_CASE("FocusManager — forceOwner does not fire callbacks") {
    FocusManager fm;
    TestWidget a, b;

    // Give focus to a normally (fires onFocus on a).
    fm.setFocus(&a);
    int prevFocusA = a.focusCount;
    int prevBlurA  = a.blurCount;

    // forceOwner(b) must bypass all lifecycle callbacks.
    fm.forceOwner(&b);
    CHECK(fm.focused() == &b);

    // a must not have received an onBlur
    CHECK(a.blurCount  == prevBlurA);
    CHECK(a.focusCount == prevFocusA);

    // b must not have received an onFocus
    CHECK(b.focusCount == 0);
    CHECK(b.blurCount  == 0);
}

TEST_CASE("Widget — isFocused tracks FocusManager state") {
    FocusManager fm;
    TestWidget w;

    CHECK(w.isFocused() == false);

    fm.setFocus(&w);
    CHECK(w.isFocused() == true);

    fm.setFocus(nullptr);
    CHECK(w.isFocused() == false);
}

TEST_CASE("Widget — setDisabled / isDisabled round-trips") {
    TestWidget w;

    CHECK(w.isDisabled() == false);

    w.setDisabled(true);
    CHECK(w.isDisabled() == true);

    w.setDisabled(false);
    CHECK(w.isDisabled() == false);
}

TEST_CASE("FocusManager — hasFocus returns correct results") {
    FocusManager fm;
    TestWidget a, b;

    fm.setFocus(&a);
    CHECK(fm.hasFocus(&a) == true);
    CHECK(fm.hasFocus(&b) == false);

    fm.setFocus(&b);
    CHECK(fm.hasFocus(&a) == false);
    CHECK(fm.hasFocus(&b) == true);
}
