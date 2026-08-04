# MIGRATION — Revision 1 → Revision 2

**This is a temporary file. It deletes itself at the end of Phase 5.5. See
[Closing Out](#closing-out).**

---

## Read this first

`DESIGN.md` has been revised. The revision changed decisions that live inside **Phase 4** and
**Phase 5**, both of which are already marked complete in `TODO.md`.

**The existing code does not match `DESIGN.md`.** Do not read a checked box in Phase 4 or
Phase 5 as "the code matches revision 2 here." The affected boxes have been un-checked and
marked `↺ rev2`, and the ordered work list for fixing them is **Phase 5.5 — Revision 2
Retrofit**.

Do Phase 5.5 before Phase 6. It is mostly signature changes and two deletions. Every
subsequent phase builds directly on this surface, so the cost of deferring it compounds.

---

## Why each thing changed

Grouped by what it protects against, because a change whose purpose is understood survives
refactoring and one that isn't gets reverted.

### 1. `Widget::layout(Rect)` — new; `draw()` loses its `Rect` parameter

Revision 1 handed each widget its rect on every `draw()` call and stored nothing. Three
problems:

- **Cost.** Geometry work ran at frame rate. `ListHeaderWidget::leftTruncate` measures
  progressively shorter substrings against the font — the most expensive operation in the UI —
  for a path that changes when the directory changes, not thirty times a second.
- **Correctness.** A widget that needs its own size to interpret *input* had no way to get it.
  `ListView`'s L1/R1 page jump is `bounds_.h / itemHeight_` rows; with the rect only available
  during `draw()`, an input event arriving before the first frame had no viewport height. Every
  container would have had to cache the rect from the last draw and hope one had happened.
- **Testability.** Container partitioning could not be verified without a renderer.

This is a *minimal* retained layout — one rect per widget, pushed down, no reflow, no
invalidation cascade, no constraint solver. The §1 non-goal was reworded accordingly. Do not
let it grow into a layout engine.

### 2. `Widget::isFocusable()` — new

Revision 1 had no way for a widget to say it is not focusable. The catalogue is full of
non-focusable widgets — status bar, hint bar, list header, playback controls row, the three
toggles — and the only negative signal was `disabled_`, which means something else entirely
(a *focusable* control that is temporarily unavailable). Nothing prevented
`setFocus(&hintBar_)`.

`FocusManager::setFocus()` and `forceOwner()` now refuse a widget that is not focusable or is
disabled, which makes the invalid state unreachable rather than merely undocumented.

### 3. The focus setter was split — `setFocusedFlag()` / `setFocusedAndNotify()`

This one was an outright defect. Revision 1 had:

```cpp
void setFocused(bool f) { focused_ = f; if (f) onFocus(); else onBlur(); }
```

…and specified `forceOwner()` as "sets current without lifecycle callbacks." Those two are
incompatible: the flag and the callbacks were fused, so `forceOwner` could not update
`focused_` at all. A widget restored after an overlay popped would render **un-highlighted**.
Revision 1 also contradicted itself about which one `restoreFocus()` used (§7.2 said
`setFocus`, the Phase 5 QA said `forceOwner`).

`forceOwner` now updates the flag on both the outgoing and incoming widget and fires no
callbacks. That is the behaviour returning-from-an-overlay wants: re-light the highlight, do
not re-run side effects.

### 4. `ViewStack` mutations are deferred

**This was a use-after-free in the default control flow of every overlay in the application**,
not an edge case.

Every overlay pops itself from inside its own `onButtonDown` — B dismisses a context menu, A
on Confirm closes a dialog, Guide closes the guide panel. With immediate `pop()`, the
`unique_ptr` ran the destructor while the call stack was still inside a member function of
that object, and `dispatchButtonDown` then read the return value from a dead frame.

`push`/`pop`/`popTo`/`replace` now enqueue. `UISystem` applies the queue at two safe points:
the top of `update()`, and after event dispatch has fully unwound. The lifecycle order is
spelled out in §8.2 and must be followed exactly, including `fm.setFocus(nullptr)` **before**
any view is destroyed — otherwise `FocusManager::current_` is left pointing into freed
widgets.

Test this under ASan. It will not reliably crash without a sanitizer.

### 5. Dimming: per-layer alpha → single scrim; `dimsBelow()` replaces `setDimmed()`

Revision 1 dimmed every non-top view with `setGlobalAlpha(128)`. Three problems:

- **It was redundant.** §8.3 *also* had overlays fill the screen with `theme.overlay`
  themselves. Two mechanisms, double-darkened background.
- **It was expensive in the way §5.2 explicitly warns against.** On SDL1 `setGlobalAlpha`
  costs a premultiplied surface copy per layer per frame. With chrome plus base plus overlay
  that was two dimmed layers, every frame.
- **It broke the chrome.** Because `Shell` was a `View` on the stack, it was never `top()`, so
  the status bar and hint bar were dimmed **permanently**, during ordinary navigation, for the
  entire life of the application.

Dimming is now a property of the view being pushed on top (`dimsBelow()`), realised as one
full-screen `fillRect` drawn by `ViewStack` immediately before that view. Normal navigation
never dims; a modal dims everything beneath it including the chrome, which is the intended
effect.

`setGlobalAlpha` stays in `IRenderer` but only for widget-local fades — toast fade-out, guide
slide.

### 6. `Shell` is no longer a `View`

Consequence of the above, plus an internal contradiction in revision 1: §12 said `Shell`
"owns the content area rect passed down to page views," but a stack entry cannot hand a rect
to the entry above it.

`Shell` is now a composite `Widget`, registered via `UISystem::setShell()`, drawn beneath the
view stack (so a modal's scrim covers it) with the toast drawn above everything.

### 7. `ToastNotification` is a `Shell`-owned widget, never pushed

Revision 1's Phase 13 listed `DirectoryView` as "pushing" `ToastNotification` alongside real
overlay views. A pushed toast becomes `top()`, so it would receive all input, replace the hint
bar's contents, and dim the screen behind it — while the widget guide requires it to be
visible *without* stealing focus. It is chrome. `Shell::showToast()`.

### 8. Lists are virtual — `IListSource` / `RowData` / stamp widgets

Revision 1's §6.2 reference pattern owned a vector of item objects and looped over **all** of
them every frame, relying on clipping to discard the off-screen ones. On a 5,000-track library
that is 5,000 clip pushes and ~15,000 `drawTextEllipsis` calls per frame at 480×320, where the
visible window is 17 rows.

Two changes: containers pull rows through `IListSource` (no copy of the application's data, no
second source of truth, no per-row allocation), and they draw only the visible window with one
clip for the whole viewport. `ListItemWidget` becomes a single reused stamp rather than one
widget per row.

This is the same distinction as Win32's owner-data list view versus its item-owning default.

### 9. Focus memory stays index-based, and the hazard is documented

Deliberately **not** changed to stable item IDs. Views are never popped during navigation
(they stay on the stack), so a suspended `ListView` keeps its focus index and scroll offset
with no save/restore at all. IDs would require every data source to mint and maintain them.

The hazard this leaves is real and is now written down: if the application mutates rows while
a view holds a focus index, that index refers to a different row afterwards. The application
calls `notifyRowsChanged()`, and `setFocusIndex()` if it cares. The toolkit does not infer it.

### 10. `NavList` — new

Manual traversal is kept (§7.3's argument is sound and the iPod-style drill-down UI does not
traverse). But revision 1 gave every `View` subclass no choice but to hand-roll the same
ordered cycle, and the revision-1 Phase 15 already had a scheduled cleanup for the
consequences ("audit all `onButtonDown` return values").

`NavList` is ~60 lines, owns nothing, draws nothing, and imposes no policy. Exactly two
consumers: `ConfirmationDialogView` (horizontal) and `GuideOverlayView` (vertical, the
"unified focus list"). Making the axis a property rather than a layout decision means both use
one mechanism, so Cancel/Confirm can stay side by side — the conventional handheld layout —
without a second traversal path.

### 11. `dt` is clamped in `UISystem::update()`

New `setSuspended()` (§10.1) means the loop can sleep for a long time. Feeding the real
elapsed time back in afterwards hands `KeyRepeatDriver` a `dt` of ~1200 s, and it will
faithfully synthesise tens of thousands of repeat events into whichever view is on top. §6.4
protects animations from this by accident; the repeat driver had no such protection.

Clamping once in `UISystem::update()` fixes it for everything downstream. `kMaxDelta = 100 ms`.

Related: held state is now flushed on every stack change. Without it, holding Down while an
overlay opens means the repeat stream immediately scrolls the overlay — a list the user's
finger was never on.

### 12. `ChordDetector` — new

`Button::Guide` existed and the widget guide specifies START+SELECT as the fallback for
hardware without a Guide button, but nothing anywhere detected the chord. Individual views
cannot. It belongs in the input layer.

### 13. `TransitionKind` and `SimpleTransition` deleted

No consumer, and revision 1 stated outright that the default `ViewStack` did not use them.
With one application consuming this toolkit, an abstraction with no caller cannot be validated
and is pure maintenance load across every remaining phase. If a transition is ever wanted, it
can be written against a real requirement then.

### 14. Scroll behaviour pinned down

Smooth (pixel-granular) scrolling with **uniform row height**, `scrollOffset_` assigned
directly rather than eased. Also fixed: the revision-1 clamp

```cpp
std::clamp(target, 0, totalContentHeight_ - bounds.h)
```

is **undefined behaviour** whenever the list is shorter than the viewport, because `lo > hi`.
The Phase 12 QA tests a zero-item list explicitly, so this was reachable. `maxScroll` must be
floored at 0.

`ListHeaderWidget` is pinned above the list rather than scrolling as row −1, so no offset
arithmetic needs a bias term.

### 15. Text cache keying — verify, do not assume

Not a signature change, but the single most performance-critical line in the document.
Smooth scrolling redraws every visible row at a new y every frame while the **strings are
identical**. The measurement/glyph cache must be keyed on **string content + font handle** and
never on row index, slot, or y position. A position-keyed cache invalidates every frame while
scrolling and silently destroys the frame budget — and it will not look like a bug until the
SDL1 target is profiled in Phase 15.

Scrolling must not invalidate the cache; only `notifyRowsChanged()` does.

---

## What was deliberately NOT changed

Recorded so these are not "fixed" later by someone assuming they were oversights:

- **Manual focus traversal.** Correct for a gamepad UI. No spatial or tab-order engine.
  `NavList` is a helper, not a traversal engine.
- **Focus memory by index, not by stable ID.** See item 9.
- **Views stay on the stack** when navigated away from. Not destroyed and rebuilt.
- **`FocusManager` kept**, despite doing little in this application. It enforces the
  one-focused-widget invariant at ~30 lines. Do not grow it; do not delete it.
- **No dirty-rect system.** Full redraw every frame (§6.3).
- **No RTTI, no exceptions, C++17.** Unchanged.
- **Fixed row height.** Uniform `itemHeight_` is load-bearing for O(1) windowing. The one
  mixed-height list in the app (the guide overlay) is a hand-laid view driven by `NavList`,
  specifically so this stays true.

---

## Closing Out

When **every** box in Phase 5.5 including its QA sign-off is checked:

1. Delete this file.
2. Remove the `↺ rev2` markers from `TODO.md`. Leave the boxes checked.
3. Remove the revision-2 notice block from the top of `TODO.md`.
4. Remove the `⚠ MIGRATION.md` notice block from the top of `DESIGN.md`. Leave the
   "**Revision 2.**" label — it is useful provenance.
5. Leave the rationale in `DESIGN.md` itself alone. Notes like "this was a real defect in an
   earlier revision" are load-bearing: they stop the defect being reintroduced by someone who
   thinks the split setter looks redundant.

The corresponding checkbox is the last item in the Phase 5.5 QA block. After that, `DESIGN.md`
and the code agree and this file has no reason to exist.
