# TODO_28 — Playlist tab (in-memory playlist with add / remove / shuffle / repeat)

> The right-pane "Playlist" tab is an empty stub. Turn it into a working **session-only** playlist: add songs by right-clicking a file in the browser, list them with a per-row "tofu" state icon (empty box normally, filled box for the currently-playing entry), remove items, and Shuffle / Repeat options. Large — implemented in chunks. Requires TODO_9/TODO_17/TODO_18 (playback + async load + subtracks).

## Context

- The Playlist is a **tab in the right pane's TabBar**, not a `ViewMode`. `Gui::drawTabsSection` (`src/gui/Gui.cpp:633`) opens the tab bar and calls `drawFileMetadata` (Metadata tab) + `drawTabPlaylist`. `Gui::drawTabPlaylist` (`:719-724`) is the stub to fill:

  ```cpp
  void Gui::drawTabPlaylist() {
      if (ImGui::BeginTabItem("Playlist")) {
          ImGui::Text("Playlist");
          ImGui::EndTabItem();
      }
  }
  ```

- **No Track/Song struct exists.** A track is identified by its file path / basename. The currently-playing identity used by the UI is `PlaybackStatus.fileName` (basename); the browser's "now playing" highlight compares `file_entry.name == playingFileName` (`Gui.cpp:559-561`), the same basis `playAdjacentTrack` uses (`Application.cpp:105-134`). Basename-only matching collides across directories — a playlist entry must store the **full path + source context** (a browser `FileEntry` is source-relative; `FileSystem::requestFile` operates against the active source), while the filled/empty icon test can mirror the existing filename comparison.

- **No context menus exist yet** (`BeginPopupContextItem` is unused) — the right-click "Add to playlist" is the first. Browser file rows are `Selectable(..., SpanAllColumns)` at `Gui.cpp:562`.

- **Playback path**: browser `onFileClick` → `Application::handleFileClick` (`:64-70`) → `m_fileSystem.requestFile` → `update()` consumes the fetch and calls `m_player.play` (`:205-207`); auto-advance via `consumeTrackEnded` (`:242-244`) → `advance` / `playAdjacentTrack` (`:93-134`).

- **Icons**: existing left-column icons are Material Symbols PUA glyphs drawn as text (folder `` `Gui.cpp:534,551`, file `` `:557`) via `TextColored(...); SameLine(); Selectable(...)`. The task wants an "ASCII tofu": □ (U+25A1) empty, ■ (U+25A0) filled for the playing entry. Caveat — U+25A0/U+25A1 are outside Roboto's loaded default range (`Platform::loadFonts`, `Platform.cpp:257-289`) and are not mapped by the Material Symbols icon font, so drawing them directly risks a real `.notdef`. Resolve in the icon chunk: either extend the loaded glyph range to include U+25A0/U+25A1 (if Roboto has them), or use Material Symbols square glyphs (`check_box_outline_blank` empty / a filled square) — the robust PUA path used everywhere else.

- **Module pattern**: mirror `VisualizerController` (`src/visualizer/VisualizerController.h`) — a small `final` module with `create()/destroy()`, owned **by value** in `Platform` (respect the declaration-order comment `Platform.h:55-56`) and constructor-injected into `Application` (like `PlayerController`/`FileSystem`/`Settings`). New `.cpp` → `add_executable` in CMakeLists.txt + GPL header.

- **Persistence**: out of scope this round — the flat INI `Settings` store (`src/settings/Settings.h`) has no list type; persisting an ordered path list would need numbered keys. Session-only for now; a later chunk can add persistence.

## Architecture

- New **`PlayList`** domain module (`src/player/PlayList.{h,cpp}` or `src/playlist/`): an ordered `std::vector` of entries, each holding the full path + display name + the source context needed to re-fetch, plus `shuffle` / `repeat` flags. Owned by `Platform`, injected into `Application`.
- The tab renders a per-frame slice of the playlist passed through `UiState` (`src/gui/UiState.h`, built in `makeUiState`); actions (add / remove / play / toggle shuffle / toggle repeat) go through new `UiActions` callbacks (`src/gui/UiActions.h`, built in `makeUiActions`, `Application.cpp:288-302`).
- Filled tofu = the entry whose path/name matches `status.fileName` (same comparison as `Gui.cpp:561`).

## Task chunks (implement, verify, and commit one at a time)

- [x] **28a — PlayList module + wiring**: create the `PlayList` module (data model + shuffle/repeat flags), own it by value in `Platform`, inject into `Application`, add its `.cpp` to CMakeLists.txt, and expose a playlist slice on `UiState` + placeholder `UiActions`. The tab is now data-driven but still effectively empty. Build desktop + Switch.
- [x] **28b — Draw the Playlist tab**: fill `drawTabPlaylist` (`Gui.cpp:719`) — iterate the `UiState` playlist slice; per row draw the tofu state icon (empty vs filled by current-track match) + `SameLine` + `Selectable`, copying the browser row idiom. **Resolve the tofu-glyph question here** (extend glyph range vs Material Symbols square).
- [x] **28c — Add to playlist (right-click)**: add `ImGui::BeginPopupContextItem` on the browser file rows (`Gui.cpp:562`) with an "Add to playlist" item → new `onAddToPlaylist(FileEntry)` action → `Application` appends to `PlayList`, capturing the full path/source at add-time (not just the source-relative `FileEntry`).
- [x] **28d — Remove from playlist**: a per-row remove control in the tab → `onRemoveFromPlaylist(index)` → `Application` erases the entry.
- [x] **28e — Play + Shuffle + Repeat**: clicking a playlist entry plays it (reuse the `requestFile` + `play` async path); Shuffle / Repeat checkboxes at the top of the tab influence next-track selection in the auto-advance logic (`advance` / `playAdjacentTrack` `:93-134` and the `consumeTrackEnded` branch `:242-244`) — shuffle randomizes the next entry, repeat wraps at the end.

Each chunk ends with green desktop + Switch builds, docs updated, user verification, then a commit. Run cpp-reviewer on the diff before committing.

## Files to change

1. **`src/player/PlayList.{h,cpp}`** (new) — the module; add the `.cpp` to `add_executable`.
2. **`src/Platform.{h,cpp}`** — own `PlayList` by value; construct/inject/`create`/`destroy` (mirror the visualizer wiring).
3. **`src/Application.{h,cpp}`** — hold the `PlayList&`, populate the `UiState` slice in `makeUiState`, handle the new actions, integrate shuffle/repeat into advance.
4. **`src/gui/UiState.h`**, **`src/gui/UiActions.h`** — playlist slice + add/remove/play/shuffle/repeat callbacks.
5. **`src/gui/Gui.cpp`** — `drawTabPlaylist` (`:719`); the browser context menu (`:562`).
6. **`src/Platform.cpp`** — only if the tofu glyph needs an extended font range (`loadFonts`, `:257-289`).

## Docs

- **`docs/playlist.md`** (new) — the `PlayList` module class diagram + notes (session-only, entry identity by full path, shuffle/repeat semantics).
- **`docs/ui.md`** — the Playlist tab, the tofu state icon, and the browser right-click "Add to playlist".

## Coordination

- Requires **TODO_9** (plugins), **TODO_17** (async load), **TODO_18** (subtracks). Independent of TODO_21–TODO_27; do it last as the large item. Reuses the current-track identity basis from the browser highlight (`Gui.cpp:561`).

## Verification

- Desktop + Switch builds green each chunk (playlist verification is desktop-first; a Switch check for the tab/glyph is worthwhile).
- Right-click "Add to playlist" appends the correct file; the tab lists entries with the tofu icon; the filled tofu tracks the currently-playing entry as it changes.
- Remove deletes the right entry; clicking an entry plays it.
- Shuffle randomizes the next entry; Repeat wraps at the end; nothing regresses the browser or transport.
