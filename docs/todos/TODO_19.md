# TODO_19 — Loop-song setting

> Single chunk. Add a `[user] loop` toggle: when enabled, a finished track re-plays instead of auto-advancing to the next file. Implemented entirely at the `Application` level (re-play the current path on track end) — format-agnostic and lowest risk. Requires TODO_6 (Settings).

## Context

End-of-track auto-advance is a single branch: `Application::update` polls `m_player.consumeTrackEnded()` and calls `playAdjacentTrack(+1)` (`src/Application.cpp:175–177`). A loop toggle inserts one condition here — if loop is on, `m_player.play(m_player.getCurrentPath())` instead of advancing. End-of-track is signalled uniformly (any plugin decoding fewer frames than requested, `PlayerController.cpp:230–240`), so this works for every format without touching a plugin.

The `Settings` INI mechanism (TODO_6) is the persistence path. It has **no** `getBool`/`setBool` — booleans are stored as `0`/`1` via `setInt`/`getInt` (`src/settings/Settings.h:52–63`). Defaults are seeded in `Settings::applyDefaults()` (`src/settings/Settings.cpp:110–111`, currently `[user] theme` + `default_folder`). The change flow to mirror is `theme`: read at startup near `main.cpp:167–168`; persisted in `Application::handleThemeChange` (`Application.cpp:112–116`, `setString` + `save()`); the callback lives on `UiActions` (`src/gui/UiActions.h:37`) wired in `Application::makeUiActions` (`Application.cpp:213`).

The UI toggle belongs in the Settings menu of `Gui::drawTopBar` (`src/gui/Gui.cpp:201–252`), using the checkmark-`MenuItem` idiom already used for Theme and the Visualizer picker (`Gui.cpp:246`: `ImGui::MenuItem(label, nullptr, isSelected)`). The state is surfaced through `UiState` (`src/gui/UiState.h:35–47`), populated in `Application::makeUiState` (`Application.cpp:187–206`).

**Notes for the spec (out of scope, but call them out):** SID tunes loop indefinitely and may never signal end-of-track (`src/player/plugins/SidPlugin.cpp:284`) — the toggle is simply moot for them, which is harmless. A per-format *seamless* loop (e.g. libopenmpt `set_repeat_count(-1)`, currently hardcoded to `0` at `OpenMptPlugin.cpp:68`) is a separate future enhancement, not part of this simple re-play approach.

## Task chunks (implement, verify, and commit one at a time)

- [ ] **19a — Persist + honour the loop toggle**: add `[user] loop = 0` to `applyDefaults()`; read it at startup (`getInt("user","loop",0)`) and feed it into `Application`; add an `onLoopToggle(bool)`-style callback to `UiActions`, wired in `makeUiActions`, handled by a new `Application::handleLoopToggle` that updates in-memory state, `settings.setInt("user","loop",v)`, and `settings.save()` (mirroring `handleThemeChange`); surface a `bool loop` on `UiState` via `makeUiState`; render a "Loop song" checkmark `MenuItem` in the Settings menu; and branch the auto-advance in `update()` (`Application.cpp:175–177`) to re-play the current path when loop is on. Verify: enable loop → a finished track restarts from the top; disable → auto-advance to the next file resumes; `osp2.ini` shows `[user] loop`; the setting survives a restart.

This chunk ends with green desktop + Switch builds, docs updated (`docs/settings.md`, `docs/ui.md`, `docs/application.md`), user verification, then a commit. Run cpp-reviewer on the diff before committing.

## Files to change

1. **`src/settings/Settings.cpp`** — seed `[user] loop = 0` in `applyDefaults()`.
2. **`src/main.cpp`** — read `[user] loop` at startup and pass the initial value into `Application`.
3. **`src/Application.{h,cpp}`** — hold the loop flag, `handleLoopToggle` (apply → `setInt` → `save`), branch the `consumeTrackEnded()` handler, surface the flag in `makeUiState`, wire the callback in `makeUiActions`.
4. **`src/gui/UiActions.h`** — new `onLoopToggle` callback (near `onThemeChange`).
5. **`src/gui/UiState.h`** — `bool loop`.
6. **`src/gui/Gui.cpp`** — "Loop song" checkmark `MenuItem` in the Settings menu.

No CMakeLists.txt change (no new file).

## Docs

- **`docs/settings.md`** — document `[user] loop` (0/1, default 0).
- **`docs/ui.md`** — the "Loop song" Settings-menu toggle.
- **`docs/application.md`** — the auto-advance branch (loop → re-play current path vs. advance).

## Coordination

- **Requires TODO_6** (Settings INI + `[user]` section + change flow through `Application`). Independent of the other new items. Interacts conceptually with TODO_18 (subtrack auto-advance) — loop should re-play the current subtrack, not force file advance; keep both branches consistent if TODO_18 lands first.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- Toggle Loop on → let a track finish → it restarts; toggle off → the next file plays.
- `osp2.ini` gains `[user] loop`; relaunch → the toggle state is restored.
- SID (endless) playback is unaffected; no popup or crash when looping is on for a format that never signals end.
