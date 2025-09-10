# TODO_2 — Playback status API (PlaybackStatus + position/duration)

> Split out of the UI redesign (now TODO_3) — this is its player-side half. Pure backend addition; lands and builds independently, so TODO_3 stays purely presentational.

## Context

The Gui currently receives only the current file path string. The redesigned player bar (TODO_3) needs a per-frame snapshot: state, title, filename, playback position and duration. This TODO adds that API to the player domain and swaps it into `UiState`; no visual change beyond the current-file line reading from the new struct.

## Files to change

1. **`src/player/PlaybackStatus.h`** (new, header-only value object):
   ```cpp
   struct PlaybackStatus {
       PlayerState state;
       std::string title;        // from decoder metadata, may be empty
       std::string fileName;     // basename of the open file
       double positionSeconds;   // 0 when stopped
       double durationSeconds;   // 0 when stopped/unknown
   };
   ```

2. **`src/player/PlayerPlugin.h`** — add `[[nodiscard]] virtual double getPosition() const = 0;` and `[[nodiscard]] virtual double getDuration() const = 0;` (seconds). Contract documented on the interface: **called only under `PlayerController::m_mutex`** — the decoder object is shared with the audio thread.

3. **`src/player/plugins/OpenMptPlugin.{h,cpp}`** — implement via `m_module->get_position_seconds()` / `get_duration_seconds()`.

4. **`src/player/PlayerController.{h,cpp}`** — add `[[nodiscard]] PlaybackStatus getStatus() const` — one snapshot under a **single** `m_mutex` lock (state, title, filename, position, duration; zeros/empty when no active plugin). Existing getters stay.

5. **`src/gui/UiState.h`** — replace `currentFile` with `PlaybackStatus status;` (`Application::makeUiState` fills it from `player.getStatus()`).

6. **`src/gui/Gui.cpp`** — minimal touch: the current-file text reads `state.status.fileName` instead of the old string. No layout change (that's TODO_3).

## Docs

- **`docs/audio.md`** — PlaybackStatus in the classDiagram, the new plugin virtuals, and the "position/duration only under m_mutex" rule.
- **`docs/ui.md`** — UiState member change.

No CMakeLists.txt change (new file is a header).

## Coordination

- **Requires TODO_1** (UiState exists).
- **TODO_3** consumes `status` for the player bar (title line, progress bar, m:ss times, play/pause sprite swap).

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- Run from repo root: play the test track — the current-file line still shows the playing file (now sourced from `PlaybackStatus`); stop → line clears. Position/duration correctness is fully exercised by TODO_3's progress bar; here a temporary `SDL_Log` of `getStatus()` during playback is enough (remove before finishing).
