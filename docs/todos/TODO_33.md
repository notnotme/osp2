# TODO_33 — Playlist "now playing" highlight by index (drop lossy basename matching)

> The playlist tab decides which row is "current" by comparing basenames (`entry.name == playingFileName`, `src/gui/Gui.cpp:827`), while the authoritative cursor — `Application::m_playlistIndex` — already exactly identifies the playing playlist entry (or -1). Two real false-match cases exist today: a **browser**-originated track lights any same-named playlist row (wrongly implying transport will follow the playlist — it won't, the cursor is -1), and duplicate basenames from different sources light multiple rows at once. Single chunk. Requires TODO_32 (chunk 32b's helper signatures).

## Context

- Playlist entries are identified by `(sourceIndex, path)` at add-time (`Application.cpp:253-255`), but the highlight matches by basename only — `PlaybackStatus.fileName` is always `m_currentPath.filename()` (`PlayerController.cpp:331, 340`).
- `Application::m_playlistIndex` (`Application.h:61`) is maintained through play (`Application.cpp:281`), advance (`:134-142`), remove-coherence (`:266-272`), and browser-click reset (`:72`); `-1` means "playback originated from the browser". It is the exact semantic the tofu icon and row-selected state want: *advance follows the playlist iff a row is lit*.
- The **browser** highlight (`Gui.cpp:582`) keeps basename matching — within one directory it's the best available key, and it's correct there.
- **Audited and rejected — do not do instead:** moving the cursor into `PlayList`. `PlayList::nextIndex(current, direction)` is deliberately a pure traversal policy; the cursor is playback-session state (`-1` = browser mode; advanced *before* the fetch as a retry cursor, `Application.cpp:123-142`) and moving it would drag the async play-request lifecycle into a clean single-threaded model.
- The comment in `PlaylistEntry.h:34` ("matched against PlaybackStatus.fileName") goes stale with this change — update it.

## Task chunks (implement, verify, and commit one at a time)

- [ ] **33 — Highlight by index**: add `int playingPlaylistIndex;` to `UiState` (`src/gui/UiState.h`, near the playlist slice; -1 = none), filled from `m_playlistIndex` in `Application::makeUiState()`. In `Gui::drawTabPlaylist` (`Gui.cpp:823-846`), compute `is_current = static_cast<int>(index) == state.playingPlaylistIndex` and drop the name comparison; the tofu icon and `Selectable` selected-state both use it. Fold-in: extract `Application::handlePlayFailure(std::string_view reason)` collapsing the 3× duplicated failure branch (`Application.cpp:324-330, 345-358`); optionally group the coordinated playback-request fields (`m_lastRequestedName`, `m_advanceDirection`, `m_pendingPlayName`, `m_advanceLoadInFlight`, `m_consecutivePlaylistSkips`, `Application.h:55-79`) into a small `PendingPlayRequest` value struct if touching them anyway.

## Files to change

1. **`src/gui/UiState.h`** — `playingPlaylistIndex` field.
2. **`src/Application.{h,cpp}`** — fill the field in `makeUiState()`; `handlePlayFailure` fold-in.
3. **`src/gui/Gui.cpp`** — `drawTabPlaylist` index comparison (`:823-846`).
4. **`src/playlist/PlaylistEntry.h`** — stale comment (`:34`).

## Docs

- **`docs/ui.md`** — the playlist-tab notes describing basename matching (~153).
- **`docs/application.md`** — `makeUiState` field list.

## Coordination

- Requires **TODO_32** (32b: `drawTabPlaylist` already receives `state`). Independent of TODO_31/TODO_34.

## Verification

- Desktop + Switch builds green.
- Add a file to the playlist, then play the *same file from the browser* → the playlist row no longer lights (it used to).
- Play from the playlist → exactly one row lights, and NEXT/PREVIOUS follow the playlist.
- Add the same basename from two different sources → only the playing entry lights.
- Remove the playing entry → highlight clears; remove an entry above it → highlight stays on the right row.
