# TODO_5 — Typed per-plugin track metadata (std::variant) driving the Metadata tab

> Requires TODO_1 (metadata travels as a `UiState` member, fetch logic lives in Application) and fills the Metadata tab created by TODO_3.

## Task chunks (implement, verify, and commit one at a time)

- [x] **5a — Metadata backend**: `src/player/Metadata.h`, `PlayerPlugin::getMetadata()` virtual + OpenMptPlugin capture-at-open, `PlayerController::getMetadata()`, Application fetch-on-track-change, `UiState::metadata`. Gui untouched (tab keeps its placeholder). Verify with a temporary `SDL_Log` of the fetched title/format on track change (remove before finishing).
- [ ] **5b — Metadata tab UI**: `drawFileMetadata` visit dispatch (no generic fallback), `drawModuleMetadata` field table + scrollable message region. Verify play/stop/auto-advance refresh the tab, and run the temporary dummy-alternative compile check.

Each chunk ends with green desktop + Switch builds, docs updated (`docs/audio.md`, `docs/application.md`, `docs/ui.md` as touched), user verification, then a commit.

## Context

Each decoder library exposes different metadata: libopenmpt has tracker/channels/patterns/samples/message; libgme will have game/system/copyright; libsidplayfp SID model/released; libsc68 hardware info. A single flat struct would be mostly-empty fields; a generic key/value list would forbid tailored rendering. Chosen approach: `std::variant` — one metadata struct per plugin family, `std::visit` in the Gui redirects to a per-type draw function. Adding a plugin then *forces* (compile error) the UI to handle its metadata shape.

## src/player/Metadata.h (new, header-only, SDL/ImGui-free)

```cpp
struct ModuleMetadata {              // tracker formats via libopenmpt
    std::string title;
    std::string artist;
    std::string format;              // e.g. "ScreamTracker 3" (type_long)
    std::string tracker;             // authoring tool, may be empty
    int channels;
    int patterns;
    int samples;
    int instruments;
    std::string message;             // song message, often multiline, may be empty
};

// One alternative per plugin family; monostate = no track loaded.
// Future: GmeMetadata, SidMetadata, Sc68Metadata added with their plugins.
using TrackMetadata = std::variant<std::monostate, ModuleMetadata>;
```

## Capture once, cache — the threading rule

The openmpt module is shared with the audio thread (guarded by `PlayerController::m_mutex`); reading it per frame would contend with `decode()`. So:

- **`PlayerPlugin`** gains `[[nodiscard]] virtual TrackMetadata getMetadata() const = 0;` — contract documented on the interface: returns a *cached* value, built during `open()`, cleared to `monostate` in `close()`; main thread only; never touches the decoder object.
- **`OpenMptPlugin`** — in `open()`, after the module is created: fill a `ModuleMetadata` member from `get_metadata("title"/"artist"/"type_long"/"tracker"/"message")` and `get_num_channels/patterns/samples/instruments()`. `getMetadata()` returns the member.
- **`PlayerController`** gains `[[nodiscard]] TrackMetadata getMetadata() const` — locks `m_mutex`, returns `m_activePlugin ? m_activePlugin->getMetadata() : TrackMetadata{}`.

## Application — fetch on track change, not per frame

`Application` gains members `TrackMetadata m_trackMetadata;` plus the path it was fetched for; in `update()`, if `player.getCurrentPath()` differs from the remembered path, refetch (`m_trackMetadata = player.getMetadata()`). Covers manual play, auto-advance, and stop (path resets → monostate → tab shows the empty state). `UiState` gains `const TrackMetadata &metadata;`, filled by `makeUiState()`.

## Gui — visit-based dispatch

- The Metadata tab body (placeholder since TODO_3) becomes `drawFileMetadata(const TrackMetadata &)`:
  ```cpp
  template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };  // Gui.cpp-local
  std::visit(overloaded{
      [](std::monostate)              { /* centered "No track loaded" */ },
      [this](const ModuleMetadata &m) { drawModuleMetadata(m); },
  }, metadata);
  ```
  Deliberately NO generic `auto` fallback — a new variant alternative must fail compilation until its draw function exists.
- `drawModuleMetadata(const ModuleMetadata &)` — two-column field table (skip empty string fields); below it, if `message` is non-empty, a scrollable child region with `TextUnformatted` + word wrap (never printf-format user data).

## Docs

- **`docs/audio.md`** — add Metadata.h types to the player classDiagram + a note on the capture-once-at-open threading rule.
- **`docs/ui.md`** — Metadata tab dispatch (visit, one draw per alternative, no fallback).
- **`docs/application.md`** — fetch-on-change member + UiState.metadata.

No CMakeLists.txt change (Metadata.h is header-only; other edits are in existing files).

## Coordination

- **Requires TODO_1** (UiState member, Application fetch logic) and **TODO_3** (the Metadata tab this fills).
- **Future plugins** (libgme/libsidplayfp/libsc68): each adds its struct + variant alternative + one `drawXxxMetadata` — the compile error from the exhaustive visit is the checklist.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- Run from repo root: play `romfs/music/test.s3m` → Metadata tab shows real title/format/channels/patterns/samples (+ message block if the file has one); STOP → tab returns to "No track loaded"; NEXT/auto-advance refreshes the fields; a rejected garbage file leaves metadata unchanged.
- Compile-time check: temporarily add a dummy variant alternative and confirm the visit fails to compile (then remove) — proves the exhaustiveness guard works.
