# Visualization domain

Presentation layer in `src/visualizer/`. A pluggable, audio-reactive visualization system mirroring the `PlayerPlugin` pattern: one interface, interchangeable implementations, each free to render with ImGui primitives **or** raw OpenGL. Unlike the player domain, the visualizer layer may depend on ImGui/GL — but it does **not** depend on `PlayerController` or `Gui`; `Application` wires them together, and `Platform` owns the controller's lifecycle and restores the persisted selection at startup (see [application.md](application.md), [platform.md](platform.md)).

```mermaid
classDiagram
    class VisualFrame {
        <<value object>>
        +float x
        +float y
        +float w
        +float h
        +const float* samples
        +size_t frameCount
        +int channels
        +int sampleRate
    }

    class VisualizerPlugin {
        <<abstract>>
        +create()
        +destroy()
        +getName()* string
        +render(frame)*
    }

    class BarsVisualizer {
        -int BAR_COUNT$
        -array~float~ m_levels
        +getName() string
        +render(frame)
    }

    class ShaderQuadVisualizer {
        -uint m_program
        -uint m_vao
        -int m_locTime
        -int m_locLevel
        -float m_time
        -float m_level
        -int m_vpX m_vpY m_vpW m_vpH
        +create()
        +destroy()
        +getName() string
        +render(frame)
        -glDrawCallback(parentList, cmd)$
    }

    class VisualizerController {
        -vector~unique_ptr~VisualizerPlugin~~ m_plugins
        -size_t m_activeIndex
        +create()
        +destroy()
        +getNames() vector~string~
        +indexOf(name) optional~size_t~
        +getActiveIndex() size_t
        +select(index)
        +render(frame)
    }

    VisualizerController "1" o-- "*" VisualizerPlugin : owns, dispatches to active
    VisualizerPlugin <|-- BarsVisualizer : ImGui backend
    VisualizerPlugin <|-- ShaderQuadVisualizer : raw GL backend
    VisualizerPlugin ..> VisualFrame : render(frame)
    VisualizerController ..> VisualFrame : forwards to active plugin
```

## Notes

- **`VisualFrame`** (`src/visualizer/VisualFrame.h`) is the per-frame input: the reserved screen rect (`x, y, w, h`) plus a read-only view of the most recently decoded audio (`samples` — interleaved stereo, `frameCount`, `channels`, `sampleRate`). It is deliberately **free of ImGui/GL types** so it can be built in the use-case layer (`Application`) without dragging the presentation backend into the audio wiring. `samples` may be `nullptr` when `frameCount == 0`; `frameCount == 0` means nothing is playing, so a visual should decay to rest rather than react.

- **`VisualizerPlugin`** (`src/visualizer/VisualizerPlugin.h`) is the abstract base, mirroring `PlayerPlugin`. **The two-phase `create()`/`destroy()` lifecycle is load-bearing**: both are called once per plugin from `VisualizerController::create()`/`destroy()` on the main thread, with a **live GL context** — the controller itself is constructed before GL exists, and its GL objects must be freed before the context dies (`Platform::destroy()` calls `m_visualizer.destroy()` before deleting the GL context). They are plain virtuals with **no-op default bodies**, so ImGui-only plugins simply don't override them. `render(frame)` runs inside the ImGui frame (between `NewFrame` and `Render`) once per frame while in VISUALIZATION mode.

- **`VisualizerController`** (`src/visualizer/VisualizerController.{h,cpp}`) mirrors `PlayerController`'s ownership pattern: it owns a `vector<unique_ptr<VisualizerPlugin>>` and an active index (default 0), with one `emplace_back` per plugin in `create()` (registration order = selector order = dispatch index). Registration order is `BarsVisualizer` ("Bars", index 0, default active) then `ShaderQuadVisualizer` ("Plasma", index 1). `create()`/`destroy()` run each plugin's own `create()`/`destroy()` — for **all** registered plugins, so GL resources exist regardless of which one is active; selecting only changes which one renders. `getNames()` feeds the Settings→Visualizer picker, `getActiveIndex()` reports the checkmark, `select(index)` (bounds-checked) switches at runtime, and `indexOf(name)` resolves a stable plugin name back to its index (`nullopt` if unknown) for the startup restore. It is non-copyable and guards `render()` against an out-of-range index, so `render()` is a safe no-op before `create()` / after `destroy()`.

- **`BarsVisualizer`** (`src/visualizer/visualizers/BarsVisualizer.{h,cpp}`, ImGui backend — keeps the no-op `create()`/`destroy()` defaults). Draws `BAR_COUNT` (64) vertical bars whose heights track per-band audio amplitude. Bands are **time-domain**: the sample window is bucketed into `BAR_COUNT` contiguous buckets and each bar's target is the **peak of |mono|** over its bucket (peak, not RMS → transients pop) times an empirical visual `GAIN`, clamped to 1. No FFT → no dependency, Switch-safe. Per-bar heights are smoothed with a fast **attack** / gentle **decay** and persisted across frames in `m_levels`, so bars rise sharply and fall smoothly; sub-`1e-4` levels snap to a true 0 so resting bars stop drawing entirely. When `frameCount == 0` all targets are 0, so the bars **decay to rest**. Drawn via `ImGui::GetBackgroundDrawList()->AddRectFilled(...)` inside the reserved rect, growing bottom→up, tiled flush to both edges (gaps only *between* bars), using the theme accent (`ImGuiCol_PlotHistogram`) for consistency with the player bar.

- **`ShaderQuadVisualizer`** (`src/visualizer/visualizers/ShaderQuadVisualizer.{h,cpp}`, raw-GL backend, `getName()` `"Plasma"`). Renders an animated plasma with a **fullscreen-triangle** fragment shader (attribute-less: the triangle's clip-space corners are derived from `gl_VertexID`, so no VBO — only an empty VAO, which core profile still requires for `glDrawArrays`). Rather than draw immediately, `render()` schedules the GL draw on the background draw list via **`ImDrawList::AddCallback`** and immediately queues `ImDrawCallback_ResetRenderState`; the GL3 backend runs the callback during `ImGui_ImplOpenGL3_RenderDrawData`, then the reset restores the backend's own viewport/scissor/program/blend for the ImGui geometry that follows (the callback deliberately leaves state dirtied and restores nothing itself). The draw is **confined to the reserved rect** with `glViewport` + `glScissor` over the rect converted to framebuffer pixels and **Y-flipped** (GL's framebuffer origin is bottom-left, ImGui's screen origin is top-left; `DisplayFramebufferScale` handles HiDPI backing). The shader uses **`#version 330 core`** — the same version `Platform` feeds `ImGui_ImplOpenGL3_Init`, portable on desktop **and** Switch. It is audio-reactive two ways: `m_level` is an **envelope-followed loudness** (mean of |mono| over the sample window, gained and clamped to `[0, 1]`, then smoothed with a quick attack / gentle decay so the visual reacts without flickering; it eases to 0 when idle), fed to the `u_level` uniform (swells the radial ripple, lifts brightness); and the animation clock `m_time` advances at a **loudness-paced rate** (`dt * (BASE_SPEED + SPEED_GAIN * m_level)` accumulated inside `render()` — never `GetTime()`), so the plasma visibly churns faster when loud and keeps a small idle churn when quiet. `create()` compiles/links the program (logging any compile/link info log via `SDL_Log` and leaving `m_program == 0` on failure, which makes `render()` a safe no-op); `destroy()` frees the program and VAO.

- **Animation freezes when hidden.** A visualizer must advance its animation/state from the per-frame delta **inside `render()`** (`ImGui::GetIO().DeltaTime`), never from the always-ticking global `ImGui::GetTime()`. Because the controller's `render()` is only called in VISUALIZATION mode, deriving motion from render-time makes a visualizer stop while hidden and resume where it left off — the bars freeze mid-decay and resume from the same heights, the plasma clock stops, no phase jump on re-entry, no work when off-screen. Audio-reactive state gets this for free (it only updates when fed a frame).

## Render bridge (ImGui / GL)

There is **one render call site and one draw order** regardless of a plugin's backend, because `render()` runs inside the ImGui frame:

- **ImGui plugins** (`BarsVisualizer`) draw **immediately** via `ImGui::GetBackgroundDrawList()` — the background list paints behind every window, directly on the reserved rect in screen coordinates. Portable and identical on desktop + Switch (no shader/CMake changes).
- **GL plugins** (`ShaderQuadVisualizer`) schedule their draw through `ImDrawList::AddCallback` on the same background draw list, so the raw-GL rendering is ordered into the same ImGui draw data and executed inside `ImGui_ImplOpenGL3_RenderDrawData`, before the backend draws its own geometry; the plugin queues `ImDrawCallback_ResetRenderState` right after so the backend restores its render state for the following ImGui geometry.

The single call site is `Application::handleRenderVisualization` (wired onto `UiActions` as `onRenderVisualization`): in VISUALIZATION mode `Gui` invokes it with the reserved rect (`viewport->WorkPos`/`WorkSize`, which already exclude the main menu bar); the handler reads the audio tap, builds a `VisualFrame`, and calls `visualizer.render(frame)`. `Gui` stays presentation-only and knows nothing about the visualizer domain — same principle as `onButtonClick` (see [ui.md](ui.md)).

**Selector (Settings→Visualizer).** The active visualizer is switched at runtime from a **Settings→Visualizer** submenu that lists each registered plugin name with a checkmark on the active one. The wiring keeps `Gui` ignorant of the visualizer domain: `Application::makeUiState()` fills `UiState::visualizerNames` (a non-owning view of its startup-built name cache — `getNames()` allocates, so it is called once via `refreshVisualizerNames()`, never per frame) and `UiState::activeVisualizer` (from `getActiveIndex()`), and `UiActions::onSelectVisualizer` routes to `Application::handleSelectVisualizer(index)`. `Gui` merely lists the names and reports the picked index.

**Persistence.** The active selection is persisted through `Settings`, alongside the theme and by the same flow: `Application::handleSelectVisualizer` selects the plugin, then writes the chosen plugin's stable `getName()` to `[user] visualizer` (`setString` + `save()`) — mirroring `handleThemeChange` (see [application.md](application.md) / [settings.md](settings.md)). At startup `Platform` (the composition root) reads that key and, when non-empty, resolves it via `VisualizerController::indexOf(name)` and `select`s the result; an empty or unknown name leaves the controller's default (index 0). Storing the **stable name** rather than the index keeps the choice valid across reordering of the plugin registration.

## Audio tap threading contract

The waveform reaches the visualizer through the player domain's lock-free **seqlock** tap (see [audio.md](audio.md)): the audio thread publishes the just-decoded block from `decode()`; `Application` reads it with `PlayerController::readLatestAudio()` (never touches `m_mutex`, never blocks the audio thread). `decode()` publishes nothing when not `PLAYING`, so an ungated read returns a **stale** block — `Application` gates on `PlayerState::PLAYING` and passes `frameCount = 0` otherwise, so the visual **decays to rest** rather than reacting to a frozen buffer. The read buffer is zero-initialized at the call site, so the idle and partial-read tails are silence — a plugin that reads `samples` without gating on `frameCount` stays safe.
