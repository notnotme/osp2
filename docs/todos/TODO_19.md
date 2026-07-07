# TODO_19 — Loop-song setting (per-plugin, via each decoder's own loop)

> Single chunk. Expose "Loop" as a **per-plugin setting** on every decoder whose library natively supports looping, driven through the existing `PluginSetting` pipeline — not an app-level re-play and not a global toggle. libopenmpt, libgme, and libsc68 all provide a loop mechanism; libsidplayfp loops forever inherently so needs no setting. Settings that only take effect on the next track carry an "applies on the next track" hint in the popup. Requires TODO_6 (Settings) and TODO_9 already in place.

## Context

The player already has a complete per-plugin settings pipeline (TODO_6): a plugin publishes `PluginSetting` descriptors from `getSettings()` and mutates its live decoder in `applySetting(key, value)`; the Gui renders one popup per plugin (checkmark/slider/combo widgets), live edits fire `onPluginSettingChange → PlayerController::applyPluginSetting` (under `m_mutex`), commits fire `onPluginSettingCommit → Settings::setInt` (persisted under `[plugin.<name>]`), and startup pushes the persisted values back through `applySetting` (`Platform.cpp`). **Adding a loop toggle is therefore just one more `PluginSetting`** per capable plugin — no `Application`, `UiState`, `UiActions`, `Platform`, or `Gui` state change; persistence, the popup UI, live apply, and startup restore all come for free.

Each library exposes looping differently, and — crucially — with different timing:

- **libopenmpt** — `set_repeat_count(-1)` (vs `0`). Takes effect on the **current** playthrough, so it is a **live** setting (no restart). `OpenMptPlugin` already caches `m_stereoSeparation`/`m_interpolation` this way; loop joins them.
- **libgme** — `gme_set_autoload_playback_limit(emu, 0)` disables the play-length termination so a looping track repeats forever. The limit is loaded at `gme_start_track`, so the flag must be set **before** the track starts → a **deferred** setting (next track). Applied in `GmePlugin::startTrack` before `gme_start_track` (so both the initial open and every subtrack switch honour it).
- **libsc68** — `sc68_play(track, SC68_INF_LOOP)` (vs `SC68_DEF_LOOP`). `sc68_play` only posts a track change (applied at the next `sc68_process`) and `SC68_CUR_TRACK` is a getter, so loop cannot be set live → a **deferred** setting (next open), exactly like the existing `asid` setting.
- **libsidplayfp** — SID tunes run 6502 code in an endless loop by nature; `getDuration()` returns 0 and `decode()` never signals end-of-track. SID already loops forever, so it exposes **no** loop setting.

Because loop suppresses end-of-track, an enabled loop keeps the current tune/subtrack playing indefinitely and never auto-advances — the intended "loop song" behaviour, consistent across formats.

**Deferred-setting hint.** `PluginSetting` gains a `bool appliesOnNextTrack` flag; when set, the settings popup renders a small greyed "Applies on the next track" line under the widget. This covers the GME/sc68 `loop` settings, plus the pre-existing deferred settings that lacked any such cue: sc68 `asid` and SID `sid_model`/`clock` (all documented as next-open in their `applySetting`). Live settings (OpenMpt's three, GME `stereo_depth`/`accuracy`) leave the flag false and show no hint.

## Task chunks (implement, verify, and commit one at a time)

- [x] **19a — Per-plugin loop settings + deferred hint**: add `appliesOnNextTrack` to `PluginSetting` and render the hint in `Gui::drawPluginPopups`; add a `loop` `EnumOptions{{"Off","On"}}` setting to `OpenMptPlugin` (live → `set_repeat_count`), `GmePlugin` (deferred → `gme_set_autoload_playback_limit` in `startTrack`), and `Sc68Plugin` (deferred → `sc68_play` `SC68_INF_LOOP` in `open()`); mark the deferred settings (GME/sc68 `loop`, sc68 `asid`, SID `sid_model`/`clock`) with the flag. `SidPlugin` gets no loop setting. Verify: each of the libopenmpt / libgme / libsc68 popups shows a "Loop" Off/On toggle; On makes a finishing tune loop instead of auto-advancing; Off restores auto-advance; the deferred toggles show the "applies on the next track" hint and OpenMpt's does not; choices persist to `[plugin.<name>] loop` and survive a restart.

This chunk ends with green desktop + Switch builds, docs updated (`docs/audio.md`), user verification, then a commit. Run cpp-reviewer on the diff before committing.

## Files to change

1. **`src/player/PluginSetting.h`** — `bool appliesOnNextTrack = false;`.
2. **`src/gui/Gui.cpp`** — render the hint under a setting when the flag is set (in `drawPluginPopups`).
3. **`src/player/plugins/OpenMptPlugin.{h,cpp}`** — live `loop` via `set_repeat_count`.
4. **`src/player/plugins/GmePlugin.{h,cpp}`** — deferred `loop` via `gme_set_autoload_playback_limit` in `startTrack`.
5. **`src/player/plugins/Sc68Plugin.{h,cpp}`** — deferred `loop` via `sc68_play` `SC68_INF_LOOP`; mark `asid` deferred.
6. **`src/player/plugins/SidPlugin.cpp`** — mark `sid_model`/`clock` deferred (no loop setting).

No CMakeLists.txt change (no new file). No `Application`/`UiState`/`UiActions`/`Platform`/`Settings` change — the plugin-settings pipeline carries it end to end.

## Docs

- **`docs/audio.md`** — the per-plugin loop settings (which library facility each uses, live vs deferred), the `appliesOnNextTrack` flag driving the popup hint, and that SID loops inherently so exposes no setting.

## Coordination

- **Requires TODO_6** (plugin-settings pipeline) and **TODO_9**. Uses each decoder's own loop facility (per-plugin), not an app-level re-play or a global toggle — plugins gain looping only where the underlying library provides it. Interacts with TODO_18: an enabled loop keeps the current subtrack playing and suppresses subtrack/file auto-advance, by design.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- libopenmpt / libgme / libsc68 popups each show a "Loop" Off/On toggle; On → the tune loops instead of advancing; Off → the next track/file plays.
- Deferred toggles (GME/sc68 loop, sc68 asid, SID model/clock) show "Applies on the next track"; OpenMpt's loop does not.
- `osp2.ini` gains `[plugin.<name>] loop`; relaunch → restored and applied.
- SID playback unaffected (no loop setting); no crash.
