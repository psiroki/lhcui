# Agent guide — lhcui (HUI toolkit)

Quick orientation for automated agents working in this repo. For full semantics, **`DESIGN.md` is authoritative**; **`TODO.md` is the implementation checklist** (phases, per-item tasks, QA sign-off).

## Build and platform

- **Library target:** `hui` (static). **Executables:** `hui_example`, `hui_tests`.
- **C++17.** Public headers live in `include/hui/`, implementations in `src/`.
- **Default renderer:** SDL2 (`cmake ..` in `build/`). SDL1 is opt-in via `-DHUI_USE_SDL1=ON`.
- **Mac:** SDL1 is not available and will not be installed. Always configure with SDL2 (`HUI_USE_SDL1=OFF`). Do not attempt SDL1 builds on macOS.
- **Mac / `sdl2-compat` trap.** Homebrew has both `sdl2` and `sdl2-compat` installed, and `sdl2-compat` owns the `/opt/homebrew/lib/libSDL2-2.0.0.dylib` symlink. It is not SDL2: it reimplements the SDL2 API and `dlopen`s **SDL3**, so it never appears in `otool -L`. Both report version 2.32.70, so `pkg-config` cannot tell them apart. Ordinary runs work; **ASan hangs indefinitely**. To get the real SDL2, run with `DYLD_LIBRARY_PATH=/opt/homebrew/opt/sdl2/lib`. Verify with `otool -L build/hui_example | grep -i sdl` — a path containing `sdl2-compat` means you are on SDL3.
- **Keyboard fallback** (`HUI_ENABLE_KEYBOARD_FALLBACK`) defaults on in non-Release builds for desktop testing without a gamepad.
- **Verify changes:** from `build/`, run `cmake --build . && ./hui_tests && ./hui_example --selftest`. Both must pass before checking QA items in `TODO.md`.

```bash
mkdir -p build && cd build
cmake .. -DHUI_USE_SDL1=OFF
cmake --build .
./hui_tests
./hui_example --selftest   # headless; drives the app through the QA navigation paths
```

`hui_example --selftest` forces SDL's dummy video driver, scripts a full navigation session
(browse, play, transport, overlays, library tabs, button mashing), prints a pass/fail count
and exits non-zero on failure. It covers the integration paths `hui_tests` cannot reach.

When adding a new `src/*.cpp`, append it to the `add_library(hui STATIC …)` list in `CMakeLists.txt`. New test files go into the `hui_tests` target the same way.

## Repository layout

| Path | Purpose |
|------|---------|
| `include/hui/` | Public API — one header per component |
| `src/` | Widget/view implementations |
| `src/renderer/` | `SDL1Renderer`, `SDL2Renderer` (implement `IRenderer`) |
| `src/sdl/` | Gamepad mapping, keyboard fallback |
| `example/main.cpp` | Visual demo; not the primary test surface |
| `tests/` | doctest suite (`tests/doctest.h`, single binary `hui_tests`) |
| `DESIGN.md` | Architecture, contracts, widget catalogue (§12), perf rules (§13) |
| `TODO.md` | Phased roadmap; check boxes when implementation/QA is done |

## Architecture (mental model)

```
Application main loop
  → UISystem::onButtonDown/Up, update(dt), draw()
    → Shell chrome (optional, not on ViewStack)
    → ViewStack (screens + overlays, bottom-to-top draw)
      → View subclasses own widgets, route input explicitly
        → Widget subclasses draw within bounds_, handle onButtonDown
    → FocusManager (exactly one focused widget; containers keep internal index separately)
```

**View vs Widget:** `View` = screen or overlay (navigation unit, on the stack). `Widget` = drawable/control inside a view. **`Shell` is a Widget, not a View** — permanent chrome (status bar, hint bar, toast); never pushed on the stack.

**Input:** Only `ViewStack::top()` receives buttons. There is no automatic focus traversal — each view/container implements D-pad routing (see §9.3, §9.5 in `DESIGN.md`). `NavList` helps ordered cycling in dialogs; it is not a widget.

**Stack mutations are deferred.** Views call `ViewStack::push/pop` from handlers; `applyPendingMutations()` runs at end of frame. Safe to pop self from `onButtonDown`.

**Overlays:** `dimsBelow() == true` → ViewStack draws **one** full-screen scrim before the overlay. Overlays must **not** draw their own full-screen dim (double-darkens).

## Widget taxonomy (DESIGN.md §12)

Implement in layer order; higher layers depend on lower ones.

| Level | Examples | Notes |
|-------|----------|-------|
| 0 — Input | `KeyRepeatDriver`, `FocusManager`, `NavList`, `IListSource` | Internal or helpers |
| 1 — Atoms | `ListItemWidget`, `GridCellWidget`, `Slider`, `ProgressBar`, toggles | **Stamp widgets** — one instance, refilled per row/cell |
| 2 — Molecules | `ListHeaderWidget`, `SeekableProgressBar`, `PlaybackControlsRow`, chrome | Composed atoms |
| 3 — Organisms | `ListView`, `GridView`, `TabBarWidget`, `QueueList`, overlays | Containers + overlay Views |
| 4 — Screens | `DirectoryView`, `Shell`, … | Application `View` subclasses (Phase 13+) |

## Patterns agents must follow

### Container lists (`ListView`, `GridView`, `QueueList`)

- Backed by **`IListSource*`** (not an owned `vector` of rows). Short static lists use `VectorListSource`.
- Own **one stamp widget** (`ListItemWidget` / `GridCellWidget`); draw only the **visible window** with **one** `pushClip(bounds_)` per frame (§6.2).
- `layout()` before first `draw()` / `onButtonDown()`; compute `pageRows_` there.
- `scrollOffset_` is pixels; `maxScroll = max(0, totalHeight - viewportHeight)` — required, not defensive (§13.3).
- `notifyRowsChanged()` reclamps focus/scroll and invalidates text cache; scrolling alone must not.
- `focusedIndex_` survives `onBlur()`; reset only via `resetFocus()` when explicitly switching context (e.g. tab change).

### Focus

- `FocusManager` tracks **which widget** holds focus, not which list row is highlighted.
- `isFocusable() == false` for chrome (`HintBarWidget`, `StatusBarWidget`, stamp widgets, toggles embedded in a focusable row).
- Focus lifecycle only via `FocusManager`; widgets must not toggle `focused_` themselves.

### Overlay views

- Constructor takes `ViewStack&`; dismiss with `stack_.pop()` on B (deferred).
- Override `restoreFocus()` to set initial focus when pushed (e.g. Cancel button at index 0).
- `currentHints()` for the hint bar; `Shell` reads `ViewStack::top()->currentHints()` each frame.

### Rendering / text

- `IRenderer` is the only draw path widgets use (mock it in tests).
- Text cache key = **string content + font**, never row index or Y position (§13.1). Call `invalidateTextCache()` from containers on `notifyRowsChanged()` (SDL1 implements this; SDL2 no-ops).
- `Helpers.h`: `leftTruncate`, `hueToColor`, `labelHash`, `buttonGlyphColor`.

## Testing

- Framework: **doctest** (not GoogleTest — README is stale).
- Single binary: `hui_tests`. Phase-aligned files: `test_types`, `test_focus`, `test_view`, `test_input`, `test_uisystem`, `test_sdl`, `test_helpers`, `test_atoms`, `test_molecules`, `test_organisms`.
- Pattern: subclass `IRenderer` to count `fillRect`/`pushClip`/colors; drive widgets directly without SDL when possible.
- `test_view.cpp` covers deferred stack mutations and `dimsBelow()` scrim count.
- Only mark `TODO.md` QA items `[x]` when covered by passing tests or explicit manual verification.

## Where to look for what

| Question | Start here |
|----------|------------|
| What to build next? | `TODO.md` — first unchecked phase |
| API contract for a widget? | `DESIGN.md` §12 + section cited in `TODO.md` |
| ListView scroll/focus math? | `DESIGN.md` §6.2, §6.5, §13.3 |
| Button routing / global map? | `DESIGN.md` §9.3, §9.5 |
| View stack / overlays? | `DESIGN.md` §8, `include/hui/ViewStack.h` |
| Focus rules? | `DESIGN.md` §7, `include/hui/FocusManager.h` |
| Per-frame entry point? | `include/hui/UISystem.h`, `DESIGN.md` §10 |

## Conventions when editing

- Match existing style: minimal scope, no exceptions/RTTI, `enum class`, `string_view` for non-owning text.
- Header guard + one class per `include/hui/Foo.h` + `src/Foo.cpp`.
- Views use `HUI_VIEW_TYPE(Class)` for `ViewStack::popTo<T>()`.
- Comments reference design sections (`§12`) where helpful; do not duplicate `DESIGN.md` prose in code.
- Do not commit unless asked. Do not mark `TODO.md` implementation items done without the code existing.
