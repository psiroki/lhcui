#include "doctest.h"
#include "hui/FocusManager.h"
#include "hui/NavList.h"

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
    bool focusable = true;

    bool isFocusable() const override { return focusable; }

    void onFocus() override { ++focusCount; }
    void onBlur()  override { ++blurCount;  }

    // draw() signature updated for rev2 (no Rect)
    void draw(IRenderer& /*renderer*/, const Theme& /*theme*/) override {}
};

TEST_CASE("Widget — layout and bounds round-trip") {
    TestWidget w;
    CHECK(w.bounds().x == 0);
    CHECK(w.bounds().y == 0);
    CHECK(w.bounds().w == 0);
    CHECK(w.bounds().h == 0);

    w.layout({10, 20, 100, 50});
    CHECK(w.bounds().x == 10);
    CHECK(w.bounds().y == 20);
    CHECK(w.bounds().w == 100);
    CHECK(w.bounds().h == 50);
}

TEST_CASE("FocusManager — setFocus A then B") {
    FocusManager fm;
    TestWidget a, b;

    CHECK(fm.setFocus(&a) == true);
    REQUIRE(a.isFocused());
    REQUIRE(!b.isFocused());
    REQUIRE(a.focusCount == 1);
    REQUIRE(a.blurCount  == 0);
    REQUIRE(b.focusCount == 0);

    CHECK(fm.setFocus(&b) == true);
    CHECK(a.isFocused() == false);
    CHECK(b.isFocused() == true);
    CHECK(a.blurCount  == 1);
    CHECK(a.focusCount == 1);
    CHECK(b.focusCount == 1);
    CHECK(b.blurCount  == 0);
}

TEST_CASE("FocusManager — setFocus refuses non-focusable or disabled widgets") {
    FocusManager fm;
    TestWidget a;
    TestWidget nonFocusable;
    nonFocusable.focusable = false;

    TestWidget disabled;
    disabled.setDisabled(true);

    fm.setFocus(&a);
    CHECK(fm.focused() == &a);

    // Refuses non-focusable widget
    CHECK(fm.setFocus(&nonFocusable) == false);
    CHECK(fm.focused() == &a);
    CHECK(nonFocusable.isFocused() == false);

    // Refuses disabled widget
    CHECK(fm.setFocus(&disabled) == false);
    CHECK(fm.focused() == &a);
    CHECK(disabled.isFocused() == false);
}

TEST_CASE("FocusManager — setFocus on already-focused widget is a no-op") {
    FocusManager fm;
    TestWidget a;

    fm.setFocus(&a);
    CHECK(a.focusCount == 1);
    CHECK(a.blurCount  == 0);

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

TEST_CASE("FocusManager — forceOwner updates flags without callbacks") {
    FocusManager fm;
    TestWidget a, b;

    fm.setFocus(&a);
    int prevFocusA = a.focusCount;
    int prevBlurA  = a.blurCount;

    CHECK(fm.forceOwner(&b) == true);
    CHECK(fm.focused() == &b);
    CHECK(b.isFocused() == true);
    CHECK(a.isFocused() == false);

    // Zero callbacks fired on either widget during forceOwner
    CHECK(a.blurCount  == prevBlurA);
    CHECK(a.focusCount == prevFocusA);
    CHECK(b.focusCount == 0);
    CHECK(b.blurCount  == 0);
}

TEST_CASE("NavList — vertical navigation and skipping disabled/non-focusable") {
    FocusManager fm;
    TestWidget w1, w2, w3;
    w2.setDisabled(true); // middle entry disabled

    NavList nav;
    nav.setAxis(Axis::Vertical);
    nav.setWrap(true);
    nav.add(&w1);
    nav.add(&w2);
    nav.add(&w3);

    // Initialize focus on first item
    nav.focusIndex(0, fm);
    CHECK(fm.focused() == &w1);

    // Down skips disabled w2 and lands on w3
    CHECK(nav.handleButton(Button::Down, fm) == true);
    CHECK(fm.focused() == &w3);
    CHECK(nav.index() == 2);

    // Down from w3 wraps to w1
    CHECK(nav.handleButton(Button::Down, fm) == true);
    CHECK(fm.focused() == &w1);
    CHECK(nav.index() == 0);

    // Up from w1 wraps to w3
    CHECK(nav.handleButton(Button::Up, fm) == true);
    CHECK(fm.focused() == &w3);

    // Off-axis buttons (Left/Right) return false
    CHECK(nav.handleButton(Button::Left, fm) == false);
    CHECK(nav.handleButton(Button::Right, fm) == false);
}

TEST_CASE("NavList — horizontal without wrap") {
    FocusManager fm;
    TestWidget w1, w2;

    NavList nav;
    nav.setAxis(Axis::Horizontal);
    nav.setWrap(false);
    nav.add(&w1);
    nav.add(&w2);

    nav.focusIndex(0, fm);
    CHECK(fm.focused() == &w1);

    CHECK(nav.handleButton(Button::Right, fm) == true);
    CHECK(fm.focused() == &w2);

    // At last entry, Right returns true (consumed) but does not wrap
    CHECK(nav.handleButton(Button::Right, fm) == true);
    CHECK(fm.focused() == &w2);
}

TEST_CASE("NavList — all entries disabled") {
    FocusManager fm;
    TestWidget w1, w2;
    w1.setDisabled(true);
    w2.setDisabled(true);

    NavList nav;
    nav.add(&w1);
    nav.add(&w2);

    CHECK(nav.handleButton(Button::Down, fm) == false);
    CHECK(fm.focused() == nullptr);
}
