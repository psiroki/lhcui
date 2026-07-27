#include "doctest.h"
#include "hui/View.h"
#include "hui/ViewStack.h"
#include "hui/FocusManager.h"
#include "hui/Widget.h"
#include "hui/IRenderer.h"

#include <string>
#include <vector>

using namespace hui;

// ---------------------------------------------------------------------------
// Mock IRenderer for testing draw() and alpha dimming.
// ---------------------------------------------------------------------------
class MockRenderer : public IRenderer {
public:
    uint8_t lastAlpha = 255;
    std::vector<uint8_t> alphaHistory;

    void beginFrame() override {}
    void endFrame() override {}
    void pushClip(Rect) override {}
    void popClip() override {}
    void fillRect(Rect, Color) override {}
    void drawRect(Rect, Color, int) override {}
    void drawLine(Point, Point, Color) override {}
    int drawText(std::string_view, Point, FontHandle, Color) override { return 0; }
    Size measureText(std::string_view, FontHandle) override { return {0, 0}; }
    void drawTextEllipsis(std::string_view, Point, FontHandle, Color, int) override {}
    TextureHandle loadTexture(std::string_view) override { return 0; }
    void freeTexture(TextureHandle) override {}
    Size textureSize(TextureHandle) override { return {0, 0}; }
    void drawTexture(TextureHandle, Rect, uint8_t) override {}

    void setGlobalAlpha(uint8_t alpha) override {
        lastAlpha = alpha;
        alphaHistory.push_back(alpha);
    }

    Size screenSize() const override { return {640, 480}; }
};

// ---------------------------------------------------------------------------
// Minimal concrete Widget subclass for testing focus restoration.
// ---------------------------------------------------------------------------
class TestWidget : public Widget {
public:
    int focusCount = 0;
    int blurCount  = 0;

    void onFocus() override { ++focusCount; }
    void onBlur()  override { ++blurCount;  }
    void draw(IRenderer&, Rect, const Theme&) override {}
};

// ---------------------------------------------------------------------------
// Instrumented View subclass for tracing call order.
// ---------------------------------------------------------------------------
class InstrumentedView : public View {
public:
    std::string name;
    std::vector<std::string>* eventLog;
    bool handleButton = false;
    bool buttonReceived = false;

    // Track recorded dimmed states at draw time
    bool wasDimmedOnDraw = false;
    uint8_t alphaOnDraw = 0;

    InstrumentedView(std::string name, std::vector<std::string>* log)
        : name(std::move(name)), eventLog(log) {}

    void onPush() override {
        if (eventLog) eventLog->push_back(name + "::onPush");
    }
    void onPop() override {
        if (eventLog) eventLog->push_back(name + "::onPop");
    }
    void onResume() override {
        if (eventLog) eventLog->push_back(name + "::onResume");
    }
    void onSuspend() override {
        if (eventLog) eventLog->push_back(name + "::onSuspend");
    }

    void draw(IRenderer& r, const Theme&) override {
        wasDimmedOnDraw = isDimmed();
        if (auto* mr = dynamic_cast<MockRenderer*>(&r)) {
            alphaOnDraw = mr->lastAlpha;
        }
    }

    bool onButtonDown(Button, FocusManager&) override {
        buttonReceived = true;
        return handleButton;
    }
};

class ViewTypeA : public View {
public:
    HUI_VIEW_TYPE(ViewTypeA)
    void draw(IRenderer&, const Theme&) override {}
};

class ViewTypeB : public View {
public:
    HUI_VIEW_TYPE(ViewTypeB)
    void draw(IRenderer&, const Theme&) override {}
};

class ViewTypeC : public View {
public:
    HUI_VIEW_TYPE(ViewTypeC)
    void draw(IRenderer&, const Theme&) override {}
};

// ---------------------------------------------------------------------------
// Tests covering Phase 5 QA sign-off checklist
// ---------------------------------------------------------------------------

TEST_CASE("ViewStack — push call order (A onSuspend before B onPush)") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack;

    stack.push(std::make_unique<InstrumentedView>("A", &log), &fm);
    CHECK(log == std::vector<std::string>{"A::onPush"});

    log.clear();
    stack.push(std::make_unique<InstrumentedView>("B", &log), &fm);
    CHECK(log == std::vector<std::string>{"A::onSuspend", "B::onPush"});
}

TEST_CASE("ViewStack — pop call order (B onPop before A onResume)") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack;

    stack.push(std::make_unique<InstrumentedView>("A", &log), &fm);
    stack.push(std::make_unique<InstrumentedView>("B", &log), &fm);

    log.clear();
    stack.pop(&fm);
    CHECK(log == std::vector<std::string>{"B::onPop", "A::onResume"});
}

TEST_CASE("ViewStack — pop on single-entry stack is no-op") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack;

    stack.push(std::make_unique<InstrumentedView>("A", &log), &fm);
    CHECK(stack.size() == 1);

    log.clear();
    stack.pop(&fm);
    CHECK(stack.size() == 1);
    CHECK(stack.top() != nullptr);
    CHECK(log.empty()); // No onPop or onResume fired
}

TEST_CASE("ViewStack — dispatchButtonDown delivers event ONLY to top view") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack;

    auto viewA = std::make_unique<InstrumentedView>("A", &log);
    auto viewB = std::make_unique<InstrumentedView>("B", &log);

    InstrumentedView* ptrA = viewA.get();
    InstrumentedView* ptrB = viewB.get();

    stack.push(std::move(viewA), &fm);
    stack.push(std::move(viewB), &fm);

    bool consumed = stack.dispatchButtonDown(Button::A, fm);
    CHECK(consumed == false);
    CHECK(ptrB->buttonReceived == true);
    CHECK(ptrA->buttonReceived == false);
}

TEST_CASE("ViewStack — alpha dimming during draw()") {
    FocusManager fm;
    ViewStack stack;

    std::vector<std::string> log;
    auto viewA = std::make_unique<InstrumentedView>("A", &log);
    auto viewB = std::make_unique<InstrumentedView>("B", &log);

    InstrumentedView* ptrA = viewA.get();
    InstrumentedView* ptrB = viewB.get();

    stack.push(std::move(viewA), &fm);
    stack.push(std::move(viewB), &fm);

    MockRenderer renderer;
    Theme theme{};

    stack.draw(renderer, theme);

    // Non-top view (A) drawn dimmed with global alpha 128
    CHECK(ptrA->wasDimmedOnDraw == true);
    CHECK(ptrA->alphaOnDraw == 128);

    // Top view (B) drawn non-dimmed with global alpha 255
    CHECK(ptrB->wasDimmedOnDraw == false);
    CHECK(ptrB->alphaOnDraw == 255);
}

TEST_CASE("View — suspendFocus and restoreFocus via forceOwner") {
    FocusManager fm;
    TestWidget widgetA;
    TestWidget widgetB;

    ViewStack stack;
    std::vector<std::string> log;

    // Push base View A
    stack.push(std::make_unique<InstrumentedView>("A", &log), &fm);

    // Focus widgetA on View A
    fm.setFocus(&widgetA);
    CHECK(widgetA.isFocused());
    CHECK(widgetA.focusCount == 1);
    CHECK(widgetA.blurCount == 0);

    // Push overlay View B -> this suspends View A, recording savedFocus_ = &widgetA
    stack.push(std::make_unique<InstrumentedView>("B", &log), &fm);
    CHECK(stack.top()->savedFocus() == nullptr); // View B has no saved focus yet
    CHECK(log.back() == "B::onPush");

    // Focus shifts to widgetB while overlay View B is active
    fm.setFocus(&widgetB);
    CHECK(widgetB.isFocused());
    CHECK(widgetA.blurCount == 1);

    // Pop overlay View B -> View A resumes and restores focus to widgetA via forceOwner
    stack.pop(&fm);

    // Current focused widget is widgetA again
    CHECK(fm.focused() == &widgetA);

    // forceOwner must NOT have triggered additional onFocus/onBlur callbacks on widgetA during restoreFocus!
    CHECK(widgetA.focusCount == 1);
    CHECK(widgetA.blurCount == 1);
}

TEST_CASE("ViewStack — popTo<T>()") {
    FocusManager fm;
    ViewStack stack;

    stack.push(std::make_unique<ViewTypeA>(), &fm);
    stack.push(std::make_unique<ViewTypeB>(), &fm);
    stack.push(std::make_unique<ViewTypeC>(), &fm);

    CHECK(stack.size() == 3);
    CHECK(stack.top()->isType<ViewTypeC>());

    stack.popTo<ViewTypeA>(&fm);

    CHECK(stack.size() == 1);
    CHECK(stack.top()->isType<ViewTypeA>());
}

TEST_CASE("ViewStack — replace()") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack;

    stack.push(std::make_unique<InstrumentedView>("A", &log), &fm);
    log.clear();

    stack.replace(std::make_unique<InstrumentedView>("B", &log), &fm);

    CHECK(stack.size() == 1);
    CHECK(log == std::vector<std::string>{"A::onPop", "B::onPush"});
}

TEST_CASE("SimpleTransition — update and reset") {
    SimpleTransition t;
    t.kind = TransitionKind::SlideLeft;
    t.duration = 0.5f;

    CHECK(t.progress == 0.0f);
    CHECK(t.isComplete() == false);

    t.update(0.25f);
    CHECK(t.progress == doctest::Approx(0.5f));
    CHECK(t.isComplete() == false);

    t.update(0.3f);
    CHECK(t.progress == 1.0f);
    CHECK(t.isComplete() == true);

    t.reset();
    CHECK(t.progress == 0.0f);
    CHECK(t.isComplete() == false);
}
