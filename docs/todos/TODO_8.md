# TODO_8 — Visualization plugin system (`VisualizerPlugin` + Bars visual)

> Fills the reserved VISUALIZATION-mode area from TODO_3 chunk 3d with a real, pluggable visualization system. Mirrors the `PlayerPlugin` pattern: one interface, interchangeable implementations, each free to render with ImGui primitives **or** a raw OpenGL shader-quad. Ships one basic implementation (vertical bars). Requires TODO_3 (view mode + reserved rect); chunk 8a is backend-only and independent.

## Context

TODO_3 chunk 3d adds a **VISUALIZATION view mode**: the workspace (browser, tabs, player bar) is hidden and the work area below the top bar is left empty — "reserved for the future visualization system." This item is that system.

Two facts from the current code shape the design:

- **Audio samples never leave the SDL audio thread.** PCM exists only inside `PlayerController::decode()` (`src/player/PlayerController.cpp:158`), under `m_mutex`. Any audio-reactive visual needs a new **lock-free tap** that publishes recent samples to the render thread — the audio thread must never block, and the reader must never touch `m_mutex`.
- **The render loop already draws ImGui over a black clear** (`src/main.cpp:181-231`). ImGui's `ImDrawList` paints the reserved rect portably (identical on desktop + Switch, no shader/CMake changes); a GL-based plugin bridges into the same frame via `ImDrawList::AddCallback`. So there is **one render call site and one draw order** regardless of a plugin's backend.

## Architecture

Three decoupled layers — the visualizer domain does **not** depend on `PlayerController`; `main.cpp` wires them.

```
audio thread          main / render thread
──────────────        ─────────────────────────────────────────────
decode() ──writes──▶ [AudioTap seqlock]  ──read──▶ main.cpp callback
(player domain)                                       │ builds VisualFrame
                                                      ▼
                            Gui (VISUALIZATION mode) ──onRenderVisualization(rect)──▶
                                                      │
                                                      ▼
                            VisualizerController.render(VisualFrame)
                                                      │
                                        ┌─────────────┴─────────────┐
                              BarsVisualizer (ImGui)     ShaderQuadVisualizer (raw GL, future)
```

- **Audio tap (player domain).** `src/player/AudioTap.h` — a header-only **seqlock** holding the latest block of interleaved stereo float frames (last 1024 frames). Single-producer (audio thread) / single-consumer (main thread), non-blocking on both sides, **independent of `m_mutex`**. `PlayerController` owns one, publishes the just-decoded block from `decode()`, and exposes `size_t readLatestAudio(float* out, size_t maxFrames) const` (seqlock read; 0 if never written; no lock). When not `PLAYING`, `decode()` publishes nothing, so the reader sees a stale block; `main.cpp` passes `frameCount = 0` so the visual decays to rest.

- **Visualizer domain (`src/visualizer/`, presentation — ImGui/GL deps allowed).**
  - `VisualFrame.h` — value struct passed to every plugin: reserved rect `float x, y, w, h` (screen coords) + audio view `const float* samples; size_t frameCount; int channels; int sampleRate;`. No ImGui types in the header.
  - `VisualizerPlugin.h` — abstract base, mirroring `PlayerPlugin`:
    ```cpp
    class VisualizerPlugin {
    public:
        virtual ~VisualizerPlugin() = default;
        virtual void create() = 0;                       // allocate GL objects if any
        virtual void destroy() = 0;                      // free them
        [[nodiscard]] virtual std::string getName() const = 0;
        virtual void render(const VisualFrame &frame) = 0;
    };
    ```
    `create()/destroy()` give a raw-GL plugin a clear lifecycle for its shader/VBO (as `PlayerPlugin` does for decoders); ImGui plugins leave them empty.
  - `VisualizerController.{h,cpp}` — mirrors `PlayerController`: owns `std::vector<std::unique_ptr<VisualizerPlugin>>`, an active index, `create()/destroy()`, `getNames()`, `select(size_t)`, and `render(const VisualFrame&)` forwarding to the active plugin. Registration is one `emplace_back` per plugin in `create()`.
  - `visualizers/BarsVisualizer.{h,cpp}` — the basic plugin (ImGui backend). Buckets the sample window into N segments, takes per-segment peak/RMS (**time-domain amplitude — no FFT, no dependency, Switch-safe**), keeps per-bar height state across frames for **attack/decay smoothing** (fast rise, gentle fall; decays toward 0 when `frameCount == 0`), and paints bars with `ImGui::GetBackgroundDrawList()->AddRectFilled(...)` inside the reserved rect. A true FFT **spectrum** and a GL **shader-quad** are natural follow-on plugins (8d/future).

- **Wiring (Gui + main.cpp).** In VISUALIZATION mode Gui invokes a new callback `onRenderVisualization(float x, float y, float w, float h)` with the reserved rect it already computes — Gui stays presentation-only and knows nothing about the visualizer domain, exactly like `onButtonClick`. `main.cpp` owns a `VisualizerController`, and the callback reads `player.readLatestAudio(...)` into a scratch buffer, builds a `VisualFrame`, and calls `visualizer.render(frame)`. It runs inside the ImGui frame, so ImGui plugins draw immediately and GL plugins schedule via `AddCallback` — single ordering, before `ImGui_ImplOpenGL3_RenderDrawData`.

## Task chunks (implement, verify, and commit one at a time)

- [x] **8a — Audio tap (backend, independent)**: `src/player/AudioTap.h` (seqlock) + `PlayerController` publish in `decode()` and `readLatestAudio()`. Pure player-domain addition; builds and verifies on its own (temporary `SDL_Log` of frames read during playback, removed before finishing). Depends on nothing — may land before TODO_3.
- [x] **8b — Visualizer skeleton + wiring**: `src/visualizer/` (`VisualFrame.h`, `VisualizerPlugin.h`, `VisualizerController.{h,cpp}`), the `onRenderVisualization` hook in Gui, and `main.cpp` wiring. Ship a trivial `DebugVisualizer` (rect fill + one moving line) to prove the pipeline in VISUALIZATION mode. Depends on TODO_3 3d (view mode + reserved rect) and 8a.
- [x] **8c — BarsVisualizer**: replace the debug plugin with the real vertical-bars visual (bucketed amplitude + attack/decay smoothing), registered as the default active plugin. The deliverable "basic plugin."
- [ ] **8d — (future / optional) GL shader-quad plugin + selector**: a `ShaderQuadVisualizer` rendering a fullscreen quad via a GLES-safe shader through `ImDrawList::AddCallback` (exercises the raw-GL path, surfaces Switch shader-version portability), plus a visualizer picker in the Settings menu calling `VisualizerController::select`. Follow-on; not required for the basic system.

Each chunk ends with green desktop + Switch builds, a docs update if classes/methods changed, user verification, then a commit.

## Files to change

1. **`src/player/AudioTap.h`** (new) — seqlock latest-block buffer.
2. **`src/player/PlayerController.{h,cpp}`** — `AudioTap m_audioTap;`, publish in `decode()`, add `readLatestAudio()`.
3. **`src/visualizer/`** (new): `VisualFrame.h`, `VisualizerPlugin.h`, `VisualizerController.{h,cpp}`, `visualizers/BarsVisualizer.{h,cpp}` (later `visualizers/ShaderQuadVisualizer.{h,cpp}`).
4. **`src/gui/Gui.{h,cpp}`** — VISUALIZATION mode calls `onRenderVisualization` with the reserved rect (in the TODO_1 world this lives on `UiActions`; otherwise a `drawUserInterface` parameter).
5. **`src/main.cpp`** — own a `VisualizerController`, `create()/destroy()` it, build the `onRenderVisualization` callback (read audio → `VisualFrame` → `render`).
6. **CMakeLists.txt** — add each new `.cpp` (`VisualizerController.cpp`, `BarsVisualizer.cpp`, …) to `add_executable`; headers need no entry. **No new external dependency** (bars are FFT-free).

## Docs

- **`docs/visualization.md`** (new) — Mermaid `classDiagram` of the visualizer domain + notes on the ImGui/GL render bridge (`AddCallback`) and the tap threading contract.
- **`docs/audio.md`** — add the `AudioTap` seqlock + `readLatestAudio`; document "reader never touches `m_mutex`; audio thread never blocks."
- **`docs/ui.md`** — the `onRenderVisualization` hook.

## Coordination

- **Requires TODO_3 chunk 3d** (VISUALIZATION view mode + reserved rect) for chunks 8b onward. Chunk 8a is backend-only and independent.
- TODO_2 (PlaybackStatus) is unrelated and not required.
- **TODO_6** (persisted settings) is where a chosen default visualizer would later be stored, alongside theme.

## Order & verification

Per chunk: green **desktop** (`cmake --build cmake-build-debug`) **and Switch** (`cmake-build-switch`, per CLAUDE.md); run `./cmake-build-debug/OSP2` from the repo root.

1. **8a** — play `romfs/music/test.s3m`; temporary log shows non-zero frames from `readLatestAudio` during playback, zero after stop; no audio glitches/underruns (the tap must not stall the audio thread).
2. **8b** — toggle into VISUALIZATION mode → the debug visual renders in the reserved area and animates; toggle back → workspace returns; **audio keeps playing across the toggle**; no ImGui Begin/End assert; clean exit.
3. **8c** — with a track playing, bars move with the music, rise on transients, and **decay smoothly to rest on stop/pause**; correct within the reserved rect at 1280×720; no CPU spike.
