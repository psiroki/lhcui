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
// Mock IRenderer for testing draw() and scrim fills.
// ---------------------------------------------------------------------------
class MockRenderer : public IRenderer {
public:
    uint8_t lastAlpha = 255;
    std::vector<uint8_t> alphaHistory;
    int fillRectCount = 0;

    void beginFrame() override {}
    void endFrame() override {}
    void pushClip(Rect) override {}
    void popClip() override {}
    void fillRect(Rect, Color) override { ++fillRectCount; }
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
    bool focusable = true;

    bool isFocusable() const override { return focusable; }

    void onFocus() override { ++focusCount; }
    void onBlur()  override { ++blurCount;  }
    void draw(IRenderer&, const Theme&) override {}
};

// ---------------------------------------------------------------------------
// Instrumented View subclass for tracing call order.
// ---------------------------------------------------------------------------
class InstrumentedView : public View {
public:
    HUI_VIEW_TYPE(InstrumentedView)

    std::string name;
    std::vector<std::string>* eventLog;
    bool handleButton = false;
    bool buttonReceived = false;
    bool modal = false;

    InstrumentedView(std::string name, std::vector<std::string>* log)
        : name(std::move(name)), eventLog(log) {}

    bool dimsBelow() const override { return modal; }

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

    void draw(IRenderer&, const Theme&) override {}

    bool onButtonDown(Button, FocusManager&) override {
        buttonReceived = true;
        return handleButton;
    }
};

class SelfPoppingView : public View {
public:
    ViewStack* stackPtr = nullptr;
    bool onButtonDownExecuted = false;

    explicit SelfPoppingView(ViewStack* stack) : stackPtr(stack) {}

    bool onButtonDown(Button b, FocusManager&) override {
        (void)b;
        if (stackPtr) {
            stackPtr->pop();
        }
        onButtonDownExecuted = true;
        return true;
    }

    void draw(IRenderer&, const Theme&) override {}
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
// Tests covering Phase 5 QA sign-off checklist & deferred mutations
// ---------------------------------------------------------------------------

TEST_CASE("ViewStack — push call order (A onSuspend before B onPush)") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack(&fm);

    stack.push(std::make_unique<InstrumentedView>("A", &log));
    stack.applyPendingMutations(fm);
    CHECK(log == std::vector<std::string>{"A::onPush"});

    log.clear();
    stack.push(std::make_unique<InstrumentedView>("B", &log));
    stack.applyPendingMutations(fm);
    CHECK(log == std::vector<std::string>{"A::onSuspend", "B::onPush"});
}

TEST_CASE("ViewStack — pop call order (B onPop before A onResume)") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack(&fm);

    stack.push(std::make_unique<InstrumentedView>("A", &log));
    stack.push(std::make_unique<InstrumentedView>("B", &log));
    stack.applyPendingMutations(fm);

    log.clear();
    stack.pop();
    stack.applyPendingMutations(fm);
    CHECK(log == std::vector<std::string>{"B::onPop", "A::onResume"});
}

TEST_CASE("ViewStack — pop on single-entry stack is no-op") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack(&fm);

    stack.push(std::make_unique<InstrumentedView>("A", &log));
    stack.applyPendingMutations(fm);
    CHECK(stack.size() == 1);

    log.clear();
    stack.pop();
    stack.applyPendingMutations(fm);
    CHECK(stack.size() == 1);
    CHECK(stack.top() != nullptr);
    CHECK(log.empty());
}

TEST_CASE("ViewStack — dispatchButtonDown delivers event ONLY to top view") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack(&fm);

    auto viewA = std::make_unique<InstrumentedView>("A", &log);
    auto viewB = std::make_unique<InstrumentedView>("B", &log);

    InstrumentedView* ptrA = viewA.get();
    InstrumentedView* ptrB = viewB.get();

    stack.push(std::move(viewA));
    stack.push(std::move(viewB));
    stack.applyPendingMutations(fm);

    bool consumed = stack.dispatchButtonDown(Button::A, fm);
    CHECK(consumed == false);
    CHECK(ptrB->buttonReceived == true);
    CHECK(ptrA->buttonReceived == false);
}

TEST_CASE("ViewStack — scrim dimming via dimsBelow()") {
    FocusManager fm;
    ViewStack stack(&fm);

    std::vector<std::string> log;
    auto viewA = std::make_unique<InstrumentedView>("Base", &log);
    auto viewB = std::make_unique<InstrumentedView>("Modal", &log);
    viewB->modal = true;

    stack.push(std::move(viewA));
    stack.push(std::move(viewB));
    stack.applyPendingMutations(fm);

    MockRenderer renderer;
    Theme theme{};

    stack.draw(renderer, theme);

    // Exactly one full-screen fill (theme.overlay) issued before modal view
    CHECK(renderer.fillRectCount == 1);

    // ViewStack makes NO setGlobalAlpha calls
    CHECK(renderer.alphaHistory.empty());
}

TEST_CASE("ViewStack — deferred pop safety (self-popping overlay)") {
    FocusManager fm;
    ViewStack stack(&fm);
    std::vector<std::string> log;

    stack.push(std::make_unique<InstrumentedView>("Base", &log));
    stack.push(std::make_unique<SelfPoppingView>(&stack));
    stack.applyPendingMutations(fm);
    CHECK(stack.depth() == 2);

    // Dispatch button down. SelfPoppingView enqueues pop() from inside onButtonDown.
    // Must NOT crash or use-after-free.
    bool consumed = stack.dispatchButtonDown(Button::A, fm);
    CHECK(consumed == true);

    // After dispatchButtonDown unwinds and applies pending mutations, stack depth is 1
    CHECK(stack.depth() == 1);
    CHECK(stack.top()->isType<InstrumentedView>());
}

TEST_CASE("View — suspendFocus and restoreFocus via forceOwner") {
    FocusManager fm;
    TestWidget widgetA;
    TestWidget widgetB;

    ViewStack stack(&fm);
    std::vector<std::string> log;

    stack.push(std::make_unique<InstrumentedView>("A", &log));
    stack.applyPendingMutations(fm);

    fm.setFocus(&widgetA);
    CHECK(widgetA.isFocused() == true);
    CHECK(widgetA.focusCount == 1);
    CHECK(widgetA.blurCount == 0);

    // Push overlay View B -> suspends View A, clearing focus so widgetA gets onBlur
    stack.push(std::make_unique<InstrumentedView>("B", &log));
    stack.applyPendingMutations(fm);

    CHECK(widgetA.isFocused() == false);
    CHECK(widgetA.blurCount == 1);

    fm.setFocus(&widgetB);
    CHECK(widgetB.isFocused() == true);

    // Pop overlay View B -> restores focus to widgetA via forceOwner without callbacks
    stack.pop();
    stack.applyPendingMutations(fm);

    CHECK(fm.focused() == &widgetA);
    CHECK(widgetA.isFocused() == true);
    CHECK(widgetA.focusCount == 1);
    CHECK(widgetA.blurCount == 1);
}

TEST_CASE("ViewStack — popTo<T>()") {
    FocusManager fm;
    ViewStack stack(&fm);

    stack.push(std::make_unique<ViewTypeA>());
    stack.push(std::make_unique<ViewTypeB>());
    stack.push(std::make_unique<ViewTypeC>());
    stack.applyPendingMutations(fm);

    CHECK(stack.size() == 3);
    CHECK(stack.top()->isType<ViewTypeC>());

    stack.popTo<ViewTypeA>();
    stack.applyPendingMutations(fm);

    CHECK(stack.size() == 1);
    CHECK(stack.top()->isType<ViewTypeA>());
}

TEST_CASE("ViewStack — replace()") {
    std::vector<std::string> log;
    FocusManager fm;
    ViewStack stack(&fm);

    stack.push(std::make_unique<InstrumentedView>("A", &log));
    stack.applyPendingMutations(fm);
    log.clear();

    stack.replace(std::make_unique<InstrumentedView>("B", &log));
    stack.applyPendingMutations(fm);

    CHECK(stack.size() == 1);
    CHECK(log == std::vector<std::string>{"A::onPop", "B::onPush"});
}
