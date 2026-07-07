# TODO_18 — Subtrack navigation (NEXT/PREVIOUS step subtracks in multi-track formats)

> Two chunks. Many chiptune formats pack several subtracks into one file — GME especially (NSF, SPC, GBS, VGM, KSS, …). Today the player always plays subtrack 0 and every subtrack beyond it is unreachable. Make NEXT/PREVIOUS step through subtracks first and fall through to the next/previous **file** only at the ends. Requires TODO_9 (GmePlugin).

## Context

**No subtrack concept exists above the plugin.** `PlayerPlugin` (`src/player/PlayerPlugin.h`, virtuals L41–70) has `open`/`close`/`decode`/`getTitle`/`getDuration`/`getMetadata`/settings — but nothing about a track count or track selection. The only subtrack datum anywhere is `GmeMetadata::trackCount` (`src/player/Metadata.h:49`), which is **display-only**: nothing consumes it to switch tracks.

**GME already has everything needed at the library level.** `GmePlugin::open` calls `gme_start_track(m_emu.get(), 0)` (`src/player/plugins/GmePlugin.cpp:81`) — the track index is hardcoded to `0` — and `gme_track_count(...)` (`GmePlugin.cpp:102`) to fill `trackCount`. Selecting another subtrack is just `gme_start_track(emu, n)` on the **already-open** emulator: no re-parse, no reopen. But the plugin caches `m_title` / `m_duration` / `m_metadata` during `open()` (`GmePlugin.h:37–41`), so a live subtrack switch must **refresh those caches under the lock** (re-read `gme_track_info` for the new index).

**Threading.** Everything touching `m_activePlugin` in `PlayerController` runs under `m_mutex` (contract in `PlayerController.h:49–59`); `applyPluginSetting` is the existing "locked pass-through to the active plugin" pattern to mirror for a new `selectSubtrack(int)`. The audio thread signals end-of-track by decoding fewer frames than requested → `m_trackEnded` (`PlayerController.cpp:230–240`); teardown never happens on the audio thread.

**Navigation lives in the app layer.** `ButtonId` = `{PLAY_PAUSE, STOP, NEXT, PREVIOUS}` (`src/gui/ButtonId.h:24–29`). `Application::handleButtonClick` routes NEXT/PREVIOUS straight to `playAdjacentTrack(±1)` (`src/Application.cpp:53–58`), which scans the current directory for the next/previous **file** (`Application.cpp:81–110`). Auto-advance on track end funnels through the same path: `Application::update` → `consumeTrackEnded()` → `playAdjacentTrack(+1)` (`Application.cpp:175–177`). `UiState` (`src/gui/UiState.h:35–47`) carries no track index today.

## Task chunks (implement, verify, and commit one at a time)

- [x] **18a — Plugin subtrack API + GME implementation + controller pass-through (backend)**: add three virtuals to `PlayerPlugin` with safe defaults so non-multi-track plugins need no change — `[[nodiscard]] virtual int getSubtrackCount() const { return 1; }`, `[[nodiscard]] virtual int getCurrentSubtrack() const { return 0; }`, `virtual void selectSubtrack(int index) {}` (called under `m_mutex`, mirroring `applySetting`). Implement them in `GmePlugin`: `selectSubtrack(n)` calls `gme_start_track(emu, n)` and refreshes the cached title/duration/metadata for track `n` (the same `gme_track_info` read `open()` does), clamped to `[0, count)`. Add `PlayerController::selectSubtrack(int)` that locks `m_mutex`, calls the active plugin, and resets `m_trackEnded`. Surface subtrack count + current index through `PlaybackStatus` (`src/player/PlaybackStatus.h`) → `UiState`. Verify: a multi-track NSF reports its true `getSubtrackCount()`; `selectSubtrack(k)` plays subtrack k with the right title/duration; a single-track format (e.g. a MOD via OpenMpt) reports count 1 and `selectSubtrack` is a no-op.
- [ ] **18b — Wire NEXT/PREVIOUS + auto-advance to subtracks**: branch the navigation so NEXT advances to the next subtrack when `current+1 < count`, else advances to the next file; PREVIOUS is symmetric (previous subtrack, else previous file — landing on that file's **last** subtrack is a reasonable choice, decide and document). Auto-advance on `consumeTrackEnded()` steps to the next subtrack when one remains, else the next file. Put the branch in `Application::handleButtonClick` / a small helper alongside `playAdjacentTrack`, keeping the file-scan path unchanged for the fall-through. Optionally show a "Track n/N" indicator in the Gui (from the `UiState` fields added in 18a). Verify on Switch hardware with a multi-track NSF: NEXT cycles through the subtracks in order and only then moves to the next file; PREVIOUS reverses; a finished subtrack auto-advances to the next subtrack, then to the next file when the last subtrack ends.

Each chunk ends with green desktop + Switch builds, docs updated (`docs/audio.md`, `docs/ui.md`, `docs/application.md` as touched), user verification (18b needs a Switch hardware test), then a commit. Run cpp-reviewer on the diff before committing.

## Architecture

```
NEXT/PREVIOUS ─▶ Application::handleButtonClick
                     │  18b: current+dir within [0,count) ?
                     ├─ yes ─▶ PlayerController::selectSubtrack(n) ──(m_mutex)──▶ plugin->selectSubtrack(n)
                     │                                                              └ gme_start_track(n) + refresh caches
                     └─ no  ─▶ playAdjacentTrack(±1)  (existing file scan)
consumeTrackEnded() ─▶ same branch: next subtrack, else next file
PlaybackStatus{ subtrackCount, currentSubtrack } ─▶ UiState ─▶ Gui "Track n/N"
```

## Files to change

1. **`src/player/PlayerPlugin.h`** — three new virtuals with defaults (count / current / select).
2. **`src/player/plugins/GmePlugin.{h,cpp}`** — implement `selectSubtrack` via `gme_start_track(n)` + cache refresh; expose count/current.
3. **`src/player/PlayerController.{h,cpp}`** — `selectSubtrack(int)` under `m_mutex`; expose count/current via `getStatus()`.
4. **`src/player/PlaybackStatus.h`** — `int subtrackCount` / `int currentSubtrack` fields.
5. **`src/gui/UiState.h`** — carry the subtrack count/index for the indicator.
6. **`src/Application.{h,cpp}`** — subtrack-first branching in NEXT/PREVIOUS and auto-advance.
7. **`src/gui/Gui.cpp`** — optional "Track n/N" indicator.

No CMakeLists.txt change (no new file). Other plugins (OpenMpt/Sid/Sc68) rely on the base-class defaults and need no edit.

## Docs

- **`docs/audio.md`** — the subtrack plugin API (defaults + GME override), the locked `selectSubtrack` pass-through, and the cache-refresh requirement.
- **`docs/ui.md`** — NEXT/PREVIOUS subtrack-then-file semantics and the optional track indicator.
- **`docs/application.md`** — the navigation branch (subtrack vs file) shared by buttons and auto-advance.

## Coordination

- **Requires TODO_9** (GmePlugin exists). Independent of the other new items. Touches the transport path, so land it after any transport-adjacent refactor if one is in flight.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- **18a**: a multi-track NSF reports the correct subtrack count; selecting a subtrack updates title/duration/metadata; single-track formats report 1 and ignore selection; no data race (selection under `m_mutex`, no teardown on the audio thread).
- **18b** (Switch hardware, ask before deploying): NEXT walks the subtracks then the next file; PREVIOUS reverses; auto-advance steps subtracks then files; the track indicator (if added) tracks the current subtrack.
