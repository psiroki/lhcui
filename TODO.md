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
  - [x] `update(float dt)`, `draw()`, `onButtonDown()`, `onButtonUp()` virtuals
  - [x] `onFocus()`, `onBlur()` virtuals; `isFocused()`, `isDisabled()`, `setDisabled()` accessors
  - [x] Private `focused_` / `disabled_` fields; `setFocused()` befriended to `FocusManager`
- [x] Implement `FocusManager` in `include/hui/FocusManager.h` (§7.1)
  - [x] `setFocus(Widget*)`: calls `onBlur` on previous, `onFocus` on new
  - [x] `focused()`, `hasFocus()` accessors
  - [x] `forceOwner(Widget*)`: sets current without lifecycle callbacks (used by ViewStack on resume)

### ✅ QA Sign-off — Phase 4

> **Testing team:** confirm all items below before marking this phase done.

- [x] `setFocus(A)` followed by `setFocus(B)`: widget A receives exactly one `onBlur` call and widget B receives exactly one `onFocus` call
- [x] Calling `setFocus` on the widget that already has focus does not fire `onBlur` or `onFocus` a second time
- [x] `setFocus(nullptr)` calls `onBlur` on the current owner and leaves `focused()` returning `nullptr`
- [x] `forceOwner(W)` sets `focused()` to W without triggering `onBlur` or `onFocus` on any widget (instrument with a call counter)
- [x] `isFocused()` returns `true` only while the widget is the current owner; returns `false` after it is blurred
- [x] `setDisabled(true)` + `isDisabled()` round-trips correctly; `setDisabled(false)` restores the original state

---

## Phase 5 — View System & View Stack

- [x] Implement `View` base class in `include/hui/View.h` (§8.1)
  - [x] Lifecycle hooks: `onPush()`, `onPop()`, `onResume()`, `onSuspend()`
  - [x] `update(float dt, FocusManager&)`, `draw(IRenderer&, const Theme&)` virtuals
  - [x] `onButtonDown()`, `onButtonUp()` virtuals returning `bool`
  - [x] `setDimmed()` / `isDimmed()` and `dimmed_` protected field
  - [x] `currentHints()` returning `std::vector<HintEntry>`
  - [x] `suspendFocus()` / `restoreFocus()` using `savedFocus_` pointer (§7.2)
- [x] Implement `ViewStack` in `src/ViewStack.cpp` (§8.2)
  - [x] `push()`: calls `onSuspend()` on previous top, then `onPush()` on new view; takes `unique_ptr` ownership
  - [x] `pop()`: calls `onPop()`, then `onResume()` on the view below; no-op on a single-entry stack
  - [x] `popTo<T>()` template: pops until the correct type is on top
  - [x] `replace()`: atomic pop+push
  - [x] `update()`: drives all views in the stack
  - [x] `draw()`: renders bottom-to-top; applies `setGlobalAlpha(128)` to all non-top views when stack depth > 1, restores to 255 before the top view (§8.2)
  - [x] `dispatchButtonDown()` / `dispatchButtonUp()`: routes to `top()` only
- [x] Provide optional `TransitionKind` enum and `SimpleTransition` helper struct (§8.4); not used by default `ViewStack`

### ✅ QA Sign-off — Phase 5

> **Testing team:** confirm all items below before marking this phase done.

- [x] `push(B)` while A is on top: A's `onSuspend` fires before B's `onPush`; confirm call order with instrumented stubs
- [x] `pop()` while B is on top of A: B's `onPop` fires, then A's `onResume`; confirm call order
- [x] `pop()` on a stack with exactly one view is a no-op: the view is not removed and no lifecycle hook fires
- [x] `dispatchButtonDown` delivers the event only to the top view; a view below the top must not receive it (instrument both views)
- [x] With two views on the stack, the non-top view is drawn with dimming applied; the top view is drawn at full alpha
- [x] `suspendFocus()` records the focused widget; `restoreFocus()` returns focus to that exact widget via `forceOwner`

---

## Phase 6 — Input System

- [ ] Implement `KeyRepeatDriver` in `src/KeyRepeatDriver.cpp` (§9.2)
  - [ ] `onButtonDown()` / `onButtonUp()` to record/clear held state
  - [ ] `update(float dt, Sink&&)` template: advances timers, fires synthetic `ButtonEvent`s
  - [ ] Timing constants: `kInitialDelay = 0.3s`, `kRepeatInterval = 0.1s`, `kFastInterval = 0.03s`, `kFastThreshold = 1.0s`
  - [ ] `shouldRepeat()`: true for directional and shoulder buttons only; face buttons and Start/Select/Guide do not repeat
  - [ ] Synthetic events must set `ButtonEvent::synthetic = true`

### ✅ QA Sign-off — Phase 6

> **Testing team:** confirm all items below before marking this phase done.

- [ ] Holding `Button::Down` for 0.25 s fires zero synthetic events (still within the 300 ms initial delay)
- [ ] Holding `Button::Down` for 0.35 s fires exactly one synthetic repeat
- [ ] After the first repeat, subsequent repeats arrive at ~100 ms intervals; measure at least five consecutive repeats and confirm none deviate by more than 10 ms
- [ ] After holding for more than 1.0 s, the interval drops to ~30 ms; measure at least five consecutive fast repeats
- [ ] Releasing and immediately re-pressing a button resets the initial delay countdown from zero
- [ ] Holding `Button::A`, `Button::B`, `Button::X`, `Button::Y`, `Button::Start`, `Button::Select`, and `Button::Guide` for 2 s produces zero synthetic repeat events for each
- [ ] Every synthetic event has `ButtonEvent::synthetic == true`; real hardware events always have `synthetic == false`

---

## Phase 7 — UISystem

- [ ] Implement `UISystem` in `src/UISystem.cpp` (§10)
  - [ ] Constructor takes `IRenderer&` and `const Theme&` by reference; owns `ViewStack`, `FocusManager`, `KeyRepeatDriver`
  - [ ] `onButtonDown()`: feeds `KeyRepeatDriver`, then dispatches to `ViewStack`
  - [ ] `onButtonUp()`: feeds `KeyRepeatDriver`
  - [ ] `update(float dt)`: drives `KeyRepeatDriver` (which injects synthetic repeats into `ViewStack`), then calls `ViewStack::update()`
  - [ ] `draw()`: calls `ViewStack::draw()`
  - [ ] `viewStack()` and `focusManager()` accessors
  - [ ] `setAnimationsEnabled(bool)` (§13.6); propagated to views/widgets that animate

### ✅ QA Sign-off — Phase 7

> **Testing team:** confirm all items below before marking this phase done.

- [ ] A real `onButtonDown(Button::A)` call reaches the top view's `onButtonDown` within the same call frame (no buffering)
- [ ] Synthetic repeats generated inside `update()` also reach the top view's `onButtonDown` during that same `update()` call
- [ ] `onButtonUp` stops all repeats for the released button; no further synthetic events arrive in subsequent `update()` calls
- [ ] `setAnimationsEnabled(false)` is confirmed to propagate: a `GuideOverlayView` stub (or the real one if already implemented) opens instantly instead of animating
- [ ] Two independent `UISystem` instances can be driven simultaneously without sharing state; driving one does not affect the other

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

- [ ] `ListItemWidget` — single row: icon, primary label (`drawTextEllipsis`), secondary label, right meta; states: default / focused / playing / disabled; variants: default, track, folder, playlist (§12)
- [ ] `GridCellWidget` — tile: thumbnail texture or gradient placeholder (using `hueToColor` + `labelHash`), label, sublabel; focused border; playing badge (§12, §13.2)
- [ ] `ProgressBar` — read-only horizontal fill bar; elapsed and total timestamp labels (§12)
- [ ] `Slider` — focusable horizontal value control; Left/Right buttons change value; calls `onValueChanged` callback (§12)
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
  - [ ] D-pad Up/Down moves focus; key-repeat consumed from `onButtonDown`
  - [ ] L1/R1 page jump; L2/R2 jump to first/last item
  - [ ] Wrap-around at top and bottom
  - [ ] Scroll-to-focus using the bottom-third / top-third formula (§13.3)
  - [ ] `getFocusIndex()` for focus memory; does not reset on `onBlur()`
  - [ ] Empty state rendering (placeholder message)
  - [ ] Scroll indicator (right-side bar)
- [ ] `GridView` — 2D focusable grid; same scroll / wrap / page / memory rules as `ListView` (§12)
- [ ] `TabBarWidget` — horizontal tab strip; L1/R1 to switch tabs; not reachable via D-pad Up/Down; calls `onTabChanged` callback (§12)
- [ ] `QueueList` — extends `ListView` with grab-mode reorder: Y to grab, Up/Down to reorder, A to drop, B to cancel (§12)
- [ ] `LetterWheel` — character strip + results list; internal focus routing between strip and list (§12)
- [ ] `OnScreenKeyboard` — 2D QWERTY grid widget; Confirm / Cancel / Backspace keys; `onCommit` and `onCancel` callbacks (§12)
- [ ] `ContextMenuView` — overlay `View`; semi-transparent background fill (`theme.overlay`); action list with destructive color support; B cancels (§12)
- [ ] `ConfirmationDialogView` — overlay `View`; default focus on Cancel; Left/Right switches between Cancel and Confirm; A confirms, B cancels (§12)
- [ ] `TrackInfoPanelView` — overlay `View`; read-only metadata table (§12)
- [ ] `GuideOverlayView` — overlay `View` sliding in from right; contains `Slider` widgets for volume and brightness, plus action items; animation degrades to instant on SDL1 when `setAnimationsEnabled(false)` (§12, §13.6)

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
- [ ] `GuideOverlayView` slides in from the right under SDL2; with `setAnimationsEnabled(false)` it appears instantly with no intermediate animation frames
- [ ] `OnScreenKeyboard` Backspace deletes the last typed character; Confirm calls `onCommit` with the full typed string; Cancel calls `onCancel` without modifying the string

---

## Phase 13 — Level 4 Screens (Application Views)

- [ ] `Shell` — permanent wrapper `View`: owns `StatusBarWidget`, `HintBarWidget`, optional `TabBarWidget`; computes and exposes the content area `Rect` for page views (§12)
- [ ] `DirectoryView` — uses `ListView` + `ListHeaderWidget`; pushes `ContextMenuView`, `TrackInfoPanelView`, `ToastNotification` (§12)
- [ ] `LibraryView` — uses `ListView` + `GridView` + `TabBarWidget`; pushes `ContextMenuView`, `LetterWheel`, `TrackInfoPanelView` (§12)
- [ ] `NowPlayingView` — uses `SeekableProgressBar`, `PlaybackControlsRow`, `QueueList`; pushes `ContextMenuView`, `TrackInfoPanelView` (§12)

### ✅ QA Sign-off — Phase 13

> **Testing team:** confirm all items below before marking this phase done.

- [ ] `Shell` content area rect does not overlap the `StatusBarWidget` (top) or `HintBarWidget` (bottom) under any screen resolution; verify at both 480×320 and 640×480
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