# TODO_22 — No UI blink when next/prev crosses a file boundary

> Stepping between subsongs of one file (next/prev) is seamless, but when next/prev rolls past the first/last subsong into the **adjacent file**, the player bar flashes to "No track" and a "Loading…" overlay appears — a visible blink. Keep the outgoing track's info on screen during the brief async load. Single chunk. Requires TODO_17 (async decode + loading overlay) + TODO_18 (subtrack navigation).

## Context

next/prev route through `Application::advance()` (`src/Application.cpp:93-101`):

```cpp
const int count  = m_player.getSubtrackCount();
const int target = m_player.getCurrentSubtrack() + direction;
if (target >= 0 && target < count) m_player.selectSubtrack(target); // in-place, seamless
else                               playAdjacentTrack(direction);     // loads a new file
```

- **Within a file** (`selectSubtrack`, `PlayerController.cpp:322-333` → `GmePlugin::selectSubtrack`, `GmePlugin.cpp:227-240`): `gme_start_track` on the already-open emulator — no teardown, no reload, no overlay. Seamless.
- **Across the file boundary** (`playAdjacentTrack`, `Application.cpp:105-134` → `PlayerController::play(path)`, `PlayerController.cpp:102-145`): `play()` **synchronously** closes the active plugin and sets `STOPPED` (`:113-118`) — the bar momentarily shows "No track" and drops the highlight — then spawns the async `loadTrack` worker and sets `m_loading`, so `Application::makeUiState()` (`:265-273`) raises the browser "Loading…" overlay. That STOPPED flash + overlay **is** the blink.

So the blink is specific to advancing into a neighbouring file; the seamless case is already correct.

## The fix (single chunk)

Preserve presentation continuity across the boundary load — keep the outgoing track's `PlaybackStatus` (title / filename / highlight) visible until the replacement is ready, instead of flashing to STOPPED + "Loading…". Options (pick during implementation):

- **Staged swap**: load the new track into a staging slot on the worker and swap it in atomically on success, so the active track/status never transiently becomes STOPPED. Cleanest; must respect the audio-thread mutex contract (`m_mutex` guards `m_state`/`m_activePlugin`) and never tear down on the audio thread.
- **Suppress the transient empty state**: while a load initiated by next/prev is in flight, keep showing the previous track and skip the "Loading…" overlay for quick local loads, retaining it only when the load is genuinely slow/remote (FTP).

Reuse the existing async machinery (`loadTrack`, `consumePlayResult`, TODO_17's overlay) — this is continuity, not a new load path. The error path (`consumePlayResult` failure → error popup, TODO_17b) must still fire.

## Files to change

1. **`src/player/PlayerController.cpp`** — avoid the synchronous STOPPED teardown on reload; stage/swap the new track (`play`, `:102-145`).
2. **`src/Application.cpp`** — overlay/status decision in `makeUiState` (`:265-273`); possibly flag boundary-advance loads.

## Docs

- **`docs/audio.md`** — the reload-continuity contract: a boundary advance keeps the previous track's status until the new track is ready; the "Loading…" overlay is reserved for slow/remote loads.

## Coordination

- Requires **TODO_17** (async decode/overlay) and **TODO_18** (subtrack nav). Touches `PlayerController` threading — mind the audio-thread mutex contract in CLAUDE.md.

## Verification

- Desktop + Switch builds green; a Switch hardware check is worthwhile (the blink is most visible there).
- Rolling next past the last subsong (and prev past the first) advances to the neighbouring file **without** the "No track" flash.
- A failed load still surfaces the playback-error popup.
- Genuinely slow / remote (FTP) loads still show a loading indication (no silent hang).
