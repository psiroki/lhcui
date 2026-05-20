# Handheld UI Toolkit — Design Document

A C++ UI toolkit library for Linux handheld consoles.  
Target hardware: Cortex-A53 + Mali-G31 MP2 class SoCs. GPU-less targets supported.  
Renderer backend: SDL2 (default) or SDL1 (build-time opt-in).

---

## Table of Contents

1. [Goals and Non-Goals](#1-goals-and-non-goals)
2. [Architecture Overview](#2-architecture-overview)
3. [Build Configuration](#3-build-configuration)
4. [Core Types](#4-core-types)
5. [Rendering Abstraction](#5-rendering-abstraction)
6. [Widget System](#6-widget-system)
7. [Focus Management](#7-focus-management)
8. [View System](#8-view-system)
9. [Input System](#9-input-system)
10. [Time and the Update Loop](#10-time-and-the-update-loop)
11. [SDL Helper Layer](#11-sdl-helper-layer)
12. [Widget Catalogue](#12-widget-catalogue)
13. [Implementation Notes](#13-implementation-notes)

---

## 1. Goals and Non-Goals

### Goals

- **Toolkit, not a framework.** The app owns its main loop, window, and SDL context. The
  library provides components the app composes and drives. No `run()` method takes over
  execution.
- **Gamepad-first input model.** No mouse, no touch. All navigation is D-pad + face/shoulder
  buttons.
- **Portable across SDL generations.** A single build-time macro switches the renderer between
  SDL2 (hardware-accelerated) and SDL1 (software surface blit). The widget code is identical in
  both cases.
- **Low overhead.** A Cortex-A53 at ~1 GHz with shared DRAM is the floor. No heap allocation
  inside hot paths. No RTTI. No exceptions.
- **Modern C++.** C++17. `enum class` for all enumerations. `std::string_view` for read-only
  strings. `std::optional` where nullability is semantically meaningful. Smart pointers for
  ownership.

### Non-Goals

- Touch input, mouse input, or keyboard physical-key input.
- A scene graph or retained-mode layout engine. Widgets compute their own geometry from a
  supplied `Rect`.
- Networking, audio playback, or filesystem access — those belong to the application.
- Theme hot-reloading at runtime.

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│  Application                                                    │
│                                                                 │
│  main loop:                                                     │
│    sdl_events → UISystem::onButtonDown / onButtonUp             │
│    frame time → UISystem::update(dt)                            │
│    UISystem::draw(renderer)                                     │
└───────────────────────────┬─────────────────────────────────────┘
                            │ drives
┌───────────────────────────▼─────────────────────────────────────┐
│  UISystem                                                       │
│  ┌──────────────────┐  ┌──────────────────┐                     │
│  │  KeyRepeatDriver │  │  FocusManager    │                     │
│  └──────────────────┘  └──────────────────┘                     │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  ViewStack                                                │  │
│  │  ┌────────────────────────────────────────────────────┐   │  │
│  │  │  View  (base screen — e.g. DirectoryView)          │   │  │
│  │  │  ┌─────────┐  ┌─────────┐  ┌─────────────────┐     │   │  │
│  │  │  │ Widget  │  │ Widget  │  │ Container Widget│     │   │  │
│  │  │  └─────────┘  └─────────┘  └────────┬────────┘     │   │  │
│  │  │                                     │children      │   │  │
│  │  └─────────────────────────────────────┼──────────────┘   │  │
│  │  ┌─────────────────────────────────────▼──────────────┐   │  │
│  │  │  View  (overlay — e.g. ContextMenu, GuideOverlay)  │   │  │
│  │  └────────────────────────────────────────────────────┘   │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                            │ calls
┌───────────────────────────▼─────────────────────────────────────┐
│  IRenderer  (SDL2Renderer | SDL1Renderer)                       │
└─────────────────────────────────────────────────────────────────┘
```

Key relationships:

- `UISystem` is the single object the application talks to. It is **not** a singleton; the
  application instantiates it and may in principle run multiple instances (e.g. a separate
  UI on a second display).
- `ViewStack` owns a stack of `View` objects. Only the topmost view receives input. All views
  are drawn bottom-to-top so that overlays render above base screens.
- `FocusManager` is owned by `UISystem` and passed (by reference) wherever focus arbitration
  is needed.
- `KeyRepeatDriver` synthesizes repeated `ButtonDown` events in `update()` and injects them
  into the normal event path before delivering them to the view stack.
- `IRenderer` is the only SDL-touching interface; all widget drawing is expressed in terms of
  this interface.

---

## 3. Build Configuration

```cpp
// Enable SDL1 back-end (default: SDL2)
// Set via -DHUI_USE_SDL1 in the build system.
#ifdef HUI_USE_SDL1
  // links against SDL 1.2 and SDL_ttf 1.2
#else
  // links against SDL2 and SDL2_ttf  (default)
#endif
```

All SDL-specific includes are confined to `renderer/SDL2Renderer.cpp` and
`renderer/SDL1Renderer.cpp`. Widget and view code never includes an SDL header directly.

Font loading is similarly wrapped: `FontHandle` is an opaque `uint32_t` that the renderer
maps internally to `TTF_Font*`.

We use CMake to build. The build is split into a widget library and an example application.
The example application demonstrates how to use the widget library and serves as a test
harness for the widgets. The example application can be controlled with both the standard PC
keys or a game controller (using the included optional connector functionality).

---

## 4. Core Types

All types live in the `hui` namespace.

### 4.1 Geometry

```cpp
struct Point { int x, y; };
struct Size  { int w, h; };
struct Rect  { int x, y, w, h; };

// Convenience
Rect rectFromPoints(Point topLeft, Point bottomRight);
bool rectContains(Rect r, Point p);
Rect rectInset(Rect r, int dx, int dy);
```

### 4.2 Color

```cpp
struct Color {
    uint8_t r, g, b, a;

    static constexpr Color white()       { return {255,255,255,255}; }
    static constexpr Color black()       { return {  0,  0,  0,255}; }
    static constexpr Color transparent() { return {  0,  0,  0,  0}; }

    Color withAlpha(uint8_t a) const;
    Color lerp(Color other, float t) const;
};
```

### 4.3 Theme

```cpp
struct Theme {
    Color background;
    Color surface;         // card / list item background
    Color surfaceAlt;      // alternate row tint
    Color accent;          // focus highlight, playing indicator
    Color textPrimary;
    Color textSecondary;
    Color textDisabled;
    Color warning;         // destructive actions
    Color success;
    Color overlay;         // semi-transparent modal background

    // Focus visuals (see widget guide §1.1)
    Color focusBorderColor;
    int   focusBorderWidth;  // px, typically 2
    Color focusFillColor;    // accent at ~15% alpha

    // Typography
    FontHandle fontBody;     // primary list text
    FontHandle fontSmall;    // secondary / meta text
    FontHandle fontMono;     // optional, for paths and codes
    int fontBodySize;
    int fontSmallSize;
};
```

`Theme` is passed into `UISystem` at construction and propagated read-only to all widgets.
It is not hot-reloadable; a rebuild is required to change the compiled-in default.

### 4.4 Button Enum

```cpp
enum class Button : uint8_t {
    Up,
    Down,
    Left,
    Right,
    A,
    B,
    X,
    Y,
    L1,
    L2,
    R1,
    R2,
    Start,
    Select,
    Guide,

    COUNT      // sentinel — do not use as a value
};
```

The library does not define which physical button is which. That mapping is the
application's responsibility (see §11, SDL Helper Layer).

### 4.5 Button Events

```cpp
enum class ButtonEventKind { Down, Up };

struct ButtonEvent {
    Button          button;
    ButtonEventKind kind;
    bool            synthetic; // true if generated by KeyRepeatDriver
};
```

`synthetic` lets widgets (or debug overlays) distinguish real hardware events from
key-repeat injections, should that be needed.

---

## 5. Rendering Abstraction

### 5.1 IRenderer Interface

```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // --- Frame lifecycle ---
    virtual void beginFrame() = 0;
    virtual void endFrame()   = 0;   // present / flip

    // --- Clipping ---
    // Clips are additive intersections; push/pop form a stack.
    virtual void pushClip(Rect r)  = 0;
    virtual void popClip()         = 0;

    // --- Primitives ---
    virtual void fillRect(Rect r, Color c)                     = 0;
    virtual void drawRect(Rect r, Color c, int thickness = 1)  = 0;
    virtual void drawLine(Point a, Point b, Color c)           = 0;

    // --- Text ---
    // Returns rendered advance width in pixels.
    virtual int  drawText(std::string_view text,
                          Point origin,
                          FontHandle font,
                          Color color)                         = 0;
    virtual Size measureText(std::string_view text,
                             FontHandle font)                  = 0;

    // Draws text clipped to maxWidth pixels with "…" suffix if it overflows.
    virtual void drawTextEllipsis(std::string_view text,
                                  Point origin,
                                  FontHandle font,
                                  Color color,
                                  int maxWidth)                = 0;

    // --- Images / Textures ---
    virtual TextureHandle loadTexture(std::string_view path)   = 0;
    virtual void          freeTexture(TextureHandle h)         = 0;
    virtual Size          textureSize(TextureHandle h)         = 0;
    virtual void          drawTexture(TextureHandle h,
                                      Rect dst,
                                      uint8_t alpha = 255)     = 0;

    // --- Alpha modulation ---
    // Sets a global multiplier applied to all subsequent draw calls.
    // Used to dim views behind overlays.
    virtual void setGlobalAlpha(uint8_t alpha) = 0;

    // --- Querying ---
    virtual Size screenSize() const = 0;
};
```

`FontHandle` and `TextureHandle` are both `uint32_t` typedefs. `0` is the null/invalid
handle. The renderer maps handles to internal SDL structures.

### 5.2 SDL2 vs SDL1 Considerations

The SDL2 backend uses `SDL_Renderer` with `SDL_TEXTUREACCESS_STREAMING` for dynamic
textures (album art) and `SDL_TEXTUREACCESS_STATIC` for pre-loaded assets.

The SDL1 backend uses `SDL_Surface` with `SDL_BlitSurface`. All drawing primitives are
software-rendered. The clip stack is emulated via `SDL_SetClipRect`. Hardware-accelerated
alpha blending is not available; `setGlobalAlpha` is approximated by pre-multiplying a
surface copy at the cost of one extra blit per dimmed layer. This is acceptable for the
infrequent overlay-open transitions, but avoid calling it every frame on SDL1 targets.

---

## 6. Widget System

### 6.1 Widget Base Class

```cpp
class Widget {
public:
    virtual ~Widget() = default;

    // Called once per frame. dt = elapsed seconds since last call.
    virtual void update(float dt) {}

    // Called once per frame after update(). r is the rect this widget occupies.
    virtual void draw(IRenderer& renderer, Rect r, const Theme& theme) = 0;

    // Event dispatch. Returns true if the event was consumed (stops propagation).
    virtual bool onButtonDown(Button b) { return false; }
    virtual bool onButtonUp(Button b)   { return false; }

    // Focus lifecycle. The widget must retain its internal focus state across
    // onBlur/onFocus cycles — it must not reset its selected index on blur.
    virtual void onFocus() {}
    virtual void onBlur()  {}

    bool isFocused()  const { return focused_; }
    bool isDisabled() const { return disabled_; }

    void setDisabled(bool d) { disabled_ = d; }

private:
    bool focused_  = false;
    bool disabled_ = false;

    friend class FocusManager;
    void setFocused(bool f) { focused_ = f; if (f) onFocus(); else onBlur(); }
};
```

Widgets are **immediate-mode at the draw level**: they receive their bounding `Rect` on
every `draw()` call. There is no retained layout tree. The application (or a container
widget) decides where each widget sits. This is appropriate for fixed-resolution handheld
UIs where layouts do not reflow dynamically.

### 6.2 Container Widgets

Container widgets own child widgets and are responsible for partitioning their own `Rect`
into children's rects. Example pattern used by `ListView`:

```cpp
class ListView : public Widget {
public:
    void draw(IRenderer& r, Rect bounds, const Theme& theme) override {
        int y = bounds.y - scrollOffset_;
        for (int i = 0; i < items_.size(); ++i) {
            Rect itemRect = {bounds.x, y, bounds.w, itemHeight_};
            if (i == focusedIndex_)
                items_[i].focused = true;
            r.pushClip(bounds);
            items_[i].draw(r, itemRect, theme);
            r.popClip();
            y += itemHeight_;
        }
        drawScrollIndicator(r, bounds, theme);
    }
    // ...
};
```

### 6.3 Dirty / Visibility

The library does not implement a dirty-rect system. All visible views redraw every frame.
On a Mali-G31 class GPU, clearing and redrawing a 480×320 or 640×480 frame at 60 fps is
well within budget. On GPU-less SDL1 targets with software rendering, the application can
opt to reduce the frame rate (30 fps or 20 fps) by simply calling `update` and `draw` less
frequently — the toolkit has no internal timer.

### 6.4 Animations

Animated properties (toast fade-in, guide overlay slide-in, now-playing pulse) are driven
by accumulating `dt` in the widget's state. Widgets that animate must implement `update(float dt)`.
No tween library is required; simple linear or clamped-lerp arithmetic is sufficient.

```cpp
// Typical animation pattern inside a widget's update():
slideOffset_ = std::max(0.0f, slideOffset_ - dt * SLIDE_SPEED);
```

For SDL1 targets that cannot afford per-frame blits at full speed, animations degrade
gracefully: the guide overlay appears immediately (slideOffset_ = 0) if the elapsed time
passed to the first `update()` call is already larger than the animation duration.

---

## 7. Focus Management

### 7.1 FocusManager

```cpp
class FocusManager {
public:
    // Give focus to a specific widget. Calls onBlur on previous owner,
    // onFocus on the new one.
    void setFocus(Widget* w);

    Widget* focused() const { return current_; }

    // Convenience: does current_ == w ?
    bool hasFocus(const Widget* w) const;

    // Called by UISystem when a View is pushed/popped so the new top
    // view can restore its saved focus without a prior setFocus.
    void forceOwner(Widget* w);

private:
    Widget* current_ = nullptr;
};
```

Focus is always held by exactly one widget. When an overlay is pushed onto the ViewStack
it calls `focusManager.setFocus(firstFocusableWidget)`, saving nothing — the previous
view's widget retains `focused_ = false` but remembers its internal focus index. When the
overlay is popped, the ViewStack restores focus to the underlying view's last known widget.

### 7.2 Focus Memory

Each `View` owns a `Widget*` field `savedFocus_`. When the view is about to be obscured
(overlay pushed on top), `ViewStack` calls `view.suspendFocus()`, which records
`savedFocus_ = focusManager.focused()`. When the view comes back to the top,
`ViewStack` calls `view.restoreFocus()`, which calls `focusManager.setFocus(savedFocus_)`.

Container widgets with internal focus indices (ListView, GridView) must never reset
those indices when they receive `onBlur()`. They reset only when explicitly instructed via
a `resetFocus()` method (e.g. when the Tab Bar switches tabs, the new tab's list resets).

### 7.3 Focus Traversal

Focus traversal is not automatic. Each view and container widget handles D-pad events and
calls `focusManager.setFocus()` on the appropriate child. This is intentional: on a
gamepad UI there is no universal DOM-style tab order — a ListView handles Up/Down
internally, while a dialog routes Left/Right between Cancel and Confirm. Imposing a
generic traversal algorithm would fight every layout.

---

## 8. View System

### 8.1 View Base Class

```cpp
class View {
public:
    virtual ~View() = default;

    // Lifecycle
    virtual void onPush()    {}  // called when this view becomes the new top
    virtual void onPop()     {}  // called just before this view is destroyed
    virtual void onResume()  {}  // called when an overlay above it is popped
    virtual void onSuspend() {}  // called when an overlay is pushed on top

    // Per-frame
    virtual void update(float dt, FocusManager& fm) {}
    virtual void draw(IRenderer& r, const Theme& theme) = 0;

    // Input — return true if consumed
    virtual bool onButtonDown(Button b, FocusManager& fm) { return false; }
    virtual bool onButtonUp  (Button b, FocusManager& fm) { return false; }

    // Called by ViewStack to dim this view when an overlay is above it.
    // Views set this via their draw() implementation (check dimmed_ flag).
    void setDimmed(bool d) { dimmed_ = d; }
    bool isDimmed() const  { return dimmed_; }

    // Hint bar data — views publish what the hint bar should show.
    // The Shell widget reads this from the top-most view each frame.
    virtual std::vector<HintEntry> currentHints() const { return {}; }

protected:
    bool dimmed_ = false;
};
```

The application subclasses `View` to implement each screen (DirectoryView, LibraryView,
NowPlayingView, etc.). The view owns all its widgets as member variables or via
`std::unique_ptr`.

### 8.2 ViewStack

```cpp
class ViewStack {
public:
    // Push a new view. Previous top gets onSuspend(). New view gets onPush().
    // Takes ownership.
    void push(std::unique_ptr<View> view);

    // Pop the top view. It gets onPop(). The view below gets onResume().
    // No-op if stack has only one entry.
    void pop();

    // Pop until a view of the given type is at the top.
    // Used for "go home" type navigation.
    template<typename T>
    void popTo();

    // Replace the top view (pop + push as an atomic operation).
    void replace(std::unique_ptr<View> view);

    View* top() const;
    bool  empty() const;

    // Drive all visible views.
    void update(float dt, FocusManager& fm);

    // Draw all views bottom-to-top.
    // Applies dimming (setGlobalAlpha) to non-top views when an overlay is open.
    void draw(IRenderer& renderer, const Theme& theme);

    // Deliver event to top view only.
    bool dispatchButtonDown(Button b, FocusManager& fm);
    bool dispatchButtonUp  (Button b, FocusManager& fm);

private:
    std::vector<std::unique_ptr<View>> stack_;
};
```

**Drawing all views bottom-to-top** (rather than only the top) is necessary because
overlay views (context menus, guide panel) need to render over the base screen, which
must be visible behind them. A view that should be invisible when not on top simply
should not be on the stack at all (it was popped).

**Alpha dimming** during overlay display: when the stack has more than one entry,
`ViewStack::draw()` renders all views except the topmost with `setGlobalAlpha(128)` (or a
configurable value). After drawing non-top views it restores `setGlobalAlpha(255)` before
drawing the top view.

### 8.3 Overlay Views

Overlays are ordinary `View` subclasses that happen to be pushed on top of a base screen.
They are visually partial (they do not fill the whole screen) and render a dimmed
background themselves via `fillRect(fullScreen, theme.overlay)`. The ViewStack draws
them last (on top) at full alpha.

Overlay views that should slide in (e.g. GuideOverlay) animate their own bounding rect
in `update()`.

### 8.4 Screen Transitions

Transitions are intentionally absent from the base library's ViewStack. On low-end
hardware, cross-fading two full frames doubles rendering work. If the application wants
transitions:

1. It subclasses `ViewStack` (or wraps it) and overrides `draw()` to implement a
   transition effect using a transition timer fed from `update()`.
2. Simple options that are cheap on SDL1: instant cut (default), horizontal slide,
   or a fade driven by `setGlobalAlpha`.

The library provides `TransitionKind { None, SlideLeft, SlideRight, FadeThrough }` as a
convenience enum and a `SimpleTransition` helper, but the default ViewStack does not use
it — the application opts in.

---

## 9. Input System

### 9.1 Application Entry Points

The application calls these two methods on `UISystem`:

```cpp
void UISystem::onButtonDown(Button b);
void UISystem::onButtonUp(Button b);
```

These are the only input entry points. The `UISystem` passes the event to
`KeyRepeatDriver`, then delivers it to `ViewStack`.

### 9.2 KeyRepeatDriver

Key-repeat is implemented entirely in the input layer, not per-widget. This matches the
widget guide's requirement (§1.2): *"Implement this at the app input layer, not per-widget."*

```cpp
class KeyRepeatDriver {
public:
    // Timing constants (seconds)
    static constexpr float kInitialDelay    = 0.300f;
    static constexpr float kRepeatInterval  = 0.100f;
    static constexpr float kFastInterval    = 0.030f;
    static constexpr float kFastThreshold   = 1.000f;  // after 1s, use fast interval

    // Called by UISystem on real hardware events.
    void onButtonDown(Button b);
    void onButtonUp  (Button b);

    // Called by UISystem::update(). Calls sink for each synthetic repeat event.
    // Sink signature: void(ButtonEvent)
    template<typename Sink>
    void update(float dt, Sink&& sink);

private:
    struct HeldButton {
        Button  button;
        float   heldFor        = 0.0f;
        float   timeSinceRepeat= 0.0f;
        bool    repeatStarted  = false;
    };

    // Only directional and shoulder buttons repeat; face buttons do not.
    static bool shouldRepeat(Button b);

    std::array<std::optional<HeldButton>,
               static_cast<size_t>(Button::COUNT)> held_;
};
```

`shouldRepeat()` returns true for `Up`, `Down`, `Left`, `Right`, `L1`, `L2`, `R1`, `R2`.
Face buttons (`A`, `B`, `X`, `Y`), `Start`, `Select`, `Guide` do not repeat.

Synthetic events set `ButtonEvent::synthetic = true`.

### 9.3 Event Routing

```
UISystem::onButtonDown(b)
  → KeyRepeatDriver::onButtonDown(b)   [records hold state]
  → ViewStack::dispatchButtonDown(b)   [delivers to top View]
      → View::onButtonDown(b)
          → widget1.onButtonDown(b) → consumed? stop
          → widget2.onButtonDown(b) → consumed? stop
          → ...
          → return false (unhandled — UISystem may have a global fallback)

UISystem::update(dt)
  → KeyRepeatDriver::update(dt, [&](ButtonEvent e) {
        ViewStack::dispatchButtonDown(e.button)
    })
  → ViewStack::update(dt)
```

The event routing is explicit, not automatic. Each `View` subclass decides which widgets
see events and in what order. A simple view might route all events to the single focused
widget; a complex view like NowPlayingView handles some buttons itself (playback
controls) and routes others to the focused child (queue list).

---

## 10. Time and the Update Loop

```cpp
class UISystem {
public:
    UISystem(IRenderer& renderer, const Theme& theme);

    // Called by the application once per frame.
    // elapsedSeconds: wall-clock time since the previous call.
    // Drives: KeyRepeatDriver, ViewStack::update, animation timers.
    void update(float elapsedSeconds);

    // Called by the application once per frame, after update().
    void draw();

    // Input — called by the application from its SDL event loop.
    void onButtonDown(Button b);
    void onButtonUp  (Button b);

    ViewStack&    viewStack();
    FocusManager& focusManager();

private:
    IRenderer&      renderer_;
    const Theme&    theme_;
    ViewStack       viewStack_;
    FocusManager    focusManager_;
    KeyRepeatDriver keyRepeat_;
};
```

The application's typical main loop:

```cpp
uint64_t last = SDL_GetTicks64();    // or SDL_GetTicks() on SDL1

while (running) {
    // -- Event phase --
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) running = false;

        auto opt = sdlHelper.translate(e);     // see §11
        if (opt) {
            if (opt->kind == ButtonEventKind::Down)
                uiSystem.onButtonDown(opt->button);
            else
                uiSystem.onButtonUp(opt->button);
        }
    }

    // -- Update phase --
    uint64_t now = SDL_GetTicks64();
    float dt = (now - last) / 1000.0f;
    last = now;
    uiSystem.update(dt);

    // -- Draw phase --
    renderer.beginFrame();
    uiSystem.draw();
    renderer.endFrame();
}
```

The library imposes no frame rate cap and has no internal timer thread. Frame pacing is
entirely the application's responsibility.

---

## 11. SDL Helper Layer

The library does not know which SDL_Joystick axis or button corresponds to which
`hui::Button`. That mapping is hardware- and OS-dependent. The library provides helpers
to make the common case easy without mandating any particular mapping. An
`SDL_GameController` connector is provided and can be used if available (depends on
the platform), and a standard keyboard connector is provided as well that can be used
to ease testing on a PC.

### 11.1 ButtonMapping (Data-Driven)

```cpp
// Maps SDL gamepad/joystick button indices to hui::Button values.
// Index = SDL button index; value = hui::Button (or nullopt to ignore).
struct ButtonMapping {
    std::array<std::optional<Button>,
               SDL_CONTROLLER_BUTTON_MAX> controllerButtons;

    // Axis-to-button thresholds for SDL_CONTROLLER_AXIS_* axes.
    // Positive axis crossing +threshold fires the positive button,
    // negative crossing -threshold fires the negative button.
    struct AxisBinding {
        SDL_GameControllerAxis axis;
        Button                 positiveButton;
        Button                 negativeButton;
        int16_t                threshold = 16384;  // ~50% of INT16_MAX
    };
    std::vector<AxisBinding> axisBindings;
};
```

A default mapping matching the most common cheap-handheld layout is provided:

```cpp
ButtonMapping ButtonMapping::defaultXboxLayout();
ButtonMapping ButtonMapping::defaultNintendoLayout();  // swapped A/B
```

Applications with unusual hardware create a `ButtonMapping` from a config file:

```cpp
// Pseudocode — application-side
ButtonMapping mapping;
for (auto& entry : config["buttons"]) {
    int sdlIdx        = entry["sdl_index"].as<int>();
    hui::Button huiBtn = hui::buttonFromName(entry["hui_button"].as<std::string>());
    mapping.controllerButtons[sdlIdx] = huiBtn;
}
```

### 11.2 SDLGamepadHelper

```cpp
class SDLGamepadHelper {
public:
    explicit SDLGamepadHelper(ButtonMapping mapping);

    // Opens the first available SDL_GameController.
    // Returns false if none found.
    bool openController(int deviceIndex = 0);
    void closeController();

    // Translates an SDL_Event to a ButtonEvent.
    // Returns std::nullopt for events that do not map to any Button.
    std::optional<ButtonEvent> translate(const SDL_Event& e) const;

private:
    ButtonMapping           mapping_;
    SDL_GameController*     controller_ = nullptr;

    // Per-axis state for threshold hysteresis.
    std::array<bool, SDL_CONTROLLER_AXIS_MAX> axisActive_{};
};
```

`translate()` handles `SDL_CONTROLLERBUTTONDOWN`, `SDL_CONTROLLERBUTTONUP`,
`SDL_CONTROLLERAXISMOTION`, and optionally `SDL_KEYDOWN` / `SDL_KEYUP` (for desktop
testing with a keyboard, where arrow keys map to D-pad, Z/X to A/B, etc.).

The keyboard fallback mapping is compiled in under `#ifdef HUI_ENABLE_KEYBOARD_FALLBACK`
and defaults to on when `NDEBUG` is not defined, giving developers a way to work without
physical hardware.

---

## 12. Widget Catalogue

The following widgets are defined in the library to support the music player application
(as specified in `gamepad-widget-guide.md`). Each is a concrete `Widget` subclass unless
noted. Views are `View` subclasses.

### Level 0 — Input Layer

| Component | Type | Notes |
|-----------|------|-------|
| `KeyRepeatDriver` | internal | Owned by UISystem; synthesizes repeat events |
| `FocusManager` | internal | Owned by UISystem; passed by ref where needed |

### Level 1 — Atoms

| Widget | Description |
|--------|-------------|
| `ListItemWidget` | Single row: icon, primary label, secondary label, right meta. Variants: default, track, folder, playlist. Handles focused/playing/disabled visual states. |
| `GridCellWidget` | Single grid tile: thumbnail (or gradient placeholder), label, sublabel. Focused border + playing badge. |
| `ProgressBar` | Read-only horizontal fill bar with elapsed/total timestamps. |
| `Slider` | Focusable horizontal value control driven by Left/Right. |
| `SortModeIndicator` | Non-focusable badge showing current sort mode. |
| `ShuffleToggle` | Non-focusable icon with on/off visual state. |
| `RepeatModeToggle` | Non-focusable icon cycling off → all → one. |

### Level 2 — Molecules

| Widget | Description |
|--------|-------------|
| `ListHeaderWidget` | Non-focusable context row: icon, label (left-truncated for paths), item count, sort badge. |
| `SeekableProgressBar` | Extends `ProgressBar`; consumes L2/R2 button events and calls `onSeek`. |
| `PlaybackControlsRow` | Non-focusable visual row of transport icons reflecting playback state. |
| `HintBarWidget` | Non-focusable bar at screen bottom. Reads `currentHints()` from the active View each frame. Renders button glyphs with per-button color coding. |
| `StatusBarWidget` | Non-focusable top bar: view mode, context label, now-playing pulse indicator, clock, battery. |
| `ToastNotification` | Non-focusable. Self-timed (driven by `update()`). Auto-dismissed; replacement policy (not stacked). |

### Level 3 — Organisms

| Widget / View | Description |
|---------------|-------------|
| `ListView` | Scrollable vertical list. Internal D-pad routing, key-repeat for Up/Down, page jump via L1/R1, first/last via L2/R2, wrap-around, scroll-to-focus, focus memory via `getFocusIndex()`, empty state, scroll indicator. |
| `GridView` | 2D focusable grid. Same scroll/wrap/page/memory rules as ListView. |
| `TabBarWidget` | Horizontal tab strip. L1/R1 to switch. Not reachable via D-pad Up/Down. |
| `QueueList` | Extends ListView with grab-mode reorder (Y to grab, Up/Down to reorder, A to drop, B to cancel). |
| `LetterWheel` | Character strip + results list. Internal focus routing between strip and list. |
| `OnScreenKeyboard` | 2D QWERTY grid. Confirm/Cancel/Backspace keys. |
| `ContextMenuView` | Overlay View. Semi-transparent background fill. Action list with destructive color support. |
| `ConfirmationDialogView` | Overlay View. Default focus on Cancel. L/R to switch, A to confirm, B to cancel. |
| `TrackInfoPanelView` | Overlay View (or non-focusable modal). Read-only metadata table. |
| `GuideOverlayView` | Overlay View sliding in from right. Contains Volume/Brightness sliders and action items. |

### Level 4 — Screens

These are application-level `View` subclasses. The library provides base classes and
shared infrastructure; the application composes them.

| View | Widgets Used |
|------|-------------|
| `Shell` | Permanent wrapper: StatusBarWidget, HintBarWidget, optional TabBarWidget. Owns the content area rect passed down to page views. |
| `DirectoryView` | ListView, ListHeaderWidget, pushes ContextMenuView / TrackInfoPanelView / ToastNotification |
| `LibraryView` | ListView + GridView + TabBarWidget, pushes ContextMenuView / LetterWheel / TrackInfoPanelView |
| `NowPlayingView` | SeekableProgressBar, PlaybackControlsRow, QueueList, pushes ContextMenuView / TrackInfoPanelView |

---

## 13. Implementation Notes

### 13.1 Text Layout

`IRenderer::drawTextEllipsis` must handle UTF-8 correctly — grapheme clusters, not raw
bytes. On SDL_ttf, measure progressively shorter substrings until the text + "…" fits.
For performance, cache the last-measured string and width; list items with the same label
will not remeasure.

Left-truncation (for filesystem paths) is a separate helper:
```cpp
std::string leftTruncate(std::string_view text, FontHandle font,
                         int maxWidth, IRenderer& r);
// Returns "…/rest/of/path" where "rest/of/path" fits within maxWidth.
```

### 13.2 Gradient Placeholder Thumbnails

`GridCellWidget` must show a colored placeholder when no thumbnail is available. Hash the
item label to a hue, generate a simple two-stop vertical gradient. On SDL1, pre-blit the
gradient onto a surface once and cache it keyed by label hash.

```cpp
Color hueToColor(float hue);  // HSV with S=0.5, V=0.7
uint32_t labelHash(std::string_view label);
```

### 13.3 Scroll-to-Focus

The scroll offset formula targeting the "bottom third / top third" of the viewport
(widget guide §1.5):

```cpp
// When moving focus downward:
int targetScrollDown = (focusedItemTop + itemHeight_)
                       - bounds.h * 2 / 3;

// When moving focus upward:
int targetScrollUp = focusedItemTop - bounds.h * 1 / 3;

scrollOffset_ = std::clamp(
    movingDown ? targetScrollDown : targetScrollUp,
    0,
    totalContentHeight_ - bounds.h);
```

### 13.4 Hint Bar Ownership

The `HintBarWidget` is owned by the `Shell` and reads hint data from the active view
via `ViewStack::top()->currentHints()` on every `draw()` call. Views return a
`std::vector<HintEntry>` reflecting their current state — the hints change automatically
when an overlay is pushed (the overlay's hints replace the base view's hints because it
is now the top of the stack).

```cpp
struct HintEntry {
    std::string buttonLabel;  // "A", "B", "L1/R1", "START", etc.
    std::string actionLabel;  // "Play", "Back", "Options"
    bool        hold = false;
    int         sortOrder;    // enforces display ordering (see widget guide §2.2)
};
```

### 13.5 Button Color Coding

The HintBarWidget renders button glyphs in the colors mandated by the widget guide:

```cpp
Color buttonGlyphColor(std::string_view buttonLabel, const Theme& theme) {
    if (buttonLabel == "A")      return Color{220,  50,  50, 255};  // red
    if (buttonLabel == "B")      return Color{220, 160,  40, 255};  // yellow/orange
    if (buttonLabel == "X")      return Color{ 60, 120, 220, 255};  // blue
    if (buttonLabel == "Y")      return Color{ 60, 180,  80, 255};  // green
    return theme.textSecondary;                                       // neutral
}
```

### 13.6 Performance Budget (A53 + SDL1 Worst Case)

Targeting 30 fps on a GPU-less device running SDL1 software rendering at 480×320:

- **Full frame blit** (~74 KB at 16 bpp): ~1.5 ms on A53 with NEON memcpy.
- **Text rendering**: SDL_ttf renders glyphs to surfaces. Cache glyph surfaces; do not
  re-render the same string at the same font size more than once per unique string per
  frame.
- **Overlay dimming**: one full-screen 50% alpha blit. On SDL1 this requires either a
  per-pixel alpha loop or a pre-filled 50%-grey surface with `SDL_SetAlpha`. The latter
  is faster (single `SDL_BlitSurface` call with alpha).
- **GuideOverlay slide animation**: on SDL1, skip the animation (instant open) unless the
  device is known to be fast enough. Expose `UISystem::setAnimationsEnabled(bool)`.

### 13.7 Memory Ownership Summary

| Thing | Owner |
|-------|-------|
| `IRenderer` | Application |
| `Theme` | Application (passed by const ref) |
| `UISystem` | Application |
| `ViewStack` | `UISystem` |
| `FocusManager` | `UISystem` |
| Views on the stack | `ViewStack` (via `unique_ptr`) |
| Widgets inside a View | The `View` (member variables or `unique_ptr`) |
| Textures / FontHandles | `IRenderer`; callers get opaque handles |

### 13.8 No Exceptions, No RTTI

All methods that can fail return a bool or `std::optional`. `dynamic_cast` is not used;
views and widgets use the visitor pattern or explicit type tags where runtime type
discrimination is unavoidable.

---

*End of design document.*
