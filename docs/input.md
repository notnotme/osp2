# Input domain

Platform-layer controller input in `src/input/CursorEmulator.{h,cpp}`. The Switch has no mouse, and ImGui is entirely mouse/nav-driven, so `CursorEmulator` synthesizes a virtual cursor from an `SDL_GameController` and injects it into ImGui IO. It lives in the platform layer, constructed by `Platform` — no domain, gui, or player type depends on it.

```mermaid
classDiagram
    class CursorEmulator {
        -SDL_GameController* m_pad
        -float m_x
        -float m_y
        -int m_w
        -int m_h
        -bool m_leftDown
        -bool m_rightDown
        +CursorEmulator(int windowWidth, int windowHeight)
        +~CursorEmulator()
        +update(ImGuiIO& io)
    }

    CursorEmulator ..> ImGuiIO : AddMouse*Event
    CursorEmulator ..> SDL_GameController : reads axes/buttons
```

## Mapping

| Input                 | Action                                    |
|-----------------------|-------------------------------------------|
| Left analog stick     | Move cursor (velocity ∝ deflection)       |
| Right analog stick    | Scroll (vertical primary, horizontal opt) |
| **A**                 | Left click (button 0, primary)            |
| **X**                 | Right click (button 1)                    |
| **L** shoulder or ZL trigger (hold) | Slow/precise, cursor + scroll (mul ≈ 0.35×) |
| **R** shoulder or ZR trigger (hold) | Fast, cursor + scroll (mul ≈ 2.5×)          |
| neither held          | Normal speed (1×)                         |

## Notes

- **Per-frame injection point (main-thread only).** `update(io)` is called once per frame from the render loop **between `ImGui_ImplSDL2_NewFrame()` and `ImGui::NewFrame()`** — after the SDL backend seeds IO, before ImGui consumes it. This is the ImGui-idiomatic way to feed a virtual cursor; calling it anywhere else drops or double-counts input. It touches only ImGui IO and the SDL controller; it holds no locks and must not be called off the main thread.
- **Switch only.** The instance is a `#ifdef __SWITCH__` local in `main()` and the `update()` call is likewise guarded; desktop uses the real mouse and none of this compiles in. `io.MouseDrawCursor` is also set `true` on the Switch (the OS draws no cursor there) so ImGui renders the emulated one.
- **Cursor integration.** The left-stick deflection (with a dead-zone at rest, rescaled so motion ramps from zero at the dead-zone edge) is scaled by a base speed in px/frame and added to the cursor position each frame, clamped to `[0, windowWidth] × [0, windowHeight]`. SDL `LEFTY` is positive when the stick is pushed down and screen Y grows downward, so raw deflection maps directly to intuitive motion on both axes. The cursor seeds to the window center.
- **Scroll injection.** The right stick feeds `io.AddMouseWheelEvent(wheel_x, wheel_y)` each frame, with vertical primary. ImGui treats positive `wheel_y` as scroll-up; SDL `RIGHTY` is negative when the stick is up, so `wheel_y = -deflection(RIGHTY) × scrollSpeed` makes an up push scroll up. Horizontal follows the same dead-zone; ImGui treats positive `wheel_x` as scroll-*left*, so it is also negated (`wheel_x = -deflection(RIGHTX) × scrollSpeed`) to make a right push scroll right. The event is emitted only when there is real deflection, so at rest no zero-wheel events are spammed. Scroll speed stays small (ImGui scrolls a fixed number of items per wheel unit) and may need tuning against hardware feel.
- **Speed modifiers.** Holding the **L** side (`SDL_CONTROLLER_BUTTON_LEFTSHOULDER` **or** the `TRIGGERLEFT` axis past a threshold — i.e. L or ZL) scales speed by ≈0.35× for precision; holding the **R** side (`RIGHTSHOULDER` or `TRIGGERRIGHT`, i.e. R or ZR) scales it by ≈2.5× for fast travel; neither held is 1×. If both sides are held, slow wins. Either input on a side works, whichever is easier to reach. The multiplier scales **both** the left-stick cursor movement and the right-stick scroll. Unlike the rotated A/B/X/Y face buttons, the physical L/R shoulders and triggers map directly to their SDL names, so no `#ifdef` is needed for them.
- **Click edges.** A/X button state is emitted to ImGui only on change (`m_leftDown`/`m_rightDown` mirror the physical state), so a held button is not re-fired oddly — though ImGui tolerates repeated same-state events.
- **Switch button labels.** SDL names controller buttons by *Xbox position*, but the Switch's physical labels are rotated: its physical **A** is at the east position (`SDL_CONTROLLER_BUTTON_B`) and physical **X** at the north (`SDL_CONTROLLER_BUTTON_Y`). Under `__SWITCH__`, `update()` reads `B`/`Y` so the console's usual confirm button (**A**) left-clicks and **X** right-clicks; the desktop build (never actually invoked — cursor emulation is Switch-only) keeps the plain `A`/`X` positions.
- **Constants.** Dead-zone (~8000 of the signed-16-bit axis range), base speed (~12 px/frame at full tilt), the L/R speed multipliers (~0.35× / ~2.5×), and scroll speed (~0.5 wheel units/frame at full tilt) are local constants in `CursorEmulator.cpp`. They could later be surfaced as [Settings](settings.md) (TODO_6) without changing the `update(io)` interface.
- **Controller ownership.** `CursorEmulator` opens its own handle via `SDL_GameControllerOpen(0)` (closed in its destructor). `Platform` independently opens the same index (`m_controller`) as the single owner used by the quit-on-START handler; `SDL_GameControllerOpen` is refcounted per index, so the two opens and their matching closes are safe. `SDL_GameControllerOpen(0)` may return `nullptr` (no pad present) — every pad read is guarded.
