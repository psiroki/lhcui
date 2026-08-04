# Handheld UI Toolkit — Design Document

**Revision 2.** A C++ UI toolkit library for Linux handheld consoles.  
Target hardware: Cortex-A53 + Mali-G31 MP2 class SoCs. GPU-less targets supported.  
Renderer backend: SDL2 (default) or SDL1 (build-time opt-in).

> **⚠ If you are implementing from this document and the codebase already exists,
> read `MIGRATION.md` first.** Revision 2 changes decisions inside Phases 4 and 5, which are
> already marked complete in `TODO.md`. The existing code does *not* match this document until
> the retrofit in Phase 5.5 is done. `MIGRATION.md` lists exactly what changed and what to
> touch, and tells you when to delete itself.

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
- A scene graph, a reflow engine, or a constraint solver. Widgets are told their `Rect` once
  via `layout()` and cache it; nothing recomputes geometry from content size, and there is no
  invalidation cascade. This is minimal retained layout, not retained-mode layout.
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
  are drawn bottom-to-top so that overlays render above base screens. Stack mutations are
  **deferred** to a safe point, because every overlay pops itself from inside its own event
  handler (§8.2).
- `Shell` is **not** on the view stack. It is a composite widget owning the permanent chrome,
  registered via `UISystem::setShell()`, drawn beneath the stack so a modal's scrim covers it,
  with the toast drawn above everything (§12).
- `FocusManager` is owned by `UISystem` and passed by reference where needed. Its only job is
  the one-focused-widget invariant; traversal lives in each view, with `NavList` available for
  ordered cycles (§7).
- `KeyRepeatDriver` synthesizes repeated `ButtonDown` events in `update()` and injects them
  into the normal event path before delivering them to the view stack. Held state is flushed
  on every stack change (§9.2).
- `IRenderer` is the only SDL-touching interface; all widget drawing is expressed in terms of
  this interface.

The `Shell`/`ViewStack` split in the diagram above should be read as: chrome sits outside the
stack, at a fixed z-position beneath it.

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
    // Used ONLY for widget-local fades: toast fade-out and the guide overlay
    // slide. Modal dimming does NOT use this — it is a single scrim fillRect
    // drawn by ViewStack (§8.2). On SDL1 each call costs a premultiplied
    // surface copy, so it must not be used per layer per frame (§5.2, §13.6).
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

    // --- Layout ---
    // Called when this widget's rect changes: at screen construction, when a
    // container repartitions, when an overlay animates its own rect. NOT called
    // per frame. Container widgets override this to partition their rect and
    // forward layout() to their children.
    virtual void layout(Rect r) { bounds_ = r; }
    Rect bounds() const { return bounds_; }

    // --- Per-frame ---
    // update() is called once per frame. dt = elapsed seconds since last call.
    virtual void update(float dt) {}
    // draw() is called once per frame after update(). The widget draws inside
    // bounds_, which layout() established.
    virtual void draw(IRenderer& renderer, const Theme& theme) = 0;

    // --- Input ---
    // Returns true if the event was consumed (stops propagation).
    virtual bool onButtonDown(Button b) { return false; }
    virtual bool onButtonUp(Button b)   { return false; }

    // --- Focus ---
    // Defaults to false. Only the focusable widgets override this to true:
    // Slider, SeekableProgressBar, ListView, GridView, TabBarWidget, QueueList,
    // LetterWheel, OnScreenKeyboard. FocusManager::setFocus() refuses any widget
    // that returns false, which makes it impossible to focus chrome such as
    // HintBarWidget or StatusBarWidget.
    virtual bool isFocusable() const { return false; }

    // Focus lifecycle. The widget must retain its internal focus state across
    // onBlur/onFocus cycles — it must not reset its selected index on blur.
    virtual void onFocus() {}
    virtual void onBlur()  {}

    bool isFocused()  const { return focused_; }
    bool isDisabled() const { return disabled_; }

    void setDisabled(bool d) { disabled_ = d; }

protected:
    Rect bounds_{};

private:
    bool focused_  = false;
    bool disabled_ = false;

    friend class FocusManager;

    // The flag and the callbacks are deliberately separable. forceOwner() needs
    // to restore the focus highlight after an overlay pops WITHOUT re-running
    // focus side effects; setFocus() needs both. A single fused setter cannot
    // serve both callers.
    void setFocusedFlag(bool f)      { focused_ = f; }
    void setFocusedAndNotify(bool f) { focused_ = f; if (f) onFocus(); else onBlur(); }
};
```

Widgets hold exactly one piece of retained layout state: `bounds_`. There is no layout
tree, no invalidation graph, and no content-driven sizing — a container computes child
rects arithmetically and pushes them down through `layout()`. This is appropriate for
fixed-resolution handheld UIs where layouts do not reflow.

Separating `layout()` from `draw()` matters for three reasons on the target hardware:

1. **Cost.** Expensive geometry work runs on layout change, not 30 times a second.
   `ListHeaderWidget` calls `leftTruncate`, which measures progressively shorter
   substrings against the font — the single most expensive operation in the UI. Its input
   changes when the directory changes, not when a frame is drawn.
2. **Correctness.** Widgets that need their own size to interpret input can now do so.
   `ListView`'s L1/R1 page jump is `bounds_.h / itemHeight_` rows; before `layout()`
   existed, that value was only available during `draw()`, so an input event arriving
   before the first frame had no viewport height to work with.
3. **Testability.** Container partitioning can be verified with no renderer present.

**Ordering contract:** `layout()` must be called on a widget before its first
`onButtonDown()` or `draw()`. `ViewStack` guarantees this for views (see §8.2); container
widgets guarantee it for their children.

### 6.2 Container Widgets

Container widgets own child widgets and are responsible for partitioning their own
`bounds_` into children's rects inside `layout()`.

Containers whose children are **homogeneous rows** (`ListView`, `GridView`, `QueueList`)
do not own one widget per row. They own a single **stamp widget** and redraw it once per
visible row, refilling it from the data source between draws. See §6.5.

The reference pattern — note that only the visible window is touched, and that there is
exactly one clip push for the whole viewport rather than one per row:

```cpp
class ListView : public Widget {
public:
    bool isFocusable() const override { return true; }

    void layout(Rect r) override {
        bounds_ = r;
        pageRows_ = std::max(1, bounds_.h / itemHeight_);
        reclampScroll();
    }

    void draw(IRenderer& r, const Theme& theme) override {
        const int rowCount = source_ ? source_->rowCount() : 0;
        if (rowCount == 0) { drawEmptyState(r, theme); return; }

        // Visible window. O(1) because itemHeight_ is uniform (§13.3).
        const int first = std::max(0, scrollOffset_ / itemHeight_);
        const int last  = std::min(rowCount - 1,
                                   (scrollOffset_ + bounds_.h - 1) / itemHeight_);

        r.pushClip(bounds_);                    // once, not per row
        int y = bounds_.y + first * itemHeight_ - scrollOffset_;  // may be negative
        RowData row;
        for (int i = first; i <= last; ++i) {
            source_->rowAt(i, row);             // fills string_views, allocates nothing
            stamp_.setRow(row);
            stamp_.setRowFocused(i == focusedIndex_ && isFocused());
            stamp_.layout({bounds_.x, y, bounds_.w, itemHeight_});
            stamp_.draw(r, theme);
            y += itemHeight_;
        }
        r.popClip();

        drawScrollIndicator(r, theme);
    }
    // ...
private:
    IListSource*    source_ = nullptr;
    ListItemWidget  stamp_;              // ONE instance, reused for every row
    int itemHeight_    = 0;
    int pageRows_      = 1;
    int focusedIndex_  = 0;
    int scrollOffset_  = 0;              // pixels (§13.3)
};
```

**Why the window matters.** A 5,000-track library drawn with a naive full loop is 5,000
clip pushes and ~15,000 `drawTextEllipsis` calls per frame, relying on clipping to discard
the ~4,985 rows that are off screen. At 480×320 with 20 px rows the visible window is 17
rows, so the windowed version does roughly 0.3% of that work. Every homogeneous-row
container in this library must window.

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

### 6.5 List Data Sourcing

List and grid containers are **virtual**: they hold a row count and a pull interface, not a
copy of the data. The application remains the single owner of its data.

```cpp
enum class ListItemVariant : uint8_t { Default, Track, Folder, Playlist };

// Filled by the source on demand. Contains no owning storage: the string_views
// must remain valid for the duration of the draw() call that requested them,
// which is free if the application holds its data in a container that outlives
// the frame (the normal case).
struct RowData {
    std::string_view primary;
    std::string_view secondary;
    std::string_view rightMeta;
    ListItemVariant  variant  = ListItemVariant::Default;
    TextureHandle    icon     = 0;
    bool             playing  = false;
    bool             disabled = false;
};

class IListSource {
public:
    virtual ~IListSource() = default;
    virtual int  rowCount() const = 0;
    virtual void rowAt(int index, RowData& out) const = 0;
};
```

**Rationale.** The alternative — a `std::vector<ListItemData>` inside the widget with owning
`std::string` fields — costs two to four heap allocations per row and a full duplicate of
the library in UI-land, and creates a second source of truth that must be rebuilt whenever
the application's data changes. On an A53 with shared DRAM and a five-thousand-track
library that is the difference between a usable and an unusable browser. This is the same
distinction as Win32's owner-data (`LVS_OWNERDATA`) list view versus its item-owning
default, and for the same reasons.

For genuinely short, static lists — context menu actions, guide overlay items, on-screen
keyboard rows — writing a source class is more ceremony than it is worth. The library
provides a convenience adapter so that both cases go through one code path in `ListView`:

```cpp
// Owns its strings. Use for lists of a handful of fixed entries.
class VectorListSource : public IListSource {
public:
    void add(std::string primary,
             std::string secondary = {},
             std::string rightMeta = {},
             ListItemVariant variant = ListItemVariant::Default);
    void clear();
    int  rowCount() const override;
    void rowAt(int index, RowData& out) const override;
private:
    struct Entry { std::string primary, secondary, rightMeta;
                   ListItemVariant variant; bool disabled = false; };
    std::vector<Entry> entries_;
};
```

#### 6.5.1 Data Mutation

`ListView` never guesses where focus should go after the data changes. Two explicit entry
points, and the application decides:

```cpp
// The row count or row contents changed. Clamps focusedIndex_ into range,
// reclamps scrollOffset_, and invalidates the text measurement cache.
// Does NOT move focus.
void ListView::notifyRowsChanged();

// Move focus deliberately. Use after an insertion to highlight the new row.
void ListView::setFocusIndex(int index, bool scrollToIt = true);

int  ListView::getFocusIndex() const;
```

Focus memory is **by index**, not by a stable item ID. This is a deliberate simplification:
IDs would require every data source to mint and maintain them, and the one case that
motivates them (the list mutating while a view is suspended) is better solved by the
application, which knows what it changed and where.

The hazard this leaves is worth stating plainly: **if the application inserts or removes
rows while a view holds a focus index, that index now refers to a different row.** The
application must call `notifyRowsChanged()`, and if it cares which row is focused
afterwards, `setFocusIndex()`. The toolkit will not infer it.

> Example. The user is in `NowPlayingView` with queue row 7 focused, browses to a file,
> adds it to the queue at position 3, and returns. The queue view was never popped
> (see §8.2), so its scroll offset and focus index are exactly as they were — but row 7 is
> now the track that used to be row 6. The application calls `notifyRowsChanged()` and then
> `setFocusIndex(3)` to highlight what was just added.

Scrolling must **not** invalidate the text measurement cache (§13.1); only
`notifyRowsChanged()` does.

---

## 7. Focus Management

### 7.1 What FocusManager Is And Is Not

Focus in this toolkit is two separate things:

1. **Which widget is the focus context** — the `ListView`, the `QueueList`, the slider row.
2. **Which item inside that container is highlighted** — `focusedIndex_`.

`FocusManager` knows only the first. The highlight the user actually sees is almost always
the second, and it lives inside the container. This is worth stating explicitly because it
bounds what `FocusManager` is for: it is a small bookkeeping object that enforces the
*exactly one focused widget* invariant from the widget guide (§1.1), and nothing more.
Traversal does not live here (see §7.3).

In this application there is at most one focusable widget per screen in the common case —
`DirectoryView` has a `ListView`; `LibraryView` has a `ListView` or a `GridView` per tab,
with the `TabBarWidget` reachable only via L1/R1 and never by D-pad; `NowPlayingView` treats
the whole screen as one focus context. The two exceptions (`ConfirmationDialogView`,
`GuideOverlayView`) are handled by `NavList`, §7.3. Do not grow `FocusManager` beyond what
follows.

```cpp
class FocusManager {
public:
    // Give focus to a widget, running the full lifecycle: onBlur on the previous
    // owner, onFocus on the new one.
    //
    // Refuses and returns false, leaving focus unchanged, if w is non-null and
    // either !w->isFocusable() or w->isDisabled(). This makes it structurally
    // impossible to focus chrome (HintBarWidget, StatusBarWidget, ListHeaderWidget,
    // PlaybackControlsRow) or a disabled control.
    //
    // setFocus(nullptr) always succeeds: it blurs the current owner and leaves
    // focus unowned.
    //
    // No-op returning true if w already holds focus (no duplicate callbacks).
    bool setFocus(Widget* w);

    Widget* focused() const { return current_; }

    // Convenience: does current_ == w ?
    bool hasFocus(const Widget* w) const;

    // Restore ownership WITHOUT running onFocus/onBlur, used by
    // View::restoreFocus() after an overlay pops. It still updates the focused_
    // flag on both the outgoing and incoming widget, so the highlight renders
    // correctly — it only skips the side effects. Subject to the same
    // isFocusable()/isDisabled() refusal as setFocus().
    bool forceOwner(Widget* w);

private:
    Widget* current_ = nullptr;
};
```

> **Implementation note.** `forceOwner` can only work because `Widget` exposes
> `setFocusedFlag()` separately from `setFocusedAndNotify()` (§6.1). A single fused
> `setFocused(bool)` that always fires the callbacks makes `forceOwner` unable to update
> the flag at all, which renders the restored widget un-highlighted. This was a real defect
> in an earlier revision of this document.

### 7.2 Focus Memory

Focus memory is cheap in this design because **views are not destroyed when navigated away
from** — they stay on the stack (§8.2). A suspended `ListView` keeps its `focusedIndex_` and
its `scrollOffset_` untouched, with no save/restore involved. What actually needs restoring
is one bool: the focus highlight.

Each `View` owns a `Widget*` field `savedFocus_`:

```cpp
// Called by ViewStack when an overlay is pushed on top of this view.
// Records savedFocus_ = fm.focused(), then fm.setFocus(nullptr) so the
// outgoing widget receives onBlur() and visually deactivates
// (widget guide §1.1).
void View::suspendFocus(FocusManager& fm);

// Called by ViewStack when this view returns to the top.
// Default implementation: fm.forceOwner(savedFocus_).
// VIRTUAL: a view may override to focus something else instead — e.g.
// NowPlayingView highlighting a track that was just inserted into the queue.
virtual void View::restoreFocus(FocusManager& fm);
```

`restoreFocus` uses `forceOwner` rather than `setFocus` deliberately: returning from an
overlay should re-light the highlight without re-running `onFocus()` side effects.

Container widgets with internal focus indices (`ListView`, `GridView`) must never reset
those indices on `onBlur()`. They reset only when explicitly instructed via `resetFocus()`
— for example when the Tab Bar switches tabs, the new tab's list resets.

### 7.3 Focus Traversal

Focus traversal is not automatic. Each view and container widget handles D-pad events and
routes them itself. This is intentional: on a gamepad UI there is no universal DOM-style tab
order. A `ListView` handles Up/Down internally; a dialog routes Left/Right between Cancel
and Confirm; a `TabBarWidget` is reachable only through L1/R1; `QueueList` reassigns Up/Down
entirely while in grab mode. Every screen in the widget guide has bespoke button semantics,
and a generic spatial or tab-order algorithm would fight all of them.

Note also that widgets know their own rect (§6.1) but the library still does **not** attempt
spatial navigation. `layout()` exists for cost and correctness, not to enable a geometry-based
traversal engine.

What the library does provide, to stop every `View` subclass from hand-rolling the same
ordered-cycle logic, is one small helper. It owns no widgets and imposes no policy:

```cpp
enum class Axis : uint8_t { Vertical, Horizontal };

class NavList {
public:
    void setAxis(Axis a);           // default Vertical
    void setWrap(bool w);           // default true (widget guide §1.3)

    void add(Widget* w);            // insertion order == traversal order
    void clear();

    // Consumes Up/Down when axis is Vertical, Left/Right when Horizontal.
    // Skips entries where !isFocusable() || isDisabled(). Wraps if enabled.
    // Returns false for buttons on the other axis, and for an empty list,
    // so the caller can go on to handle them.
    bool handleButton(Button b, FocusManager& fm);

    bool    focusIndex(int i, FocusManager& fm);
    int     index() const;
    Widget* current() const;

private:
    std::vector<Widget*> items_;     // non-owning
    Axis axis_  = Axis::Vertical;
    bool wrap_  = true;
    int  index_ = 0;
};
```

Exactly two components in the application need it, and making the axis a property rather
than a layout decision means both use the identical mechanism:

- `ConfirmationDialogView` — `Axis::Horizontal`, wrap off, Cancel then Confirm, default
  index 0 (Cancel, per widget guide §4.2). The buttons stay side by side, which is the
  conventional handheld dialog layout, without needing a second traversal mechanism.
- `GuideOverlayView` — `Axis::Vertical`, the "sliders and actions in one unified focus list"
  from widget guide §4.5. Left/Right falls through `handleButton` and is forwarded to the
  focused `Slider`, which is exactly the specified behaviour ("←→ adjusts value when a
  slider is focused; ignored on action items").

`NavList` is deliberately not a container widget: it does not draw, does not own, and does
not lay out. Views that use it lay their children out themselves.

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

    // Layout. Called by ViewStack with the content rect (§8.2) on push and
    // whenever that rect changes. Views partition it among their widgets and
    // forward layout() down. Guaranteed to be called before the first
    // update()/draw()/onButtonDown().
    virtual void layout(Rect contentRect) { bounds_ = contentRect; }

    // Per-frame
    virtual void update(float dt, FocusManager& fm) {}
    virtual void draw(IRenderer& r, const Theme& theme) = 0;

    // Input — return true if consumed
    virtual bool onButtonDown(Button b, FocusManager& fm) { return false; }
    virtual bool onButtonUp  (Button b, FocusManager& fm) { return false; }

    // --- Modal scrim ---
    // Return true if this view should darken everything beneath it. Overlay
    // views that are modal and short-lived (ContextMenuView,
    // ConfirmationDialogView, TrackInfoPanelView, GuideOverlayView) return true.
    // Ordinary screens return false: normal navigation never dims.
    //
    // The scrim is drawn by ViewStack as ONE full-screen fillRect immediately
    // before this view, which means it covers the Shell chrome too — status bar
    // and hint bar dim along with the content, which is the intended effect.
    virtual bool dimsBelow() const { return false; }

    // Focus memory (§7.2)
    void         suspendFocus(FocusManager& fm);
    virtual void restoreFocus(FocusManager& fm);

    // Hint bar data — views publish what the hint bar should show.
    // The Shell reads this from the top-most view each frame (§13.4).
    virtual std::vector<HintEntry> currentHints() const { return {}; }

protected:
    Rect     bounds_{};
    Widget*  savedFocus_ = nullptr;
};
```

The application subclasses `View` to implement each screen (DirectoryView, LibraryView,
NowPlayingView, etc.). The view owns all its widgets as member variables or via
`std::unique_ptr`.

Note there is no `setDimmed()` / `isDimmed()` pair. Dimming is a property of the view being
pushed **on top**, not a state pushed down onto the views below, and it is realised by a
single scrim fill rather than by per-layer alpha. See §8.2.

### 8.2 ViewStack

```cpp
class ViewStack {
public:
    // --- Mutation (all DEFERRED — see below) ---
    void push(std::unique_ptr<View> view);
    void pop();                                  // no-op if only one entry
    template<typename T> void popTo();           // pop until a T is on top
    void replace(std::unique_ptr<View> view);    // atomic pop+push

    // Applies queued mutations and runs the lifecycle callbacks.
    // Returns true if the stack changed, which tells UISystem to flush held
    // buttons in KeyRepeatDriver (§9.2).
    bool applyPendingMutations(FocusManager& fm);
    bool hasPendingMutations() const;

    // The rect that views are laid out into: the screen minus Shell chrome.
    // Set by Shell (§12). Triggers layout() on all stacked views when it changes.
    void setContentRect(Rect r);
    Rect contentRect() const;

    View* top() const;
    bool  empty() const;
    int   depth() const;

    // Drive all views on the stack.
    void update(float dt, FocusManager& fm);

    // Draw all views bottom-to-top, inserting a scrim before any view whose
    // dimsBelow() is true.
    void draw(IRenderer& renderer, const Theme& theme);

    // Deliver event to top view only.
    bool dispatchButtonDown(Button b, FocusManager& fm);
    bool dispatchButtonUp  (Button b, FocusManager& fm);

private:
    std::vector<std::unique_ptr<View>> stack_;
    // Queued mutations, applied at a safe point.
    std::vector<PendingMutation>       pending_;
    Rect contentRect_{};
};
```

#### Deferred mutation — why this is not optional

Every overlay in the catalogue pops **itself** from inside its own `onButtonDown` (B
dismisses a context menu; A on Confirm closes a dialog; Guide closes the guide panel). If
`pop()` destroyed the view immediately, the `unique_ptr` would run the destructor while the
call stack was still inside a member function of that object, and `dispatchButtonDown` would
then read the return value from a dead frame. That is use-after-free in the default control
flow of every overlay in the application, not an edge case.

So `push`/`pop`/`popTo`/`replace` only enqueue. `UISystem` applies the queue at two safe
points: at the top of `update()`, and immediately after event dispatch has fully unwound.

Lifecycle order when the queue is applied:

```
push(B):   A.suspendFocus(fm)   // records savedFocus_, then fm.setFocus(nullptr) → A's widget gets onBlur
           A.onSuspend()
           B.onPush()
           B.layout(contentRect_)
           B sets its own initial focus

pop():     fm.setFocus(nullptr)  // BEFORE destruction — never leave current_ dangling
           B.onPop()
           destroy B
           A.onResume()
           A.restoreFocus(fm)    // forceOwner(savedFocus_), or A's override
```

`fm.setFocus(nullptr)` before destruction is what keeps `FocusManager::current_` from
pointing into a destroyed view's widgets. `popTo<T>()` and `replace()` destroy views the
same way and must do the same.

**Drawing bottom-to-top** is necessary because overlays are visually partial and the base
screen must remain visible behind them. A view that should be invisible when not on top
simply should not be on the stack.

#### Dimming

`ViewStack::draw()` walks the stack bottom-to-top and, before drawing any view whose
`dimsBelow()` returns true, fills the **whole screen** once with `theme.overlay`:

```cpp
for (auto& v : stack_) {
    if (v->dimsBelow())
        renderer.fillRect({0, 0, screen.w, screen.h}, theme.overlay);
    v->draw(renderer, theme);
}
```

This replaces the earlier per-layer `setGlobalAlpha(128)` scheme, which was both redundant
with the scrim that overlays already drew themselves (§8.3) and actively harmful on SDL1,
where `setGlobalAlpha` costs a premultiplied surface copy per dimmed layer per frame — the
one thing §5.2 says not to do every frame. One `fillRect` against a pre-filled surface is a
single blit (§13.6).

Two consequences worth being explicit about:

- **Normal navigation never dims.** Only modal overlays set `dimsBelow()`, so browsing
  directories and switching tabs runs at full brightness.
- **Chrome dims with the content.** Because the scrim spans the full screen and Shell chrome
  is drawn beneath the view stack (§12), opening a confirmation dialog dims the status bar
  and hint bar along with everything else. That is the intended effect: the dialog is the
  only live thing on screen.

`setGlobalAlpha` survives in the renderer interface, but only for what it is actually good
at: toast fade-out and the guide overlay slide (§6.4).

### 8.3 Overlay Views

Overlays are ordinary `View` subclasses that happen to be pushed on top of a base screen.
They are visually partial — they do not fill the whole screen — and they return `true` from
`dimsBelow()`, which makes `ViewStack` lay down the scrim for them (§8.2). An overlay must
**not** draw its own full-screen fill; that was the old convention and doing both produces a
double-darkened background.

Overlay views that should slide in (e.g. `GuideOverlayView`) animate their own rect in
`update()` and call `layout()` on their children as it changes.

### 8.4 Screen Transitions

Transitions are absent, and the library provides **no** transition machinery at all. On
low-end hardware cross-fading two full frames doubles rendering work, and there is exactly
one application consuming this toolkit — a `TransitionKind` enum and a `SimpleTransition`
helper with no caller is speculative surface area to be maintained through every subsequent
phase for no benefit. Both are removed.

If the application later wants a transition, it wraps `ViewStack`, overrides `draw()`, and
drives a timer from `update()`. Should that happen and should the code prove reusable, it
can be promoted into the library at that point, with a real consumer to validate its shape.

The guide overlay's slide-in is not a screen transition — it is a widget animating its own
rect (§6.4, §8.3).

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
    // dt is clamped by UISystem before it gets here (§10).
    template<typename Sink>
    void update(float dt, Sink&& sink);

    // Drop all held state without emitting anything. Called by UISystem
    // whenever the view stack changes, and on suspend/resume.
    void flushHeld();

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

**Synthetic `Down` only.** The driver never fabricates a matching `Up`. Repeats model
"the user is still pressing", not a press-release sequence, and a widget latching on
Down/Up pairs would see them unbalanced if we synthesised both. Widgets that need press
duration must use the real `Up`, which always arrives exactly once.

**Held state is flushed on every stack change.** Without this, holding Down while an
overlay opens means the repeat stream immediately starts scrolling the *overlay* — the
user's finger was never on the overlay's list. `UISystem::update()` calls `flushHeld()`
whenever `ViewStack::applyPendingMutations()` reports a change. The same applies on
suspend and resume (§10).

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

Views that traverse an ordered set of heterogeneous children should delegate that part to
`NavList` (§7.3) rather than open-coding the cycle.

**Consume discipline.** A handler returns `true` if and only if it acted on the event.
Returning `true` for an event you ignored silently swallows it; returning `false` for an
event you acted on lets it fire twice. The most common mistake is a container returning
`true` for the whole directional group when it only handled one axis — see
`SeekableProgressBar`, which must return `false` for everything except L2/R2.

### 9.4 Button Chords

The widget guide specifies START+SELECT as a fallback for opening the guide overlay on
hardware with no dedicated Guide button. Chord detection is not something individual views
can do, so it lives in the input layer:

```cpp
class ChordDetector {
public:
    // Register: when all of `inputs` are held simultaneously, emit `output`
    // and suppress the individual inputs.
    void addChord(std::initializer_list<Button> inputs, Button output);

    // Returns the substituted button if a chord completed, otherwise the
    // original. Called by UISystem before KeyRepeatDriver sees the event.
    std::optional<Button> onButtonDown(Button b);
    std::optional<Button> onButtonUp  (Button b);

    static constexpr float kChordWindow = 0.150f;  // seconds
};
```

The default configuration registers `{Start, Select} → Guide`. Applications on hardware
with a real Guide button can leave it registered harmlessly, or clear it.

Chord detection necessarily delays the individual buttons by up to `kChordWindow`. This is
acceptable for Start and Select, which are never used in time-critical interactions; do not
register chords involving directional or face buttons.

---

## 10. Time and the Update Loop

```cpp
class UISystem {
public:
    UISystem(IRenderer& renderer, const Theme& theme);

    // Called by the application once per frame.
    // elapsedSeconds: wall-clock time since the previous call, CLAMPED
    // internally to kMaxDelta before anything sees it.
    // Drives: pending stack mutations, KeyRepeatDriver, ViewStack::update.
    void update(float elapsedSeconds);

    // Called by the application once per frame, after update().
    // Draw order: Shell chrome → view stack (with scrims) → Shell overlay layer.
    void draw();

    // Input — called by the application from its SDL event loop.
    void onButtonDown(Button b);
    void onButtonUp  (Button b);

    // --- Screen-off support (§10.1) ---
    void setSuspended(bool s);
    bool isSuspended() const;

    // --- Shell chrome (§12) ---
    // Optional. Nullable. Not owned. Setting it also wires the Shell's content
    // rect into the view stack.
    void   setShell(Shell* shell);
    Shell* shell() const;

    void setAnimationsEnabled(bool e);   // §13.6

    ViewStack&    viewStack();
    FocusManager& focusManager();

    // A single frame may legitimately be long (asset load, SD card stall).
    // Anything longer than this is a gap, not a frame, and must not be
    // integrated. See §10.1.
    static constexpr float kMaxDelta = 0.100f;

private:
    IRenderer&      renderer_;
    const Theme&    theme_;
    ViewStack       viewStack_;
    FocusManager    focusManager_;
    KeyRepeatDriver keyRepeat_;
    ChordDetector   chords_;
    Shell*          shell_        = nullptr;
    bool            suspended_    = false;
    bool            clearPending_ = false;
};
```

`update()` in order: clamp `dt`; apply pending stack mutations and `flushHeld()` if the
stack changed; drive `KeyRepeatDriver`, injecting synthetic events into the stack; apply
pending mutations again in case a synthetic event caused a push or pop; `ViewStack::update()`.

### 10.1 Screen-Off / Suspend

Turning the panel or backlight off is device-specific and therefore out of scope — that is
the application's job. What the toolkit provides is the state that makes it safe and cheap:

```cpp
uiSystem.setSuspended(true);
// ... application turns off its display by whatever device-specific means ...
```

While suspended:

- **The first `draw()` clears to black and presents; every subsequent `draw()` is a no-op.**
  Not "draws black every frame" — that would still cost a full-frame blit and a vsync wait
  per frame for the entire idle period, on a device whose display is off. Clearing once
  guarantees nothing stale is in the framebuffer when the panel comes back; after that the
  application is free to `SDL_Delay()` or block on events.
- **`update()` still runs**, so `KeyRepeatDriver` timing stays coherent, but see the clamp
  below.
- **Input is recorded but not dispatched.** `onButtonDown` updates held state and returns
  without reaching the view stack, so the button press that wakes the device does not also
  activate whatever happened to be focused. The application observes the press, calls
  `setSuspended(false)`, and normal dispatch resumes with the next event.
- On resume, `flushHeld()` runs and the next `draw()` repaints everything — free here,
  because there is no dirty-rect system (§6.3).

**The `dt` clamp is what makes this safe, and it is not optional.** If the application sleeps
its loop for twenty minutes and then feeds the real elapsed time to `update()`, a `dt` of
1200 seconds enters `KeyRepeatDriver`, which will faithfully synthesise roughly forty
thousand repeat events into whichever view is on top. §6.4 protects animations from this by
accident (they clamp to their end state); the repeat driver has no such protection. Clamping
in `UISystem::update()` fixes it once for everything downstream.

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
    uiSystem.update(dt);          // clamps dt internally to kMaxDelta

    // -- Draw phase --
    // While suspended, draw() clears to black once and is a no-op thereafter,
    // so the loop is free to sleep instead of spinning (§10.1).
    renderer.beginFrame();
    uiSystem.draw();
    renderer.endFrame();

    if (uiSystem.isSuspended())
        SDL_Delay(100);
}
```

The library imposes no frame rate cap and has no internal timer thread. Frame pacing is
entirely the application's responsibility. Passing a real, unclamped `dt` is safe: `UISystem`
clamps it, which is what prevents a long stall or a screen-off period from being integrated
as tens of thousands of key-repeat events (§10.1).

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
| `KeyRepeatDriver` | internal | Owned by UISystem; synthesizes repeat `Down` events; `flushHeld()` on stack change |
| `ChordDetector` | internal | Owned by UISystem; `{Start,Select} → Guide` (§9.4) |
| `FocusManager` | internal | Owned by UISystem; enforces the one-focus invariant only (§7.1) |
| `NavList` | helper | Not a widget. Ordered focus cycle for `ConfirmationDialogView` and `GuideOverlayView` (§7.3) |
| `IListSource` / `VectorListSource` | interface / helper | Row data pull interface for list and grid containers (§6.5) |

### Level 1 — Atoms

| Widget | Description |
|--------|-------------|
| `ListItemWidget` | Single row: icon, primary label, secondary label, right meta. Variants: default, track, folder, playlist. Handles focused/playing/disabled visual states. **Used as a stamp** — `ListView` owns one instance and refills it per visible row from `RowData` (§6.2, §6.5). Not focusable; the row highlight is set by the owning container. |
| `GridCellWidget` | Single grid tile: thumbnail (or gradient placeholder), label, sublabel. Focused border + playing badge. Also a stamp, reused per visible cell. |
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
| `ToastNotification` | Non-focusable **Widget owned by `Shell`** — never pushed onto the ViewStack. Self-timed via `update()`. Auto-dismissed; replacement policy (not stacked). Drawn in the Shell overlay layer, above everything and never dimmed. |

### Level 3 — Organisms

| Widget / View | Description |
|---------------|-------------|
| `ListView` | Scrollable vertical list. Internal D-pad routing, key-repeat for Up/Down, page jump via L1/R1, first/last via L2/R2, wrap-around, scroll-to-focus, focus memory via `getFocusIndex()`, empty state, scroll indicator. |
| `GridView` | 2D focusable grid. Same scroll/wrap/page/memory rules as ListView. |
| `TabBarWidget` | Horizontal tab strip. L1/R1 to switch. Not reachable via D-pad Up/Down. |
| `QueueList` | Extends ListView with grab-mode reorder (Y to grab, Up/Down to reorder, A to drop, B to cancel). |
| `LetterWheel` | Character strip + results list. Internal focus routing between strip and list. |
| `OnScreenKeyboard` | 2D QWERTY grid. Confirm/Cancel/Backspace keys. |
| `ContextMenuView` | Overlay View, `dimsBelow() == true`. Backed by a `VectorListSource`. Action list with destructive color support. Does not draw its own scrim (§8.3). |
| `ConfirmationDialogView` | Overlay View, `dimsBelow() == true`. `NavList` with `Axis::Horizontal`, wrap off, default index 0 (Cancel). A activates, B cancels. |
| `TrackInfoPanelView` | Overlay View, `dimsBelow() == true`. Read-only metadata table. |
| `GuideOverlayView` | Overlay View, `dimsBelow() == true`, slides in from right. `NavList` with `Axis::Vertical` over the two `Slider`s plus the action items — the "unified focus list". Left/Right falls through `NavList` to the focused `Slider`. |

### Level 4 — Screens

These are application-level `View` subclasses, with one exception: **`Shell` is not a View
and is not on the stack.**

#### Shell

`Shell` is a composite `Widget` that owns the permanent chrome — `StatusBarWidget`,
`HintBarWidget`, an optional `TabBarWidget`, and the `ToastNotification`. It is registered
with `UISystem::setShell()` and holds a `ViewStack&` so its hint bar can read
`top()->currentHints()` each frame (§13.4).

It is deliberately not a stack entry. Putting it on the stack meant it was never `top()`, so
under the old per-layer dimming rule the status bar and hint bar were dimmed permanently,
during ordinary navigation, forever. It also contradicted the requirement that Shell own the
content rect that page views are laid out into — a stack entry cannot meaningfully hand a
rect to the entry above it.

```cpp
class Shell : public Widget {
public:
    explicit Shell(ViewStack& stack);

    void layout(Rect screen) override;   // partitions chrome, then
                                         // stack_.setContentRect(contentRect_)
    Rect contentRect() const;

    void drawChrome (IRenderer& r, const Theme& theme);  // beneath the view stack
    void drawOverlay(IRenderer& r, const Theme& theme);  // toast, above everything

    void showToast(std::string_view message, float seconds);
    void setTabBarVisible(bool v);
    // ... status bar setters: view mode, context label, playing, clock, battery
};
```

Draw order, enforced by `UISystem::draw()`:

```
shell->drawChrome()      // status bar, hint bar, tab bar — full brightness
viewStack.draw()         // base view, then scrim + overlay if a modal is open
shell->drawOverlay()     // toast — above the scrim, never dimmed
```

Chrome is drawn *beneath* the stack so that a modal's full-screen scrim covers it (§8.2).
The toast is drawn after everything because the widget guide requires it to be visible
without stealing focus, which is also why it must not be a View: a pushed toast would become
`top()`, receive all input, and replace the hint bar's contents.

| View | Widgets Used |
|------|-------------|
| `DirectoryView` | ListView, ListHeaderWidget, pushes ContextMenuView / TrackInfoPanelView / ToastNotification |
| `LibraryView` | ListView + GridView + TabBarWidget, pushes ContextMenuView / LetterWheel / TrackInfoPanelView |
| `NowPlayingView` | SeekableProgressBar, PlaybackControlsRow, QueueList, pushes ContextMenuView / TrackInfoPanelView |

---

## 13. Implementation Notes

### 13.1 Text Layout

`IRenderer::drawTextEllipsis` must handle UTF-8 correctly — grapheme clusters, not raw
bytes. On SDL_ttf, measure progressively shorter substrings until the text + "…" fits.

**Cache key: string content + font handle. Never row index, slot number, or y position.**

This is the single most performance-critical rule in the document, because smooth scrolling
(§13.3) redraws every visible row at a new y on every frame while the strings are identical.
That is precisely the case the cache exists to absorb. A cache keyed on row slot or position
is invalidated every frame while scrolling and silently destroys the frame budget: with
uncached `TTF_RenderUTF8_Blended` calls instead of cached-surface blits, a scrolling list on
the SDL1 target drops from 30 fps to single digits.

Corollary: **scrolling must not invalidate the cache**; only `notifyRowsChanged()` does
(§6.5.1).

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

Scrolling is **pixel-granular and smooth**, with **uniform row height**. `scrollOffset_` is
in pixels and is assigned directly to its target — it is not animated toward it. This still
looks smooth because the steps are one row, not one page, and it avoids a class of problem
described below.

```cpp
void ListView::scrollToFocus(bool movingDown) {
    const int focusedItemTop = focusedIndex_ * itemHeight_;

    // Widget guide §1.5: bottom third when moving down, top third when moving up.
    const int target = movingDown
        ? (focusedItemTop + itemHeight_) - bounds_.h * 2 / 3
        :  focusedItemTop               - bounds_.h * 1 / 3;

    reclampScroll(target);
}

void ListView::reclampScroll(int target) {
    const int rowCount           = source_ ? source_->rowCount() : 0;
    const int totalContentHeight = rowCount * itemHeight_;

    // maxScroll MUST be floored at 0. When the list is shorter than the
    // viewport, totalContentHeight - bounds_.h is negative, and std::clamp
    // with lo > hi is undefined behaviour. A short or empty list is not an
    // edge case here — Phase 12 QA tests a zero-item list explicitly.
    const int maxScroll = std::max(0, totalContentHeight - bounds_.h);

    scrollOffset_ = std::clamp(target, 0, maxScroll);
}
```

**Uniform `itemHeight_` is what keeps this cheap.** Index ↔ pixel is multiplication in both
directions, the visible window is two divisions (§6.2), and no cumulative-offset table is
needed. Everything in the widget guide fits a uniform row height; the one apparent exception,
the guide overlay's mixed slider-and-action list, is a hand-laid-out view driven by `NavList`
(§7.3) rather than a `ListView`, precisely so this stays true.

**Page jump is a whole number of rows:** `pageRows_ * itemHeight_`, where
`pageRows_ = max(1, bounds_.h / itemHeight_)`, computed in `layout()`. Using the raw pixel
height instead would accumulate sub-row drift across repeated L1/R1 presses.

**Wrap-around always snaps**, regardless of any animation policy. Pressing Down on the last
of five thousand tracks sets `scrollOffset_ = 0` immediately; it must not travel through a
hundred thousand pixels of intermediate positions.

> **If eased scrolling is added later**, it needs a hard clamp: never let the animated offset
> lag more than one row behind what `focusedIndex_` requires. At the accelerated key-repeat
> interval of 30 ms (§9.2) the user generates ~33 focus moves per second, so any tween slower
> than roughly 700 px/s falls behind and the focused row leaves the viewport while the scroll
> is still catching up. Direct assignment has no such failure mode.

**Both edge rows bleed**, since the first and last visible rows are partially outside
`bounds_`. The single `pushClip(bounds_)` in §6.2 is therefore mandatory, not defensive.

`ListHeaderWidget` is **pinned, not scrolled.** `ListView`'s bounds are reduced by the header
height and the header is drawn outside them, so a left-truncated directory path stays legible
while a long listing scrolls beneath it. Consequently the header is not row −1 and none of
the arithmetic above needs a bias term.

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

`HintBarWidget` — not the views — is responsible for enforcing the presentation rules from
the widget guide (§2.2), so that no view has to remember them:

- Sort by `sortOrder`, which encodes the mandated order: A, X, Y, Start/Select, shoulder
  pairs, Guide, then B always rightmost.
- **Cap at five visible hints, truncating from the middle**, so that A and B — the two the
  user relies on most — always survive.
- Update with no animation delay; the bar reads fresh data every frame, so this is automatic.

Since `Shell` is not on the stack (§12), `top()` is always a real page or overlay view and
never the Shell itself.

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
- **Overlay dimming**: exactly one full-screen scrim `fillRect` per modal (§8.2), not one
  `setGlobalAlpha` pass per stacked layer. On SDL1, implement `fillRect` of
  `theme.overlay` against a pre-filled 50%-grey surface with `SDL_SetAlpha` — a single
  `SDL_BlitSurface`. A per-pixel alpha loop, or the old per-layer `setGlobalAlpha` scheme
  (which costs a premultiplied surface copy per layer per frame), will not hold the budget.
- **Scrolling a list**: every visible row redraws at a new y every frame while scrolling —
  roughly 17 rows × 3 text draws at 480×320. This is comfortably affordable *only* because
  the text cache is keyed on string content (§13.1). Verify that assumption on the target
  before assuming the budget holds.
- **Suspended**: after the first black clear, `draw()` is a no-op and the application is free
  to sleep its loop (§10.1). Do not present a black frame per tick.
- **GuideOverlay slide animation**: on SDL1, skip the animation (instant open) unless the
  device is known to be fast enough. Expose `UISystem::setAnimationsEnabled(bool)`.

### 13.7 Memory Ownership Summary

| Thing | Owner |
|-------|-------|
| `IRenderer` | Application |
| `Theme` | Application (passed by const ref) |
| `UISystem` | Application |
| `Shell` | Application; registered with `UISystem::setShell()`, not owned by it |
| `ViewStack` | `UISystem` |
| `FocusManager` | `UISystem` |
| `KeyRepeatDriver`, `ChordDetector` | `UISystem` |
| Views on the stack | `ViewStack` (via `unique_ptr`) |
| Widgets inside a View | The `View` (member variables or `unique_ptr`) |
| Chrome widgets + `ToastNotification` | `Shell` |
| Row data behind `IListSource` | Application — the toolkit copies nothing (§6.5) |
| `Widget*` in `NavList` / `View::savedFocus_` / `FocusManager::current_` | Non-owning. `ViewStack` nulls focus before destroying a view (§8.2) |
| Textures / FontHandles | `IRenderer`; callers get opaque handles |

### 13.8 No Exceptions, No RTTI

All methods that can fail return a bool or `std::optional`. `dynamic_cast` is not used;
views and widgets use the visitor pattern or explicit type tags where runtime type
discrimination is unavoidable.

---

*End of design document.*
