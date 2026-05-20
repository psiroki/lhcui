#include "doctest.h"
#include "hui/types.h"

using namespace hui;

TEST_CASE("Geometry functions") {
    Rect r{0, 0, 100, 80};
    
    SUBCASE("rectContains returns true for a point on the border") {
        CHECK(rectContains(r, Point{0, 0}) == true);
        CHECK(rectContains(r, Point{100, 80}) == true);
        CHECK(rectContains(r, Point{0, 80}) == true);
        CHECK(rectContains(r, Point{100, 0}) == true);
        CHECK(rectContains(r, Point{50, 0}) == true);
    }
    
    SUBCASE("rectContains returns false for a point one pixel outside") {
        CHECK(rectContains(r, Point{-1, 0}) == false);
        CHECK(rectContains(r, Point{0, -1}) == false);
        CHECK(rectContains(r, Point{101, 80}) == false);
        CHECK(rectContains(r, Point{100, 81}) == false);
    }
    
    SUBCASE("rectInset behaves correctly") {
        Rect inset = rectInset(r, 5, 5);
        CHECK(inset.x == 5);
        CHECK(inset.y == 5);
        CHECK(inset.w == 90);
        CHECK(inset.h == 70);
    }
}

TEST_CASE("Color helpers") {
    SUBCASE("Color::lerp at limits and midpoint") {
        Color c1{100, 100, 100, 255};
        Color c2{200, 200, 200, 255};
        
        Color t0 = c1.lerp(c2, 0.0f);
        CHECK(t0.r == 100);
        CHECK(t0.g == 100);
        CHECK(t0.b == 100);
        CHECK(t0.a == 255);
        
        Color t1 = c1.lerp(c2, 1.0f);
        CHECK(t1.r == 200);
        CHECK(t1.g == 200);
        CHECK(t1.b == 200);
        CHECK(t1.a == 255);
        
        Color tHalf = c1.lerp(c2, 0.5f);
        CHECK(tHalf.r == 150);
        CHECK(tHalf.g == 150);
        CHECK(tHalf.b == 150);
        CHECK(tHalf.a == 255);
    }
    
    SUBCASE("Color::withAlpha preserves rgb") {
        Color orig{10, 20, 30, 255};
        Color transp = orig.withAlpha(128);
        CHECK(transp.r == 10);
        CHECK(transp.g == 20);
        CHECK(transp.b == 30);
        CHECK(transp.a == 128);
    }
}

TEST_CASE("Button events") {
    SUBCASE("ButtonEvent is synthetic=false by default") {
        ButtonEvent e;
        CHECK(e.synthetic == false);
    }
}
