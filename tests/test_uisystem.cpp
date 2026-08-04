#include "doctest.h"
#include "hui/UISystem.h"
#include "hui/Shell.h"
#include "hui/View.h"
#include "hui/IRenderer.h"
#include <vector>
#include <string>

using namespace hui;

namespace {

class DummyRenderer : public IRenderer {
public:
    int fillRectCount = 0;
    int endFrameCount = 0;

    void beginFrame() override {}
    void endFrame() override { endFrameCount++; }
    void pushClip(Rect r) override { (void)r; }
    void popClip() override {}
    void fillRect(Rect r, Color c) override { (void)r; (void)c; fillRectCount++; }
    void drawRect(Rect r, Color c, int t) override { (void)r; (void)c; (void)t; }
    void drawLine(Point a, Point b, Color c) override { (void)a; (void)b; (void)c; }
    int drawText(std::string_view t, Point o, FontHandle f, Color c) override { (void)t; (void)o; (void)f; (void)c; return 0; }
    Size measureText(std::string_view t, FontHandle f) override { (void)t; (void)f; return {0, 0}; }
    void drawTextEllipsis(std::string_view t, Point o, FontHandle f, Color c, int w) override { (void)t; (void)o; (void)f; (void)c; (void)w; }
    TextureHandle loadTexture(std::string_view p) override { (void)p; return 0; }
    void freeTexture(TextureHandle h) override { (void)h; }
    Size textureSize(TextureHandle h) override { (void)h; return {0, 0}; }
    void drawTexture(TextureHandle h, Rect d, uint8_t a) override { (void)h; (void)d; (void)a; }
    void setGlobalAlpha(uint8_t a) override { (void)a; }
    Size screenSize() const override { return {640, 480}; }
};

class DummyView : public View {
public:
    std::vector<Button> receivedDowns;
    std::vector<Button> receivedUps;
    bool popSelfOnDown = false;
    bool pushNewViewOnRepeat = false;
    ViewStack* stackRef = nullptr;
    bool destroyed = false;
    bool updateRanAfterPush = false;

    ~DummyView() override {
        destroyed = true;
    }

    bool onButtonDown(Button b, FocusManager& fm) override {
        (void)fm;
        receivedDowns.push_back(b);
        if (popSelfOnDown && stackRef) {
            stackRef->pop();
        }
        if (pushNewViewOnRepeat && b == Button::Down && stackRef) {
            stackRef->push(std::make_unique<DummyView>());
        }
        return true;
    }

    bool onButtonUp(Button b, FocusManager& fm) override {
        (void)fm;
        receivedUps.push_back(b);
        return true;
    }

    void update(float dt, FocusManager& fm) override {
        (void)dt; (void)fm;
        if (pushNewViewOnRepeat) {
            updateRanAfterPush = true;
        }
    }

    void draw(IRenderer& r, const Theme& theme) override {
        (void)r; (void)theme;
    }
};

class MockShell : public Shell {
public:
    int chromeDrawCount = 0;
    int overlayDrawCount = 0;

    explicit MockShell(ViewStack& stack) : Shell(stack) {}

    void draw(IRenderer& r, const Theme& theme) override {
        (void)r; (void)theme;
    }

    void drawChrome(IRenderer& r, const Theme& theme) override {
        (void)r; (void)theme;
        chromeDrawCount++;
    }

    void drawOverlay(IRenderer& r, const Theme& theme) override {
        (void)r; (void)theme;
        overlayDrawCount++;
    }
};

} // namespace

TEST_CASE("UISystem - Real button event reaches view immediately") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    auto view = std::make_unique<DummyView>();
    auto* rawView = view.get();
    ui.viewStack().push(std::move(view));
    ui.update(0.016f); // apply push

    ui.onButtonDown(Button::A);
    CHECK(rawView->receivedDowns.size() == 1);
    CHECK(rawView->receivedDowns[0] == Button::A);
}

TEST_CASE("UISystem - Synthetic repeats reach view in update()") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    auto view = std::make_unique<DummyView>();
    auto* rawView = view.get();
    ui.viewStack().push(std::move(view));
    ui.update(0.016f);

    ui.onButtonDown(Button::Down);
    CHECK(rawView->receivedDowns.size() == 1);

    // Update in frame steps of 0.100s up to 0.400s to trigger synthetic repeat
    ui.update(0.100f);
    ui.update(0.100f);
    ui.update(0.100f);
    ui.update(0.100f);
    CHECK(rawView->receivedDowns.size() >= 2);
}

TEST_CASE("UISystem - onButtonUp stops repeats") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    auto view = std::make_unique<DummyView>();
    auto* rawView = view.get();
    ui.viewStack().push(std::move(view));
    ui.update(0.016f);

    ui.onButtonDown(Button::Down);
    ui.onButtonUp(Button::Down);

    ui.update(0.100f);
    ui.update(0.100f);
    ui.update(0.100f);
    ui.update(0.100f);
    CHECK(rawView->receivedDowns.size() == 1); // Only initial down, no synthetic repeats
}

TEST_CASE("UISystem - Animations enabled flag") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    CHECK(ui.animationsEnabled());
    ui.setAnimationsEnabled(false);
    CHECK_FALSE(ui.animationsEnabled());
}

TEST_CASE("UISystem - Independent instances") {
    DummyRenderer r1, r2;
    Theme theme{};
    UISystem ui1(r1, theme), ui2(r2, theme);

    auto v1 = std::make_unique<DummyView>();
    auto v2 = std::make_unique<DummyView>();
    auto* raw1 = v1.get();
    auto* raw2 = v2.get();

    ui1.viewStack().push(std::move(v1));
    ui2.viewStack().push(std::move(v2));

    ui1.update(0.016f);
    ui2.update(0.016f);

    ui1.onButtonDown(Button::A);

    CHECK(raw1->receivedDowns.size() == 1);
    CHECK(raw2->receivedDowns.size() == 0);
}

TEST_CASE("UISystem - dt clamp safety") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    auto view = std::make_unique<DummyView>();
    auto* rawView = view.get();
    ui.viewStack().push(std::move(view));
    ui.update(0.016f);

    ui.onButtonDown(Button::Down);
    // Simulating screen off / long lag spike with 1200 seconds
    ui.update(1200.0f);

    // Should clamp dt to 0.100s, producing at most 1 synthetic event for 0.100s
    CHECK(rawView->receivedDowns.size() <= 3);
}

TEST_CASE("UISystem - View calling pop() in handler is destroyed on next pending mutation") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    // Push root view + top view
    ui.viewStack().push(std::make_unique<DummyView>());
    auto view = std::make_unique<DummyView>();
    auto* rawView = view.get();
    rawView->popSelfOnDown = true;
    rawView->stackRef = &ui.viewStack();
    ui.viewStack().push(std::move(view));

    ui.update(0.016f); // Apply pushes
    CHECK(ui.viewStack().size() == 2);

    ui.onButtonDown(Button::B); // Pop enqueued
    CHECK(ui.viewStack().size() == 1); // Old top view popped on pending mutation apply
}

TEST_CASE("UISystem - Synthetic repeat triggering push applies before ViewStack update()") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    auto view = std::make_unique<DummyView>();
    auto* rawView = view.get();
    rawView->pushNewViewOnRepeat = true;
    rawView->stackRef = &ui.viewStack();

    ui.viewStack().push(std::move(view));
    ui.update(0.016f);

    // Hold down until synthetic repeat fires inside update()
    ui.onButtonDown(Button::Down);
    ui.update(0.100f);
    ui.update(0.100f);
    ui.update(0.100f);
    ui.update(0.100f);

    // Stack should now have 2 views
    CHECK(ui.viewStack().size() == 2);
}

TEST_CASE("UISystem - Pushing overlay clears held keys for overlay") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    auto baseView = std::make_unique<DummyView>();
    auto* rawBase = baseView.get();
    ui.viewStack().push(std::move(baseView));
    ui.update(0.016f);

    ui.onButtonDown(Button::Down);

    // Push overlay view
    auto overlayView = std::make_unique<DummyView>();
    auto* rawOverlay = overlayView.get();
    ui.viewStack().push(std::move(overlayView));

    // Update applies push mutation -> flushHeld() runs
    ui.update(0.500f);

    // Overlay received 0 repeats from the still-held Down button
    CHECK(rawOverlay->receivedDowns.empty());
    (void)rawBase;
}

TEST_CASE("UISystem - Suspended state draw and input behavior") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    auto view = std::make_unique<DummyView>();
    auto* rawView = view.get();
    ui.viewStack().push(std::move(view));
    ui.update(0.016f);

    ui.setSuspended(true);
    CHECK(ui.isSuspended());

    // First two draws after suspending clear to black and present to clear both double-buffers
    ui.draw();
    ui.draw();
    CHECK(renderer.fillRectCount == 2);
    CHECK(renderer.endFrameCount == 2);

    // Next 10 draw calls issue zero renderer commands
    for (int i = 0; i < 10; ++i) {
        ui.draw();
    }
    CHECK(renderer.fillRectCount == 2);
    CHECK(renderer.endFrameCount == 2);

    // Input while suspended does not reach top view
    ui.onButtonDown(Button::A);
    CHECK(rawView->receivedDowns.empty());

    // Resume
    ui.setSuspended(false);
    CHECK_FALSE(ui.isSuspended());

    // Next press reaches view
    ui.onButtonDown(Button::A);
    CHECK(rawView->receivedDowns.size() == 1);
}

TEST_CASE("UISystem - Shell chrome and overlay drawing") {
    DummyRenderer renderer;
    Theme theme{};
    UISystem ui(renderer, theme);

    MockShell shell(ui.viewStack());
    ui.setShell(&shell);
    CHECK(ui.shell() == &shell);

    ui.draw();
    CHECK(shell.chromeDrawCount == 1);
    CHECK(shell.overlayDrawCount == 1);

    // Clear shell
    ui.setShell(nullptr);
    CHECK(ui.shell() == nullptr);
    ui.draw(); // Works and draws only viewstack
}
