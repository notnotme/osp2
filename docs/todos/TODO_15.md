# TODO_15 — Quit-app button

> Single chunk. There is no way to quit from the UI today; add an in-app quit control wired through the existing `ButtonId` / `onButtonClick` mechanism. Independent of the other items (benefits from TODO_12's cleaner `main.cpp` if done after it).

## Context

The run loop's exit flag `is_running` is a **plain local `bool`** in `main.cpp` (`:299`), flipped only by Esc (desktop), START (Switch), or window-close `SDL_QUIT` (`main.cpp:304–319`) — it is not exposed to `Gui` or `Application`.

Buttons flow through `ButtonId` (`src/gui/ButtonId.h`, currently `{ PLAY_PAUSE, STOP, NEXT, PREVIOUS }`): the Gui fires `actions.onButtonClick(ButtonId)` (`src/gui/UiActions.h`), wired in `Application::makeUiActions()` and routed to `Application::handleButtonClick` (`src/Application.cpp:34–60`), a `switch` over the enum. `Application` has no quit concept and can't reach `main.cpp`'s local `is_running`.

## Task chunks (implement, verify, and commit one at a time)

- [ ] **15a — Quit control**: add `QUIT` to the `ButtonId` enum; add a quit control in the Gui in a spot consistent with the existing top bar / menu (clear icon or label; consider a confirm step since it exits immediately). Because `is_running` is main-local and `Application` has no quit state, have **`main.cpp` provide the `onButtonClick` lambda that intercepts `QUIT`** (sets `is_running = false`) and delegates every other id to `app.handleButtonClick(...)` — no new state on `Application`. On Switch, quitting returns to the Home menu; confirm that's the intended UX alongside the existing START-to-exit. Verify: clicking Quit exits cleanly on desktop (window closes, `finalize()` runs) and returns to Home on Switch hardware; other buttons still route correctly through `Application`.

This chunk ends with green desktop + Switch builds, docs updated (`docs/ui.md`), user verification (incl. a Switch hardware test), then a commit. Run cpp-reviewer on the diff before committing.

## Files to change

1. **`src/gui/ButtonId.h`** — add `QUIT`.
2. **`src/gui/Gui.cpp`** — draw the quit control; fire `onButtonClick(QUIT)` (add a sprite/label as needed).
3. **`src/main.cpp`** — the `onButtonClick` lambda intercepts `QUIT` → `is_running = false`, delegating the rest to `app.handleButtonClick`.

No CMakeLists.txt change.

## Docs

- **`docs/ui.md`** — document the new quit control and the `QUIT` `ButtonId`; note that `main.cpp` (not `Application`) handles it because it owns the run-loop flag.

## Coordination

- Independent. Slightly cleaner if it lands after **TODO_12** (which reworks the `main.cpp` event loop / `onButtonClick` wiring), but not blocked by it.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- Desktop: click Quit → app exits cleanly (equivalent to Esc / window close); `finalize()` teardown order preserved (audio device closed before plugins).
- Switch hardware (ask before deploying): Quit returns to the Home menu; START-to-exit still works; other transport buttons (play/stop/next/prev) unaffected.
