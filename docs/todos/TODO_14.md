# TODO_14 — Persist selected visualizer

> Single chunk. The `Settings` INI mechanism (TODO_6) already exists; this just adds a `[user] visualizer` key and restores it at startup, mirroring how `theme` is handled. Requires TODO_6 (Settings) + TODO_8 (visualizer system).

## Context

The visualizer selection lives solely in `VisualizerController::m_activeIndex` (`src/visualizer/VisualizerController.cpp:27`, initialised to `0`) and resets to index 0 on every launch — nothing persists it. The `onSelectVisualizer` callback (`src/main.cpp:288`) only calls `visualizer.select(i)`; it never touches `settings`, and startup never restores a saved choice.

The pattern to mirror already exists for the color theme (TODO_6): startup reads `[user] theme` and applies it; the change callback applies + `settings.setString(...)` + `settings.save()`, with the change flow routed through `Application` (which holds the `Settings &`). The Settings file is `Settings.{h,cpp}` in `src/settings/`; user settings are keyed under `[user]`.

Selection is currently surfaced to the UI via `UiState::activeVisualizer` (`src/gui/UiState.h:46`) and `VisualizerController::getNames()`/`getActiveIndex()`/`select(std::size_t)`.

## Task chunks (implement, verify, and commit one at a time)

- [x] **14a — Persist + restore the visualizer**: add a `[user] visualizer` INI key storing the **stable plugin name** (not the index — so adding/reordering plugins never selects the wrong one). On startup (`main.cpp`, after `visualizer.create()` and `settings.load(...)`) resolve the saved name against `VisualizerController::getNames()` and `select()` the match, falling back to index 0 if absent/unknown. In the change flow (route through `Application`, consistent with theme/plugin settings) write the chosen visualizer's name via `settings.setString("user", "visualizer", name)` + `settings.save()`. Add a name→index lookup helper on `VisualizerController` if one isn't already there. Verify: select a non-default visualizer, restart → it comes back selected; delete/garble the key → falls back to the first visualizer with no crash; `osp2.ini` shows the key.

This chunk ends with green desktop + Switch builds, docs updated (`docs/settings.md`, `docs/visualization.md`), user verification, then a commit. Run cpp-reviewer on the diff before committing.

## Files to change

1. **`src/main.cpp`** — startup restore: read `[user] visualizer`, resolve to an index, `visualizer.select(...)` (or feed the initial `UiState`).
2. **`src/Application.{h,cpp}`** — route `onSelectVisualizer` through the app layer so it persists (apply → `settings.setString` → `save()`), matching the theme change flow.
3. **`src/visualizer/VisualizerController.{h,cpp}`** — a `[[nodiscard]] std::optional<std::size_t> indexOf(const std::string &name) const` (or equivalent) if not already present.

No CMakeLists.txt change (no new file).

## Docs

- **`docs/settings.md`** — add `[user] visualizer` to the known-keys list (stable name, fallback-to-first rule).
- **`docs/visualization.md`** — note that the active selection is persisted via `Settings` and restored at startup.

## Coordination

- **Requires TODO_6** (Settings INI + `[user]` section + change flow through `Application`) and **TODO_8** (VisualizerController + selection API). Independent of the other new items.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- Select visualizer B (not the default), quit, relaunch → B is active and rendering.
- Hand-edit `[user] visualizer` to an unknown name → app starts on the first visualizer, no crash; removing the key behaves the same.
- Changing the selection rewrites `osp2.ini` (verify with `cat`).
