# TODO_10 — Switch controller → mouse/scroll emulation (`CursorEmulator`)

> The Switch has no mouse, so ImGui (which is entirely mouse/nav-driven) is hard to operate on hardware. This item drives the ImGui cursor from the gamepad: **left stick moves the cursor, right stick scrolls, A = left click, X = right click, and the L / R shoulder buttons are held-only speed modifiers (L = slow/precise, R = fast)**. Greenfield — no existing input spec and no `docs/` input domain doc. No decoder dependency; independent of TODO_9.

## Context

Input today is minimal and mouse-centric:

- **ImGui gets all input via the SDL backend.** `ImGui_ImplSDL2_ProcessEvent(&event)` (`src/main.cpp:285`) forwards every SDL event; the app never touches ImGui IO directly. Mouse position/click/scroll therefore only ever come from a real mouse — which the Switch lacks.
- **The app opens a raw `SDL_Joystick`, used only to quit.** `joystick = SDL_JoystickOpen(0)` (`src/main.cpp:127`); the sole use is a quit-on-START handler (`main.cpp:292-296`) — and that handler has a **latent bug**: under a `SDL_CONTROLLERBUTTONDOWN` case it reads `event.jbutton` (the joystick union member) instead of `event.cbutton`. `SDL_INIT_GAMECONTROLLER` is already set (`main.cpp:102`), which implicitly inits the joystick subsystem.
- **ImGui gamepad navigation is off.** `ImGuiConfigFlags_NavEnableGamepad` is never set, so the sticks and face buttons are entirely free for cursor emulation — no fight with ImGui nav.
- **There is a clean per-frame injection point.** The render loop calls `ImGui_ImplOpenGL3_NewFrame()` / `ImGui_ImplSDL2_NewFrame()` / `ImGui::NewFrame()` in sequence (`main.cpp:309-311`). Injecting synthetic mouse input via `ImGui::GetIO().AddMouse*Event()` **between `ImGui_ImplSDL2_NewFrame()` and `ImGui::NewFrame()`** is the ImGui-idiomatic way to feed a virtual cursor.

## Architecture

A single small platform-layer class, driven once per frame from `main.cpp`; nothing else in the codebase changes shape.

```
per frame (main thread), Switch only
────────────────────────────────────
ImGui_ImplSDL2_NewFrame()
        │
        ▼
CursorEmulator::update(io)
   read SDL_GameController axes/buttons
   ├─ left stick  ─(deadzone × speedMul)─▶ integrate cursor x/y (clamp to window) ─▶ io.AddMousePosEvent
   ├─ right stick ─(deadzone)───────────▶ io.AddMouseWheelEvent (v primary, h optional)
   ├─ A / X       ─────────────────────▶ io.AddMouseButtonEvent(0/1, down)
   └─ L / R held  ─────────────────────▶ speedMul = slow / fast / normal
        │
        ▼
ImGui::NewFrame()  →  Gui draws, cursor is where the stick put it
```

- **`src/input/CursorEmulator.{h,cpp}` (new, platform layer).** GPL header. Owns an `SDL_GameController*` (opened via `SDL_GameControllerOpen(0)`, closed in its destructor), holds cursor `float m_x, m_y` (seeded to window center, clamped to window size), and exposes:
  ```cpp
  class CursorEmulator {
  public:
      CursorEmulator(int windowWidth, int windowHeight);
      ~CursorEmulator();
      CursorEmulator(const CursorEmulator&) = delete;
      CursorEmulator& operator=(const CursorEmulator&) = delete;
      void update(ImGuiIO &io);   // read pad, integrate, inject mouse pos/click/wheel
  private:
      SDL_GameController *m_pad;
      float m_x, m_y;
      int   m_w, m_h;
  };
  ```
  Reads named inputs via `SDL_GameControllerGetAxis` (`LEFTX/LEFTY/RIGHTX/RIGHTY`) and `SDL_GameControllerGetButton` (`A`, `X`, `LEFTSHOULDER`, `RIGHTSHOULDER`). Applies a dead-zone (~8000/32767), scales left-stick deflection by a base speed (px/frame at full tilt) × the held-modifier multiplier, integrates position, clamps to `[0,m_w]×[0,m_h]`, and calls `io.AddMousePosEvent` / `io.AddMouseButtonEvent(0/1, down)` / `io.AddMouseWheelEvent(h, v)`. Keeps click edge state so a held button isn't re-fired oddly (ImGui tolerates repeated same-state events; still, mirror down/up cleanly).

- **Mapping (confirmed).**

  | Input                 | Action                                   |
  |-----------------------|------------------------------------------|
  | Left analog stick     | Move cursor (velocity ∝ deflection)      |
  | Right analog stick    | Scroll (vertical primary, horizontal opt)|
  | **A**                 | Left click (button 0, primary)           |
  | **X**                 | Right click (button 1)                   |
  | **L** shoulder (hold) | Slow/precise cursor (mul ≈ 0.35×)        |
  | **R** shoulder (hold) | Fast cursor (mul ≈ 2.5×)                 |
  | neither held          | Normal speed (1×)                        |

- **Wiring in `main.cpp` (Switch only).** Under `#ifdef __SWITCH__`: own a `CursorEmulator`, set `io.MouseDrawCursor = true` once at init (the OS provides no cursor on Switch), and call `cursorEmulator.update(io)` between `ImGui_ImplSDL2_NewFrame()` and `ImGui::NewFrame()`. On desktop the real mouse is used and none of this compiles in. (A debug `#define` may enable it on desktop for quick iteration.)

- **Opportunistic fix.** Replace the raw `SDL_Joystick` usage (`main.cpp:127`, `:250`) and the buggy `event.jbutton` quit handler (`main.cpp:292-296`) with consistent `SDL_GameController` handling now that a controller is opened for cursor emulation.

- **Constants → future Settings.** Dead-zone, base speed, slow/fast multipliers, and scroll speed are constants in `CursorEmulator` for now; they could later become `Settings` entries (TODO_6) without changing the interface.

## Task chunks (implement, verify, and commit one at a time)

- [x] **10a — Controller cursor + clicks.** Add `src/input/CursorEmulator.{h,cpp}`; open `SDL_GameController`, migrate off the raw `SDL_Joystick` and fix the `event.cbutton` quit bug; wire `update()` into the per-frame injection point; enable `io.MouseDrawCursor` on Switch. Left stick moves the cursor (with dead-zone), **A = left click, X = right click**. Verify: cursor moves and buttons activate ImGui widgets (temporary desktop enable for fast iteration, removed/guarded before finishing; then Switch hardware test).
- [x] **10b — Scroll + speed modifiers + polish.** Right stick → `AddMouseWheelEvent`; **L held = slow, R held = fast**; clamp cursor to window bounds; tune dead-zone/speed/scroll constants. Verify scrolling in a long file list and precise-vs-fast movement on Switch hardware.

Each chunk ends with green desktop + Switch builds, a docs update if classes/methods changed, user verification, then a commit. Run cpp-reviewer on the diff before committing; run the pending Switch hardware test before merging the branch.

## Files to change

1. **`src/input/CursorEmulator.{h,cpp}`** (new) — the controller→ImGui-IO cursor class.
2. **`src/main.cpp`** — own a `CursorEmulator` (Switch only), `SDL_GameControllerOpen`/close replacing the raw joystick, `io.MouseDrawCursor = true` on Switch, `update()` call between the SDL and ImGui `NewFrame`s, and the `event.cbutton` quit-handler fix.
3. **CMakeLists.txt** — add `src/input/CursorEmulator.cpp` to `add_executable`. **No new external dependency** (SDL2 + ImGui only).

## Docs

- **`docs/input.md`** (new domain doc — none exists for platform/input). Mermaid `classDiagram` for `CursorEmulator`, the controller→ImGui-IO mapping table, and a note on the per-frame injection point and main-thread-only contract. Cross-reference from `application.md` (which documents main.cpp's platform-lifecycle role).

## Coordination

- Independent of every other TODO. Touches only `main.cpp` and a new `src/input/` file; no player/gui/domain changes.
- Constants could later be surfaced as **TODO_6** settings, but that is not required here.

## Order & verification

Per chunk: green **desktop** (`cmake --build cmake-build-debug`) **and Switch** (`cmake-build-switch`, per CLAUDE.md).

1. **10a** — on Switch hardware (ask before deploying): the drawn cursor moves with the left stick and settles on release; **A** clicks buttons/list rows, **X** opens context/secondary actions; existing quit-on-START still works. Desktop build unaffected (real mouse).
2. **10b** — right stick scrolls a long list smoothly; holding **L** gives fine control, holding **R** moves fast; the cursor never leaves the 1280×720 window; no jitter at rest (dead-zone correct).
