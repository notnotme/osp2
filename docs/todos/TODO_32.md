# TODO_32 — UI seam hardening (visualizer into Application, designated initializers, Gui helpers take the bundles, SettingsKeys)

> Structural hardening of the Platform → Application → Gui seam, from the 2026-07 architecture audit. Four related refactors, all behavior-preserving (the UI must look and behave identically): move the visualizer bridge from `Platform` into `Application` (**architecture decision made 2026-07-08: approved**), switch the `UiState`/`UiActions` aggregates to designated initializers, make the private Gui draw helpers take `(state, actions)` instead of 8–9 positional parameters, and single-source the INI section/key literals in a `SettingsKeys.h`.

## Context

- **Visualizer bypass (C1-F2, decision: move it).** `Platform::run()` wires `onRenderVisualization`/`onSelectVisualizer` by mutating the actions bundle after `makeUiActions()` (`src/Platform.cpp:76-98`), patches `visualizerNames`/`activeVisualizer` onto `UiState` after `makeUiState()` (`:158-160`), and persists `user/visualizer` (`:95-96`) — while the structurally identical theme flow persists inside `Application::handleThemeChange` (`Application.cpp:194-197`). Nothing in those callbacks needs platform facilities (they touch only `PlayerController`, `VisualizerController`, `Settings`, `VisualFrame`), `Platform` is a second `UiState` producer, and `m_visualizer.getNames()` allocates a `vector<string>` **every frame** (`:159`). docs/platform.md:13-14 declares the current shape deliberate — that rationale is hereby reversed. The startup *restore* (`Platform.cpp:314-318`) and the QUIT interception (`:104-111`) stay in `Platform` — genuinely composition-root/platform work.
- **Positional aggregates (C1-F1).** `makeUiActions()` returns 12 positional lambdas (`Application.cpp:419-438`) into a struct with colliding member types (three `void(const FileEntry&)`, two `void(size_t)`, three `void()`); `makeUiState()` returns 13 positional fields (`:402-416`) with adjacent same-typed strings/bools. A mid-list insertion — the seam's documented growth mode — compiles cleanly while silently swapping handlers or flags. C++20 designated initializers fix this for zero structure (fine on devkitPro GCC).
- **Exploded parameter lists in Gui (C2-F1).** The parameter objects already exist (`UiState`/`UiActions`) but the private helpers don't take them: `drawTopBar` (9 params), `drawFileBrowser` (9, including **three adjacent** `const std::function<void(const FileEntry&)>&`), `drawTabsSection` (9), `drawTabPlaylist` (8) — `src/gui/Gui.h:96-154`. `drawUserInterface` unpacks the structs positionally (`Gui.cpp:978-989, 1039-1049, 1056-1066`) and `drawTabsSection` re-relays the same 8 args to `drawTabPlaylist` (`:685-695`). The docs class diagram has already drifted stale (ui.md shows 4 params vs 9 in code) — the signatures churn faster than anyone maintains them. Keep `playingFileName` a separate param: it is *derived* in `drawUserInterface` (`Gui.cpp:1034-1035`), not a `UiState` field, and both panes share the one computation.
- **Stray abstraction (C2-F2).** `virtual ~Gui()` (`Gui.h:93`) on a class with zero subclasses, owned by value, never deleted through a base pointer — falsely advertises a polymorphic seam. Also: stale include guard `OSP2_WINDOW_SYSTEM_H` (`Gui.h:20-21, 164`), and the centered-dimmed-placeholder idiom duplicated at `Gui.cpp:706-714` ("No track loaded") vs `:802-810` ("Playlist is empty").
- **Stringly-typed settings keys (C6-F1).** `"user"`/`"theme"`/`"visualizer"`/`"default_folder"`/`"plugin."`/`"source."` literals are spread across `Platform.cpp:95, 295, 302, 314, 329, 369-382`, `Application.cpp:195, 222`, `Settings.cpp:121-124` — save and restore sites live in *different files*, so a typo compiles silently and forks the key (the setting would silently reset every launch).

## Architecture

- `Application` gains a fifth domain reference, `VisualizerController &m_visualizer` (constructor param; `Platform.h:58-64` declaration order already puts `m_visualizer` before `m_app`, so binding is safe — update the order comment at `Platform.h:56-57`). After this TODO, `Application::makeUiState()` is again the **single** producer of the view model and `Application` the single home of "user picked X → persist X" flows; `Platform::run()` is pure lifecycle/loop.
- New header-only `src/settings/SettingsKeys.h` (GPL header, no CMake change): `namespace settingskeys { inline constexpr const char *kUserSection = "user"; ... kTheme, kVisualizer, kDefaultFolder, kPluginSectionPrefix = "plugin.", kSourceSectionPrefix = "source.", kSourceHost, kSourcePath; }` — the INI schema's single source of truth.

## Task chunks (implement, verify, and commit one at a time)

- [x] **32a — Visualizer bridge into Application + designated initializers**: add `VisualizerController&` to `Application`; move the `onRenderVisualization`/`onSelectVisualizer` bodies from `Platform::run()` (`Platform.cpp:76-98`) into `Application` handlers wired inside `makeUiActions()`; fill `visualizerNames`/`activeVisualizer` inside `makeUiState()` and delete the post-hoc patch (`Platform.cpp:158-160`) plus the field-default comment in `UiState.h:53-56`; cache the names (they change only at startup) to kill the per-frame allocation. In the same edit, rewrite `makeUiState()`/`makeUiActions()` (`Application.cpp:402-438`) with designated initializers naming every member. Startup restore stays in `Platform`.
- [x] **32b — Gui draw helpers take the bundles**: change `drawTopBar`, `drawFileBrowser`, `drawTabsSection`, `drawTabPlaylist`, `drawPluginPopups` to take `(const UiState &state, const UiActions &actions[, const std::string &playingFileName])` (`Gui.h:96-154`); reference `state.files`, `actions.onFileClick`, etc. directly; delete the positional relay lists (`Gui.cpp:978-989, 1039-1049, 1056-1066`) and the `drawTabsSection`→`drawTabPlaylist` double relay (`:685-695`). `drawPlayerBar` and the four `drawXxxMetadata` stay as-is (each already takes one cohesive object). Fold-ins: `class Gui final` + drop `virtual ~Gui()` (`Gui.h:93`); guard rename `OSP2_WINDOW_SYSTEM_H` → `OSP2_GUI_H`; file-local `centeredDisabledText(const char *)` helper deduping `Gui.cpp:706-714`/`802-810`.
- [x] **32c — SettingsKeys.h**: create the header; replace the ~11 literal sites in `Platform.cpp`, `Application.cpp`, and `Settings::applyDefaults` (`Settings.cpp:121-124`). With 32a done, both `kVisualizer` sites end up in one file.

## Files to change

1. **`src/Application.{h,cpp}`** — `VisualizerController&`, visualizer handlers, designated-init `makeUiState`/`makeUiActions`, settings keys (32a, 32c).
2. **`src/Platform.{h,cpp}`** — drop the bridge wiring and UiState patch, update the member-order comment, settings keys (32a, 32c).
3. **`src/gui/Gui.{h,cpp}`** — helper signatures, `final`, guard, `centeredDisabledText` (32b).
4. **`src/gui/UiState.h`** — field-default comment cleanup (32a).
5. **`src/settings/SettingsKeys.h`** (new, header-only) + **`src/settings/Settings.cpp`** (32c).

## Docs

- **`docs/platform.md`** — remove the "visualizer bridge lives here" rationale (13-14); update the Mermaid diagram (drop Platform→visualizer wiring, keep ownership).
- **`docs/application.md`** — add `VisualizerController&`; note the aggregates are designated-initialized so seam extensions are order-safe.
- **`docs/ui.md`** — fix the stale class-diagram signatures (lines 21-33) to the new `(state, actions[, playingFileName])` forms; fix line 139 (says the bridge is "wired in main.cpp"); trim the "threaded through" narration (~153).
- **`docs/visualization.md`** — whichever end of the bridge it documents.
- **`docs/settings.md`** — mention `SettingsKeys.h` as the schema's single source of truth.

## Coordination

- Requires the C1-F2 decision (**made: move**). TODO_33 depends on 32b's helper signatures. Independent of TODO_31/TODO_34.

## Verification

- Desktop + Switch builds green each chunk; the UI is pixel-identical and behaves identically throughout.
- 32a: visualizer picker still switches and persists across restarts; theme flow unchanged; visualization renders in both view modes.
- 32b: all tabs, popups, browser, playlist render and act as before.
- 32c: theme/visualizer/default_folder/plugin/source settings all persist and restore exactly as before (check the INI on disk keeps the same keys).
