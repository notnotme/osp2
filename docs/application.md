# Application domain

Use-case layer in `src/Application.{h,cpp}`. `Application` sits between the domain (`PlayerController`, `FileSystem`) and the presentation layer (`Gui`): it turns UI intent into playback/navigation actions and produces the per-frame view model. It holds references to the player and filesystem (both outlive it) and no state of its own. `main.cpp` shrinks to pure platform lifecycle (SDL/GL/ImGui init, event loop) — it owns the `Application` global, calls `update()` each frame, and forwards `makeUiState()`/`makeUiActions()` to `Gui`. On the Switch it also drives an emulated mouse cursor from the gamepad (see [input.md](input.md)).

```mermaid
classDiagram
    class Application {
        -PlayerController& m_player
        -FileSystem& m_fileSystem
        -Settings& m_settings
        -string m_lastRequestedName
        -int m_advanceDirection
        -TrackMetadata m_trackMetadata
        -path m_metadataPath
        -vector~pair~string, vector~PluginSetting~~~ m_pluginSettings
        +Application(PlayerController&, FileSystem&, Settings&)
        +handleButtonClick(ButtonId)
        +handleFileClick(const FileEntry&)
        +handleDirectoryClick(const FileEntry&)
        +handleThemeChange(Theme)
        +handlePluginSettingChange(pluginName, key, value)
        +handlePluginSettingCommit(pluginName, key, value)
        +refreshPluginSettings()
        +playAdjacentTrack(int direction)
        +update()
        +makeUiState() UiState
        +makeUiActions() UiActions
    }

    class UiState {
        <<value object>>
    }
    class UiActions {
        <<value object>>
    }

    Application o-- PlayerController : drives playback
    Application o-- FileSystem : reads listing
    Application o-- Settings : persists user choices
    Application ..> UiState : builds per frame
    Application ..> UiActions : builds once at init
    Application ..> ButtonId : handleButtonClick
    Gui ..> UiState : consumes
    Gui ..> UiActions : consumes
```

## Notes

- **Callback style, one seam.** UI reports intent, `Application` decides. `makeUiActions()` returns a `UiActions` whose lambdas capture `this`; it is called once at startup because `Application` outlives the actions. `makeUiState()` runs every frame — the domain → view-model translation (edge translation) lives here, never in `Gui`.
- `handleButtonClick` owns the `ButtonId` switch, including the `TODO(temporary)` hardcoded `music/test.s3m` played on PLAY while STOPPED (until `FileSystem` returns real directories). Its `BASE_PATH` (`romfs:/` on Switch, else `romfs/`) mirrors main.cpp.
- **Playback is routed through `FileSystem`** (so TODO_7 can resolve remote files asynchronously without touching callers). A file click / auto-advance no longer calls `player.play()` directly: `handleFileClick` sets `m_advanceDirection = 0` and calls `m_fileSystem.requestFile(entry)`; `playAdjacentTrack(direction)` requests only the *first* playable sibling, records it in `m_lastRequestedName` (the retry cursor) and keeps the direction in `m_advanceDirection`. Because success isn't known at request time, the play loop lives at the consume site in `update()`.
- `playAdjacentTrack(direction)` (`+1` NEXT, `-1` PREVIOUS) resolves the "current" track from `m_lastRequestedName` when set, else `player.getCurrentPath().filename()`; it scans the listing for the next playable sibling. When a direction runs off the end with no candidate it clears `m_lastRequestedName` so a later NEXT/PREVIOUS resolves against the actually-playing track.
- **`update()` responsibilities** (once per frame, before Gui draw): `m_fileSystem.update()` (swap a finished scan in on the main thread); then consume a resolved `FetchResult` — on success `player.play(localPath)` and clear the cursor, on a failed *sibling* fetch retry via `playAdjacentTrack(m_advanceDirection)`; then poll `PlayerController::consumeTrackEnded()` to auto-advance with `playAdjacentTrack(+1)`. The fetch-result consume runs **before** the track-ended poll: a successful `play()` clears the track-ended flag, so an explicit click landing as the current track ends wins over auto-advance instead of being clobbered by it. Track teardown stays off the audio thread (see [audio.md](audio.md)).
- **Cancelling network work.** `handleCancelWork` (wired as `UiActions::onCancelWork`, fired by the browser-overlay Cancel button) calls `m_fileSystem.cancel()` and then zeroes the auto-advance intent (`m_advanceDirection = 0`, `m_lastRequestedName.clear()`). Dropping the intent is what makes a cancelled *download* stop instead of chaining on: the aborted fetch's `FetchResult{empty, false}` is then consumed with direction 0, taking the log-only branch rather than `playAdjacentTrack`.
- **Metadata is fetched on track change, not per frame.** `update()` compares `player.getCurrentPath()` against `m_metadataPath`; on a difference it refetches `m_trackMetadata = player.getMetadata()` and remembers the new path. This covers manual play, auto-advance, and stop (a cleared path resets `m_trackMetadata` to `monostate`, so the Metadata tab returns to its empty state). Fetching only on change keeps `getMetadata()`'s `m_mutex` lock off the per-frame path. `makeUiState()` exposes `m_trackMetadata` as the `UiState::metadata` reference (valid for the frame); the `Gui` dispatches on the variant (see [ui.md](ui.md)).
- `handleDirectoryClick(entry)` routes `entry.name == ".."` to `FileSystem::navigateToParent()` and any other entry to `navigateToEntry(entry)` — no path joining, since at the virtual root `entry.name` is a source display name, not a path component (`FileSystem` resolves it against the active source). Wired as the third `UiActions` lambda (`onDirectoryClick`).
- `makeUiState()` maps `FileSystem`'s empty path (the virtual root) to the label `"Sources"` — a view-model translation that belongs in `Application`, not in `FileSystem` or `Gui`.
- **Theme change flow.** `Application` holds a `Settings &` (persistence domain, see [settings.md](settings.md)). The Gui's Theme menu applies the palette *itself* (`applyTheme` — presentation owns the ImGui style) and also fires `onThemeChange(theme)`; `Application::handleThemeChange` then persists only — `settings.setString("user", "theme", themeToString(theme))` + `settings.save()`. Keeping the visual apply in the Gui avoids dragging ImGui knowledge into the use-case layer, so the persistence handler has a single responsibility. The initial theme is applied at startup by `main.cpp` (the composition root) from `[user] theme`, not by `Application`.
- **Plugin-setting flow is apply-live-on-edit, persist-on-Save.** The Gui popup owns a working copy and applies edits to the decoder live for an audio preview, persisting only when the user clicks **Save** (see [ui.md](ui.md)). The seam has **two** callbacks: `onPluginSettingChange` → `Application::handlePluginSettingChange` applies to the live decoder via `player.applyPluginSetting(pluginName, key, value)` (mutex-guarded, see [audio.md](audio.md)) for immediate audio and **also patches the matching cached descriptor's value in place** so `m_pluginSettings` tracks the live decoder value; it does **not** save. `onPluginSettingCommit` → `Application::handlePluginSettingCommit` only persists (`settings.setInt("plugin." + pluginName, key, value)` + `settings.save()`), fired by the popup's Save button once per descriptor. The decoder already holds the value from the live edits, so the commit handler never touches the player. **The descriptors are cached, not fetched per frame.** `player.getPluginSettings()` locks the audio mutex and allocates, so — like `m_trackMetadata` — `Application` keeps a `m_pluginSettings` snapshot, built by `refreshPluginSettings()` **once at startup** (from `main.cpp` after the persisted-value push); thereafter the in-place patch on each live edit keeps it current, so there is no per-frame or deferred rebuild (the old `m_pluginSettingsDirty` flag is gone). The in-place write mutates only an `int` in an existing element (no reallocation), so it is safe even though `makeUiState()` hands `m_pluginSettings` to the Gui by reference — during the popup the Gui reads its own working copy, touching the cache only to seed on open. No plugin name is hardcoded in `Application` — the pair list drives everything.
- Later TODOs extend the seam by adding members to `UiState`/`UiActions` rather than changing signatures: `PlaybackStatus` (TODO_2), `onDirectoryClick` (TODO_4), `metadata` (TODO_5), `onThemeChange` (TODO_6a), `onPluginSettingChange` (TODO_6c).
