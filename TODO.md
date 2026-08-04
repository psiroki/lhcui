# Handheld UI Toolkit — Development Checklist

Step-by-step implementation order. Use alongside `DESIGN.md`. Each phase should compile and be testable before moving to the next.

> **For the testing team:** every phase ends with a **QA Sign-off** block. Work through those items after the developer marks the phase complete, then check each box to confirm the phase is verified and ready to proceed.

---

## Phase 1 — Project Skeleton

- [x] Create the CMake root `CMakeLists.txt`; define two targets: `hui` (static library) and `hui_example` (executable)
- [x] Add `HUI_USE_SDL1` build option; wire it to a compile definition propagated to both targets
- [x] Add `HUI_ENABLE_KEYBOARD_FALLBACK` build option; default to ON when `NDEBUG` is not defined
- [x] Set up the `include/hui/` and `src/` directory structure; configure include paths
- [x] Confirm a minimal `main.cpp` (empty SDL window loop) in `example/` builds and links cleanly for both SDL1 and SDL2 configurations

### ✅ QA Sign-off — Phase 1

> **Testing team:** confirm all items below before marking this phase done.

- [x] `cmake -DHUI_USE_SDL1=OFF ..` and `cmake -DHUI_USE_SDL1=ON ..` both configure without CMake errors
- [x] Both configurations compile and link to a runnable (empty window) binary with zero warnings at `-Wall -Wextra`
- [x] `HUI_ENABLE_KEYBOARD_FALLBACK` is defined in the build when `NDEBUG` is absent, and absent when `NDEBUG` is set; confirm by inspecting compile flags
- [x] No SDL header appears anywhere under `include/hui/` (grep check)

---

## Phase 2 — Core Types (`include/hui/types.h`)

All in the `hui` namespace. No dependencies on SDL or the renderer.

- [x] Define `Point`, `Size`, `Rect` structs (§4.1)
- [x] Implement `rectFromPoints`, `rectContains`, `rectInset` free functions
- [x] Define `Color` struct with `white()`, `black()`, `transparent()` static helpers, `withAlpha()`, and `lerp()` (§4.2)
- [x] Define `FontHandle` and `TextureHandle` as `uint32_t` typedefs; document that `0` is the null handle
- [x] Define `Button` enum class (§4.4)
- [x] Define `ButtonEventKind` enum class and `ButtonEvent` struct with the `synthetic` flag (§4.5)
- [x] Define `HintEntry` struct (`buttonLabel`, `actionLabel`, `hold`, `sortOrder`) (§13.4)
- [x] Define `Theme` struct with all color and font fields (§4.3)

### ✅ QA Sign-off — Phase 2

> **Testing team:** confirm all items below before marking this phase done.

- [x] `rectContains` returns `true` for a point on the border and `false` for a point one pixel outside
- [x] `rectInset` with `dx=5, dy=5` on a `{0,0,100,80}` rect produces `{5,5,90,70}`
- [x] `Color::lerp` at `t=0.0f` returns the original color exactly; at `t=1.0f` returns the target color exactly
- [x] `Color::withAlpha(128)` preserves the original `r`, `g`, `b` values
- [x] `ButtonEvent` default-constructed has `synthetic == false`
- [x] All 15 `Button` enum values compile without duplicate or gap (static_assert on `Button::COUNT == 15`)

---

## Phase 3 — Renderer Abstraction

- [x] Define the `IRenderer` pure-virtual interface in `include/hui/IRenderer.h` (§5.1)
  - [x] Frame lifecycle: `beginFrame()`, `endFrame()`
  - [x] Clip stack: `pushClip()`, `popClip()`
  - [x] Primitives: `fillRect()`, `drawRect()`, `drawLine()`
  - [x] Text: `drawText()`, `measureText()`, `drawTextEllipsis()`
  - [x] Textures: `loadTexture()`, `freeTexture()`, `textureSize()`, `drawTexture()`
  - [x] Alpha modulation: `setGlobalAlpha()`
  - [x] Query: `screenSize()`
- [x] Implement `SDL2Renderer` in `src/renderer/SDL2Renderer.cpp` (SDL2 + SDL2_ttf only; no SDL header leaks into headers) (§5.2)
  - [x] Internal `FontHandle` → `TTF_Font*` map; `TextureHandle` → `SDL_Texture*` map
  - [x] Clip stack emulated with `SDL_RenderSetClipRect` using intersected rects
  - [x] `drawTextEllipsis` with UTF-8-correct progressive truncation and per-string width cache (§13.1)
  - [x] `setGlobalAlpha` via `SDL_SetRenderDrawBlendMode` / `SDL_SetTextureAlphaMod`
- [x] Implement `SDL1Renderer` in `src/renderer/SDL1Renderer.cpp` (guarded by `HUI_USE_SDL1`) (§5.2)
  - [x] Clip stack via `SDL_SetClipRect` (intersected)
  - [x] `setGlobalAlpha` via pre-multiplied surface copy (not called every frame)
  - [x] Glyph surface cache: same string + font → reuse surface, do not re-render (§13.6)
  - [x] `drawTextEllipsis` with same UTF-8 truncation logic as SDL2 variant
- [x] Write a minimal renderer smoke-test in the example app: draw a filled rect, a border rect, and a line; confirm both backends render correctly

### ✅ QA Sign-off — Phase 3

> **Testing team:** confirm all items below before marking this phase done.

- [x] Smoke-test scene (filled rect, border rect, line) renders correctly and identically in both SDL1 and SDL2 builds — capture screenshots and compare visually
- [x] `pushClip` / `popClip` correctly confines drawing: anything drawn outside the clip rect must not appear on screen
- [x] Nested clips (push A, push B, pop B, pop A) restore the outer clip region correctly
- [x] `drawTextEllipsis` with a string that fits within `maxWidth` renders the full string without a `…` suffix
- [x] `drawTextEllipsis` with a string that overflows renders a truncated string ending in `…` that fits within `maxWidth`
- [x] `drawTextEllipsis` handles a multi-byte UTF-8 character (e.g. `é`, `中`) without crashing or corrupting output
- [x] `setGlobalAlpha(0)` makes all subsequent draw calls invisible; `setGlobalAlpha(255)` restores full opacity
- [x] `loadTexture` with a non-existent path returns handle `0` (the null handle) and does not crash
- [x] No SDL header (`SDL.h`, `SDL2/SDL.h`, etc.) appears anywhere under `include/hui/` (grep check)

---

## Phase 4 — Widget Base Class & Focus Manager

- [x] Implement `Widget` base class in `include/hui/Widget.h` (§6.1)
  - [x] `update(float dt)`, `onButtonDown()`, `onButtonUp()` virtuals
  - [x] `virtual void layout(Rect r)` setting protected `bounds_`; `bounds()` accessor
  - [x] `draw(IRenderer&, const Theme&)` without the `Rect` parameter; widgets draw inside `bounds_`
  - [x] `virtual bool isFocusable() const` defaulting to `false`
  - [x] `onFocus()`, `onBlur()` virtuals; `isFocused()`, `isDisabled()`, `setDisabled()` accessors
  - [x] Private `focused_` / `disabled_` fields
  - [x] Split the fused setter into `setFocusedFlag(bool)` (flag only) and `setFocusedAndNotify(bool)` (flag + callback), both befriended to `FocusManager`
- [x] Implement `FocusManager` in `include/hui/FocusManager.h` (§7.1)
  - [x] `setFocus(Widget*)` returns `bool` and refuses a non-null widget when `!isFocusable()` or `isDisabled()`, leaving focus unchanged; `setFocus(nullptr)` always succeeds
  - [x] `setFocus`: calls `onBlur` on previous, `onFocus` on new; no-op if already focused
  - [x] `focused()`, `hasFocus()` accessors
  - [x] `forceOwner(Widget*)`: updates the `focused_` flag on both the outgoing and incoming widget via `setFocusedFlag()` while skipping `onFocus`/`onBlur`; returns `bool`, same refusal rules as `setFocus`
- [x] Implement `NavList` in `include/hui/NavList.h` (§7.3)
  - [x] `setAxis(Axis)`, `setWrap(bool)`, `add(Widget*)`, `clear()`
  - [x] `handleButton(Button, FocusManager&)`: consumes only the buttons on its axis; skips `!isFocusable()` and `isDisabled()` entries; wraps; returns `false` for off-axis buttons and for an empty list
  - [x] `focusIndex(int, FocusManager&)`, `index()`, `current()`

### ✅ QA Sign-off — Phase 4

> **Testing team:** confirm all items below before marking this phase done.

- [x] `setFocus(A)` followed by `setFocus(B)`: widget A receives exactly one `onBlur` call and widget B receives exactly one `onFocus` call
- [x] Calling `setFocus` on the widget that already has focus does not fire `onBlur` or `onFocus` a second time
- [x] `setFocus()` on a widget whose `isFocusable()` is `false` returns `false` and leaves `focused()` unchanged
- [x] `setFocus()` on a widget with `setDisabled(true)` returns `false` and leaves `focused()` unchanged
- [x] `layout({10,20,100,50})` then `bounds()` round-trips exactly; a widget that never had `layout()` called reports a zero rect rather than garbage
- [x] `NavList` with `Axis::Vertical`: Down advances, Up retreats, both wrap; Left/Right return `false` and are left for the caller
- [x] `NavList` with `Axis::Horizontal` and `setWrap(false)`: Right at the last entry returns `true` without moving (or `false`, whichever the implementation documents — the point is that it must not wrap)
- [x] `NavList` skips a disabled middle entry entirely in both directions
- [x] `NavList` with every entry disabled returns `false` and does not infinite-loop
- [x] `setFocus(nullptr)` calls `onBlur` on the current owner and leaves `focused()` returning `nullptr`
- [x] `forceOwner(W)` sets `focused()` to W and `W->isFocused()` to `true`, clears the previous owner's `isFocused()`, and fires zero `onFocus`/`onBlur` callbacks. The flag update is the part the earlier revision got wrong — a widget restored via `forceOwner` must render highlighted.
- [x] `isFocused()` returns `true` only while the widget is the current owner; returns `false` after it is blurred
- [x] `setDisabled(true)` + `isDisabled()` round-trips correctly; `setDisabled(false)` restores the original state

---

## Phase 5 — View System & View Stack

- [x] Implement `View` base class in `include/hui/View.h` (§8.1)
  - [x] Lifecycle hooks: `onPush()`, `onPop()`, `onResume()`, `onSuspend()`
  - [x] `update(float dt, FocusManager&)`, `draw(IRenderer&, const Theme&)` virtuals
  - [x] `onButtonDown()`, `onButtonUp()` virtuals returning `bool`
  - [x] Remove `setDimmed()` / `isDimmed()` / `dimmed_`; replace with `virtual bool dimsBelow() const { return false; }`
  - [x] `virtual void layout(Rect contentRect)` setting protected `bounds_`
  - [x] `currentHints()` returning `std::vector<HintEntry>`
  - [x] `suspendFocus(FocusManager&)` recording `savedFocus_`
  - [x] `suspendFocus()` must also call `fm.setFocus(nullptr)` so the outgoing widget receives `onBlur()`
  - [x] Make `restoreFocus(FocusManager&)` virtual; default implementation calls `fm.forceOwner(savedFocus_)`
- [x] Implement `ViewStack` in `src/ViewStack.cpp` (§8.2)
  - [x] `push()` / `pop()` / `popTo<T>()` / `replace()` enqueue a mutation instead of applying it immediately
  - [x] `applyPendingMutations(FocusManager&)` returns `bool` (stack changed); runs the documented lifecycle order; calls `fm.setFocus(nullptr)` before destroying any view; calls `layout(contentRect_)` on a newly pushed view before its first update/draw
  - [x] `hasPendingMutations()`
  - [x] `setContentRect(Rect)` / `contentRect()`; setting it re-runs `layout()` on every stacked view
  - [x] `popTo<T>()` template: pops until the correct type is on top
  - [x] `replace()`: atomic pop+push
  - [x] `update()`: drives all views in the stack
  - [x] `draw()`: renders bottom-to-top; before any view whose `dimsBelow()` is `true`, fills the whole screen once with `theme.overlay`. Remove all `setGlobalAlpha` use from `ViewStack` entirely.
  - [x] `dispatchButtonDown()` / `dispatchButtonUp()`: routes to `top()` only
  - [x] `depth()` accessor

### ✅ QA Sign-off — Phase 5

> **Testing team:** confirm all items below before marking this phase done.

- [x] `push(B)` while A is on top: A's `onSuspend` fires before B's `onPush`; confirm call order with instrumented stubs
- [x] `pop()` while B is on top of A: B's `onPop` fires, then A's `onResume`; confirm call order
- [x] `pop()` on a stack with exactly one view is a no-op: the view is not removed and no lifecycle hook fires
- [x] `dispatchButtonDown` delivers the event only to the top view; a view below the top must not receive it (instrument both views)
- [x] With an ordinary (non-modal) view pushed on top, nothing dims: `grep` confirms `ViewStack` makes no `setGlobalAlpha` call, and the lower view renders at full brightness
- [x] With a view whose `dimsBelow()` is `true` on top, exactly one full-screen `fillRect(theme.overlay)` is issued, immediately before that view is drawn (instrument the renderer and count calls)
- [x] Stack depth 3 with two modal views: exactly two scrims, each immediately before its own view; no scrim before the base view
- [x] `suspendFocus()` records the focused widget
- [x] `suspendFocus()` also fires exactly one `onBlur()` on the outgoing widget and leaves `focused()` as `nullptr`
- [x] `restoreFocus()` returns focus to that exact widget via `forceOwner`, and the widget's `isFocused()` is `true` afterwards
- [x] A view that overrides `restoreFocus()` gets its override honoured instead of the `savedFocus_` default
- [x] Deferred pop safety: a view that calls `pop()` from inside its own `onButtonDown` is not destroyed before that handler returns; the handler's return value is read correctly. Verify under a sanitizer build (ASan) — this is the use-after-free the deferral exists to prevent.
- [x] After a pop, `focusManager().focused()` never points into the destroyed view; verify with ASan and with a pointer-identity check against the surviving view's widgets
- [x] `push()` calls `layout(contentRect())` on the new view before its first `update()` or `draw()`; a view that asserts on a zero-sized `bounds_` in `draw()` does not fire
- [x] `setContentRect()` with a new rect re-runs `layout()` on every view on the stack, not just the top

---

## Phase 5.5 — Revision 2 Retrofit

> Retrofit completed. All revision-2 changes from `DESIGN.md` are applied.

### ✅ QA Sign-off — Phase 5.5

- [x] Both SDL1 and SDL2 configurations build clean at `-Wall -Wextra` after the retrofit
- [x] Every re-opened Phase 4 and Phase 5 QA item above is now checked
- [x] `grep -rn "setGlobalAlpha" src/ViewStack.cpp` returns nothing
- [x] `grep -rn "setDimmed\|isDimmed\|TransitionKind\|SimpleTransition" src/ include/hui/` returns nothing
- [x] ASan build: push and pop 200 views in a loop, with self-popping overlays, with no reported error

---

## Phase 6 — Input System

- [x] Implement `KeyRepeatDriver` in `src/KeyRepeatDriver.cpp` (§9.2)
  - [x] `onButtonDown()` / `onButtonUp()` to record/clear held state
  - [x] `update(float dt, Sink&&)` template: advances timers, fires synthetic `ButtonEvent`s
  - [x] Timing constants: `kInitialDelay = 0.3s`, `kRepeatInterval = 0.1s`, `kFastInterval = 0.03s`, `kFastThreshold = 1.0s`
  - [x] `shouldRepeat()`: true for directional and shoulder buttons only; face buttons and Start/Select/Guide do not repeat
  - [x] Synthetic events must set `ButtonEvent::synthetic = true`
  - [x] Synthetic `Down` **only** — never fabricate a matching `Up` (§9.2)
  - [x] `flushHeld()`: drops all held state without emitting anything
- [x] Implement `ChordDetector` in `src/ChordDetector.cpp` (§9.4)
  - [x] `addChord({Start, Select}, Guide)` as the default registration
  - [x] `onButtonDown` / `onButtonUp` return the substituted button, or the original when no chord completes
  - [x] `kChordWindow = 0.150f`; individual inputs are suppressed when the chord fires

### ✅ QA Sign-off — Phase 6

> **Testing team:** confirm all items below before marking this phase done.

- [x] Holding `Button::Down` for 0.25 s fires zero synthetic events (still within the 300 ms initial delay)
- [x] Holding `Button::Down` for 0.35 s fires exactly one synthetic repeat
- [x] After the first repeat, subsequent repeats arrive at ~100 ms intervals; measure at least five consecutive repeats and confirm none deviate by more than 10 ms
- [x] After holding for more than 1.0 s, the interval drops to ~30 ms; measure at least five consecutive fast repeats
- [x] Releasing and immediately re-pressing a button resets the initial delay countdown from zero
- [x] Holding `Button::A`, `Button::B`, `Button::X`, `Button::Y`, `Button::Start`, `Button::Select`, and `Button::Guide` for 2 s produces zero synthetic repeat events for each
- [x] Every synthetic event has `ButtonEvent::synthetic == true`; real hardware events always have `synthetic == false`
- [x] No synthetic `ButtonEventKind::Up` is ever emitted, for any button, under any hold duration
- [x] `flushHeld()` while `Down` is held stops all further repeats; a subsequent real `Up` for that button is harmless (no crash, no spurious event)
- [x] Pressing Start then Select within 150 ms emits a single `Button::Guide` and **neither** `Start` nor `Select` individually
- [x] Pressing Start alone and waiting past the window emits `Start` (delayed by at most `kChordWindow`), not `Guide`
- [x] `update()` with a `dt` of 1200.0 s (simulating a wake from screen-off) emits **at most a handful** of repeats, not tens of thousands — verify the `UISystem` clamp is in the path (§10.1)

---

## Phase 7 — UISystem

- [x] Implement `UISystem` in `src/UISystem.cpp` (§10)
  - [x] Constructor takes `IRenderer&` and `const Theme&` by reference; owns `ViewStack`, `FocusManager`, `KeyRepeatDriver`
  - [x] `onButtonDown()`: feeds `KeyRepeatDriver`, then dispatches to `ViewStack`
  - [x] `onButtonUp()`: feeds `KeyRepeatDriver`
  - [x] `update(float dt)`: drives `KeyRepeatDriver` (which injects synthetic repeats into `ViewStack`), then calls `ViewStack::update()`
  - [x] `draw()`: calls `ViewStack::draw()`
  - [x] `viewStack()` and `focusManager()` accessors
  - [x] `setAnimationsEnabled(bool)` (§13.6); propagated to views/widgets that animate
  - [x] Clamp incoming `elapsedSeconds` to `kMaxDelta = 0.100f` before anything downstream sees it (§10.1)
  - [x] Call `ViewStack::applyPendingMutations()` at the top of `update()` **and** again after event dispatch unwinds; call `KeyRepeatDriver::flushHeld()` whenever it reports a change
  - [x] Route input through `ChordDetector` before `KeyRepeatDriver` (§9.4)
  - [x] `setSuspended(bool)` / `isSuspended()` (§10.1): first two `draw()` calls after suspending clear to black and present (clearing both double-buffers), subsequent `draw()` calls are no-ops; `onButtonDown` records held state but does not dispatch; `flushHeld()` on both suspend and resume
  - [x] `setShell(Shell*)` / `shell()`; `draw()` order is chrome → view stack → Shell overlay layer (§12)

### ✅ QA Sign-off — Phase 7

> **Testing team:** confirm all items below before marking this phase done.

- [x] A real `onButtonDown(Button::A)` call reaches the top view's `onButtonDown` within the same call frame (no buffering)
- [x] Synthetic repeats generated inside `update()` also reach the top view's `onButtonDown` during that same `update()` call
- [x] `onButtonUp` stops all repeats for the released button; no further synthetic events arrive in subsequent `update()` calls
- [x] `setAnimationsEnabled(false)` is confirmed to propagate: a `GuideOverlayView` stub (or the real one if already implemented) opens instantly instead of animating
- [x] Two independent `UISystem` instances can be driven simultaneously without sharing state; driving one does not affect the other
- [x] `update(1200.0f)` produces the same observable behaviour as `update(0.1f)`: confirm the clamp with an instrumented `KeyRepeatDriver` sink and count the synthetic events
- [x] A view that calls `viewStack().pop()` from `onButtonDown` is destroyed during the *following* `applyPendingMutations()`, not inside the handler
- [x] A synthetic repeat that triggers a push is applied before `ViewStack::update()` runs in that same frame
- [x] Holding `Button::Down`, then pushing an overlay: the overlay receives **zero** synthetic repeats from the still-held button
- [x] `setSuspended(true)`: the first two `draw()` calls issue a black clear plus present; the next ten `draw()` calls issue no renderer commands at all (instrument the renderer)
- [x] While suspended, `onButtonDown(Button::A)` does not reach the top view; after `setSuspended(false)` the next press does
- [x] With no `Shell` registered (`setShell(nullptr)`), `draw()` works and draws only the view stack

---

## Phase 8 — SDL Helper Layer

- [ ] Define `ButtonMapping` struct in `include/hui/sdl/ButtonMapping.h` (§11.1)
  - [ ] `controllerButtons` array: SDL button index → `std::optional<Button>`
  - [ ] `AxisBinding` struct: axis, positive button, negative button, threshold
  - [ ] `axisBindings` vector
  - [ ] `defaultXboxLayout()` and `defaultNintendoLayout()` static factories
  - [ ] `buttonFromName(std::string_view)` free function for config-file parsing
- [ ] Implement `SDLGamepadHelper` in `src/sdl/SDLGamepadHelper.cpp` (§11.2)
  - [ ] `openController(int deviceIndex)` / `closeController()`
  - [ ] `translate(const SDL_Event&)` → `std::optional<ButtonEvent>`; handles `SDL_CONTROLLERBUTTONDOWN/UP`, `SDL_CONTROLLERAXISMOTION` with hysteresis
- [ ] Implement keyboard fallback connector in `src/sdl/KeyboardFallback.cpp` (§11, §12), guarded by `HUI_ENABLE_KEYBOARD_FALLBACK`
  - [ ] Arrow keys → D-pad, Z/X → A/B (or similar); document the mapping

### ✅ QA Sign-off — Phase 8

> **Testing team:** confirm all items below before marking this phase done.

- [ ] `defaultXboxLayout()`: verify A, B, X, Y, D-pad Up/Down/Left/Right, L1, L2, R1, R2, Start, Select, Guide each map to the correct `hui::Button` (check against documented SDL controller button indices)
- [ ] `defaultNintendoLayout()`: verify A and B are swapped relative to the Xbox layout; all other buttons identical
- [ ] Axis binding: synthesise an `SDL_CONTROLLERAXISMOTION` event with value `+20000` (above the 16384 threshold); confirm the positive `hui::Button` fires
- [ ] Axis hysteresis: synthesise a second event at `+20000` without crossing back through zero; confirm no second `ButtonEvent` is emitted
- [ ] Axis release: synthesise an event at `+100` (below threshold) after a positive trigger; confirm the corresponding `ButtonUp` fires
- [ ] `buttonFromName("Up")` → `Button::Up`; `buttonFromName("L2")` → `Button::L2`; an unknown name returns a defined error state (nullopt or assertion)
- [ ] Keyboard fallback (build with `HUI_ENABLE_KEYBOARD_FALLBACK=ON`): press each mapped key and confirm the correct `hui::Button` event is delivered; confirm the full mapping is documented in a comment or README section

---

## Phase 9 — Text & Rendering Helpers

- [ ] Implement `leftTruncate(std::string_view, FontHandle, int maxWidth, IRenderer&)` free function returning `"…/rest/of/path"` (§13.1)
- [ ] Implement `hueToColor(float hue)` (HSV S=0.5, V=0.7) and `labelHash(std::string_view)` helpers for gradient placeholders (§13.2)
- [ ] Implement `buttonGlyphColor(std::string_view buttonLabel, const Theme&)` for hint bar color coding (§13.5)

### ✅ QA Sign-off — Phase 9

> **Testing team:** confirm all items below before marking this phase done.

- [ ] `leftTruncate` with a string that already fits within `maxWidth` returns it unchanged (no `…` prefix added)
- [ ] `leftTruncate` with a long path (e.g. `/home/user/music/rock/album/track.flac`) returns a string beginning with `…/` whose rendered pixel width is ≤ `maxWidth`
- [ ] `leftTruncate` result always preserves the rightmost path component intact (the filename is never cut mid-word)
- [ ] `hueToColor(0.0f)` returns a recognisably red-tinted color; `hueToColor(0.33f)` green; `hueToColor(0.67f)` blue
- [ ] `labelHash("alpha") != labelHash("beta")` and `labelHash("alpha") == labelHash("alpha")` (deterministic, not trivially constant)
- [ ] `buttonGlyphColor("A", theme)` → `{220,50,50,255}`; `"B"` → `{220,160,40,255}`; `"X"` → `{60,120,220,255}`; `"Y"` → `{60,180,80,255}`; any other string → `theme.textSecondary`

---

## Phase 10 — Level 1 Atoms (Widgets)

Implement each as a concrete `Widget` subclass. Verify each draws correctly in the example app before moving on.

- [ ] Define `ListItemVariant`, `RowData`, and the `IListSource` interface in `include/hui/ListSource.h` (§6.5)
- [ ] Implement `VectorListSource` — owning convenience adapter for short static lists (context menu, guide items) (§6.5)
- [ ] `ListItemWidget` — single row: icon, primary label (`drawTextEllipsis`), secondary label, right meta; states: default / focused / playing / disabled; variants: default, track, folder, playlist (§12)
  - [ ] `setRow(const RowData&)` and `setRowFocused(bool)` — this widget is a **stamp**: one instance is reused for every visible row by the owning container, so it must hold no per-row state beyond what `setRow` assigns (§6.2, §6.5)
  - [ ] `isFocusable()` returns `false` — the row highlight is driven by the container, not by `FocusManager`
- [ ] `GridCellWidget` — tile: thumbnail texture or gradient placeholder (using `hueToColor` + `labelHash`), label, sublabel; focused border; playing badge (§12, §13.2)
  - [ ] Same stamp contract as `ListItemWidget`
- [ ] `ProgressBar` — read-only horizontal fill bar; elapsed and total timestamp labels (§12)
- [ ] `Slider` — focusable horizontal value control; Left/Right buttons change value; calls `onValueChanged` callback (§12)
  - [ ] `isFocusable()` returns `true`; step size configurable (the guide overlay uses 5)
- [ ] `SortModeIndicator` — non-focusable badge; renders current sort mode label (§12)
- [ ] `ShuffleToggle` — non-focusable icon; on/off visual state (§12)
- [ ] `RepeatModeToggle` — non-focusable icon; cycles off → all → one (§12)

### ✅ QA Sign-off — Phase 10

> **Testing team:** confirm all items below before marking this phase done.

- [ ] `ListItemWidget` renders all four variants (default, track, folder, playlist) without visual corruption or crash
- [ ] `ListItemWidget` focused state shows the focus border and accent fill tint
- [ ] `ListItemWidget` disabled state renders the label in `theme.textDisabled` and does not respond to button events
- [ ] `ListItemWidget` with a very long primary label truncates with `…` and does not overflow its bounding rect
- [ ] `GridCellWidget` with a null texture handle renders a gradient placeholder (not a black tile, not a crash)
- [ ] Two `GridCellWidget` instances with different label strings show visually distinct placeholder gradients
- [ ] `GridCellWidget` focused state renders the focus border
- [ ] `ProgressBar` at 0%, 50%, and 100% fill all render correctly; elapsed/total labels are visible and accurate
- [ ] `Slider` value increases on `Button::Right` and decreases on `Button::Left`; `onValueChanged` callback fires on each change
- [ ] `Slider` does not respond to input when `setDisabled(true)`
- [ ] `RepeatModeToggle` cycles through exactly three states (off → all → one → off) on successive activations; never skips or wraps incorrectly
- [ ] `ListItemWidget` used as a stamp: calling `setRow()` / `layout()` / `draw()` twenty times in a row with different `RowData` produces twenty correct rows with **no** state leaking between them (e.g. a `playing` row does not leave the next row accent-colored)
- [ ] `ListItemWidget` and `GridCellWidget` both report `isFocusable() == false`, so `FocusManager::setFocus()` on one is refused
- [ ] `RowData` string views pointing into a caller-owned buffer render correctly, and the widget copies no strings (inspect for allocation — no `std::string` members holding row text)
- [ ] `VectorListSource`: `add()` three entries, `rowCount() == 3`, `rowAt()` fills views that remain valid until `clear()`

---

## Phase 11 — Level 2 Molecules (Widgets)

- [ ] `ListHeaderWidget` — non-focusable context row: icon, label (left-truncated using `leftTruncate`), item count, sort badge (§12)
- [ ] `SeekableProgressBar` — extends `ProgressBar`; consumes L2/R2 events; calls `onSeek` callback (§12)
- [ ] `PlaybackControlsRow` — non-focusable row of transport icons reflecting playback state passed in (§12)
- [ ] `HintBarWidget` — non-focusable bottom bar; reads `ViewStack::top()->currentHints()` each `draw()` call; renders button glyphs with `buttonGlyphColor()` (§12, §13.4, §13.5)
- [ ] `StatusBarWidget` — non-focusable top bar: view mode label, context label, now-playing pulse indicator (animated via `update()`), clock, battery (§12)
- [ ] `ToastNotification` — non-focusable; self-timed via `update()`; auto-dismiss with fade-out animation; replacement policy (new toast replaces old, no stacking) (§12, §6.4)

### ✅ QA Sign-off — Phase 11

> **Testing team:** confirm all items below before marking this phase done.

- [ ] `ListHeaderWidget` with a long filesystem path renders a left-truncated label beginning with `…/` that does not overflow its bounds
- [ ] `SeekableProgressBar` calls `onSeek` when L2 or R2 is pressed, and does not call it for any other button
- [ ] `SeekableProgressBar` returns `false` from `onButtonDown` for non-seek buttons, allowing them to propagate
- [ ] `HintBarWidget` renders face-button glyphs in the correct colors: A red, B yellow/orange, X blue, Y green
- [ ] `HintBarWidget` non-button labels (e.g. `"L1/R1"`, `"START"`) render in `theme.textSecondary`
- [ ] `StatusBarWidget` now-playing pulse indicator changes visual state across frames when `update(dt)` is driven with nonzero `dt`
- [ ] `PlaybackControlsRow` visually reflects play, pause, and stop states when its playback-state input changes
- [ ] `ToastNotification` auto-dismisses after its configured timeout; confirm with a timer that the widget is no longer drawing after the timeout elapses
- [ ] Firing a second toast while the first is still visible replaces it immediately; only one toast is ever on screen at a time

---

## Phase 12 — Level 3 Organisms (Container Widgets & Overlay Views)

- [ ] `ListView` — scrollable vertical list (§12)
  - [ ] Backed by `IListSource*`, not by an owned item vector (§6.5)
  - [ ] Owns **one** `ListItemWidget` stamp, refilled per visible row (§6.2)
  - [ ] `layout()` computes `pageRows_ = max(1, bounds_.h / itemHeight_)` and reclamps scroll (§13.3)
  - [ ] Draws **only the visible window** `[first, last]`, with a single `pushClip(bounds_)` for the whole viewport — not one clip per row (§6.2)
  - [ ] D-pad Up/Down moves focus; key-repeat consumed from `onButtonDown`
  - [ ] L1/R1 page jump by `pageRows_ * itemHeight_` (whole rows, no sub-row drift); L2/R2 jump to first/last item
  - [ ] Wrap-around at top and bottom; wrap **snaps** the scroll offset rather than travelling through it (§13.3)
  - [ ] Scroll-to-focus using the bottom-third / top-third formula, with `maxScroll = max(0, totalContentHeight - bounds_.h)` — the floor at 0 is required, not defensive (§13.3)
  - [ ] `getFocusIndex()` / `setFocusIndex(int, bool scrollToIt)` for focus memory; does not reset on `onBlur()`
  - [ ] `notifyRowsChanged()`: clamps focus index, reclamps scroll, invalidates the text measurement cache, does **not** move focus (§6.5.1)
  - [ ] Scrolling does **not** invalidate the text cache (§13.1)
  - [ ] `ListHeaderWidget` is pinned above the list, outside `bounds_` — the header does not scroll and is not row −1 (§13.3)
  - [ ] Empty state rendering (placeholder message)
  - [ ] Scroll indicator (right-side bar)
- [ ] `GridView` — 2D focusable grid; same source / stamp / window / scroll / wrap / page / memory rules as `ListView`, with a row of cells as the scroll unit (§12)
- [ ] `TabBarWidget` — horizontal tab strip; L1/R1 to switch tabs; not reachable via D-pad Up/Down; calls `onTabChanged` callback (§12)
- [ ] `QueueList` — extends `ListView` with grab-mode reorder: Y to grab, Up/Down to reorder, A to drop, B to cancel (§12)
- [ ] `LetterWheel` — character strip + results list; internal focus routing between strip and list (§12)
- [ ] `OnScreenKeyboard` — 2D QWERTY grid widget; Confirm / Cancel / Backspace keys; `onCommit` and `onCancel` callbacks (§12)
> **All four overlays below:** `dimsBelow()` returns `true`, and none of them draws its own
> full-screen fill — `ViewStack` lays down the single scrim (§8.2, §8.3). Drawing both
> double-darkens the background. Each pops itself from its own handler, which is safe only
> because `ViewStack` mutations are deferred (§8.2).

- [ ] `ContextMenuView` — overlay `View`; backed by a `VectorListSource`; action list with destructive color support; B cancels (§12)
- [ ] `ConfirmationDialogView` — overlay `View`; `NavList` with `Axis::Horizontal`, wrap off, default index 0 (Cancel); A activates the focused option, B cancels from either (§12, §7.3)
- [ ] `TrackInfoPanelView` — overlay `View`; read-only metadata table (§12)
- [ ] `GuideOverlayView` — overlay `View` sliding in from right; `NavList` with `Axis::Vertical` over both `Slider`s and the action items as one unified focus list; Left/Right falls through `NavList` to the focused `Slider` and is ignored on action items; animation degrades to instant when `setAnimationsEnabled(false)` (§12, §7.3, §13.6)

### ✅ QA Sign-off — Phase 12

> **Testing team:** confirm all items below before marking this phase done.

- [ ] `ListView` Up/Down moves focus one item at a time; wrap-around from the last item lands on the first and vice versa
- [ ] `ListView` L1/R1 jumps exactly one visible page; L2 jumps to index 0; R2 jumps to the last index
- [ ] `ListView` scroll-to-focus: after moving focus down past the bottom third of the viewport, the list scrolls so the focused item remains visible; same behaviour moving upward past the top third
- [ ] `ListView` blur then re-focus: `getFocusIndex()` returns the same index it had before blur; the list does not scroll back to the top
- [ ] `ListView` with zero items renders a non-empty placeholder message and does not crash
- [ ] `GridView` Left/Right/Up/Down navigate the grid; wrap-around at row and column edges behaves consistently with `ListView`
- [ ] `TabBarWidget` L1/R1 cycles through tabs and calls `onTabChanged`; pressing D-pad Up/Down while the tab bar is focused does not move focus away from it
- [ ] `QueueList` grab mode: press Y to grab an item; Up/Down reorders it; A drops it in the new position; B cancels and the item returns to its original index
- [ ] `ContextMenuView` shows the overlay background fill and the action list; pressing B dismisses it; a destructive action renders in `theme.warning` color
- [ ] `ConfirmationDialogView` opens with focus on Cancel; Left/Right moves focus between Cancel and Confirm; A on Confirm confirms; B cancels from either button
- [ ] `ConfirmationDialogView` Left at Cancel and Right at Confirm do **not** wrap
- [ ] `GuideOverlayView` Up/Down traverses sliders and action items as one list; Left/Right on a slider adjusts by 5; Left/Right on an action item does nothing and does not move focus
- [ ] Each overlay draws no full-screen fill of its own: with one modal open, the instrumented renderer sees exactly **one** full-screen `fillRect`
- [ ] `ListView` with 5,000 rows: the instrumented renderer sees at most `pageRows_ + 2` row draws per frame, and exactly one `pushClip`/`popClip` pair for the list body
- [ ] `ListView` scrolling continuously for 5 s: the text measurement cache hit rate stays high (instrument it) — a rate near zero means the cache is keyed on position rather than content (§13.1)
- [ ] `ListView` with a list shorter than the viewport: `scrollOffset_` stays 0 and no undefined-behaviour clamp occurs (build with UBSan)
- [ ] `ListView` insert-while-suspended: focus index 7, insert a row at index 3, `notifyRowsChanged()` — focus stays at index 7 (now a different row) and nothing crashes; `setFocusIndex(3)` then highlights the inserted row
- [ ] `GuideOverlayView` slides in from the right under SDL2; with `setAnimationsEnabled(false)` it appears instantly with no intermediate animation frames
- [ ] `OnScreenKeyboard` Backspace deletes the last typed character; Confirm calls `onCommit` with the full typed string; Cancel calls `onCancel` without modifying the string

---

## Phase 13 — Level 4 Screens (Application Views)

- [ ] `Shell` — permanent chrome. **A composite `Widget`, NOT a `View`, and never pushed onto the ViewStack** (§12)
  - [ ] Owns `StatusBarWidget`, `HintBarWidget`, optional `TabBarWidget`, and the `ToastNotification`
  - [ ] Holds a `ViewStack&`; the hint bar reads `top()->currentHints()` each frame (§13.4)
  - [ ] `layout(Rect screen)` partitions the chrome, computes `contentRect_`, and calls `stack_.setContentRect(contentRect_)`
  - [ ] `drawChrome()` — drawn **beneath** the view stack so a modal scrim covers it
  - [ ] `drawOverlay()` — the toast, drawn **above** everything and never dimmed
  - [ ] `showToast(std::string_view, float seconds)`; replacement policy, no stacking
- [ ] `DirectoryView` — uses `ListView` + pinned `ListHeaderWidget`; pushes `ContextMenuView` and `TrackInfoPanelView`; raises toasts via `Shell::showToast()` (the toast is **not** pushed onto the stack) (§12)
  - [ ] Provides an `IListSource` over the application's directory listing; copies no row strings (§6.5)
- [ ] `LibraryView` — uses `ListView` + `GridView` + `TabBarWidget`; pushes `ContextMenuView`, `LetterWheel`, `TrackInfoPanelView` (§12)
- [ ] `NowPlayingView` — uses `SeekableProgressBar`, `PlaybackControlsRow`, `QueueList`; pushes `ContextMenuView`, `TrackInfoPanelView` (§12)

### ✅ QA Sign-off — Phase 13

> **Testing team:** confirm all items below before marking this phase done.

- [ ] `Shell` content area rect does not overlap the `StatusBarWidget` (top) or `HintBarWidget` (bottom) under any screen resolution; verify at both 480×320 and 640×480
- [ ] `Shell::layout()` propagates the content rect through `ViewStack::setContentRect()` to every stacked view
- [ ] During ordinary navigation (no modal open) the status bar and hint bar are at **full brightness** — confirm the regression that motivated moving `Shell` off the stack is gone
- [ ] With a `ConfirmationDialogView` open, the status bar and hint bar **are** dimmed along with the content, and the toast (if visible) is **not**
- [ ] `HintBarWidget` caps at five hints, truncating from the middle, with A and B always retained (§13.4)
- [ ] `HintBarWidget` renders hints in the mandated order regardless of the order the view returns them in
- [ ] `HintBarWidget` displays the overlay's hints (not the base view's) immediately after a `ContextMenuView` is pushed onto the stack
- [ ] `HintBarWidget` reverts to the base view's hints immediately after the overlay is popped
- [ ] `DirectoryView`: open context menu, press B to cancel, confirm focus returns to the exact list item that was focused before the menu opened
- [ ] `LibraryView`: L1/R1 switches between list and grid tabs; the content area updates to the correct widget and focus memory is preserved independently per tab
- [ ] `NowPlayingView`: L2/R2 calls `onSeek`; `PlaybackControlsRow` visually updates when the playback state changes externally (simulated state change)
- [ ] All three views handle an empty data set gracefully (no items in directory, no library tracks, empty queue) without crashing or showing corrupt layout

---

## Phase 14 — Example Application

- [ ] Wire up the SDL main loop in `example/main.cpp` following the pattern in §10
- [ ] Instantiate the appropriate renderer (`SDL2Renderer` or `SDL1Renderer`) based on build configuration
- [ ] Define a default `Theme` with sensible colors and load fonts via the renderer
- [ ] Instantiate `UISystem`; push an initial `Shell` wrapping a `DirectoryView`
- [ ] Connect `SDLGamepadHelper` (and `KeyboardFallback` if enabled) to `UISystem::onButtonDown/Up`
- [ ] Populate the `DirectoryView` with sample/mock data sufficient to exercise all widget states
- [ ] Verify `LibraryView` and `NowPlayingView` are reachable via navigation
- [ ] Confirm the example builds and runs correctly under both SDL1 and SDL2 backends

### ✅ QA Sign-off — Phase 14

> **Testing team:** confirm all items below before marking this phase done.

- [ ] Example builds and runs with `-DHUI_USE_SDL1=OFF` (SDL2 mode); no runtime errors or visual corruption
- [ ] Example builds and runs with `-DHUI_USE_SDL1=ON` (SDL1 mode); visually equivalent output to SDL2 for static frames
- [ ] Keyboard fallback build (`HUI_ENABLE_KEYBOARD_FALLBACK=ON`): all three views are fully navigable using keyboard alone; each key in the documented mapping produces the correct action
- [ ] Rapid button-mashing (hold multiple keys simultaneously for 10 s) does not crash or visibly corrupt the UI
- [ ] Navigate from `DirectoryView` → `NowPlayingView` → back to `DirectoryView`; confirm focus is restored to the same list item that was focused before leaving
- [ ] `LibraryView` grid and list tabs both display mock data; switching tabs preserves the scroll position and focus index of each tab independently
- [ ] Sample data includes at least one item in the focused state, one in the playing state, and one in the disabled state; all three visual variants are exercised in a single session

---

## Phase 15 — Polish & Correctness

- [ ] Audit all `onButtonDown` return values: ensure every consumed event returns `true` and no event is silently dropped
- [ ] Verify focus memory: blur and re-focus `ListView` and `GridView` instances to confirm `getFocusIndex()` is preserved
- [ ] Test overlay push/pop cycle: open `ContextMenuView` from `DirectoryView`, cancel, confirm focus returns to the correct list item
- [ ] Test `ToastNotification` replacement policy: fire two toasts in quick succession; only the second should be visible
- [ ] Test `KeyRepeatDriver` timing: hold Up on a list; confirm initial delay, repeat rate, and fast-repeat threshold all fire at the correct cadence
- [ ] Profile on the A53 SDL1 target (or emulate): confirm full-frame blit budget, no per-frame string re-rendering (§13.6)
- [ ] Verify no SDL headers are included from any file in `include/hui/` (only `src/renderer/` touches SDL)
- [ ] Verify no `dynamic_cast`, no RTTI usage, no exceptions thrown anywhere in the library (§13.8)
- [ ] Confirm `MIGRATION.md` has been deleted and no `↺ rev2` marker remains in this file or in `DESIGN.md`
- [ ] Confirm no allocation occurs per frame during list scrolling (hook the allocator and scroll a 5,000-row list for 10 s)
- [ ] Check all fallible methods return `bool` or `std::optional`; no silent failure paths

### ✅ QA Sign-off — Phase 15

> **Testing team:** this is the final gate. All prior phase sign-offs must be complete before starting here.

- [ ] Full end-to-end session on physical or emulated A53 hardware running SDL1 at 480×320: navigate all three views, open every overlay type, trigger a toast, use `GuideOverlayView` sliders — no frame drops below 25 fps and no crashes
- [ ] Full end-to-end session on SDL2 desktop at 640×480: same scenario as above at a stable 60 fps
- [ ] `grep -r "SDL" include/hui/` returns no results
- [ ] `grep -rE "dynamic_cast|typeid" src/ include/hui/` returns no results
- [ ] `grep -r "throw " src/ include/hui/` returns no results
- [ ] Static analysis (clang-tidy or cppcheck) run on the full library source with no new errors introduced in this phase
- [ ] All prior QA sign-off checkboxes across Phases 1–14 are confirmed checked