/*
 * Copyright (C) 2026 Romain Graillot
 *
 * This file is part of OSP2.
 *
 * OSP2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSP2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "Application.h"

#include "player/PlayResult.h"
#include "player/PlayerState.h"
#include "settings/SettingsKeys.h"
#include "visualizer/VisualFrame.h"
#include "visualizer/VisualizerController.h"


Application::Application(
    PlayerController &player,
    FileSystem &fileSystem,
    Settings &settings,
    PlayList &playList,
    VisualizerController &visualizer
)
    : m_player(player),
      m_fileSystem(fileSystem),
      m_settings(settings),
      m_playList(playList),
      m_visualizer(visualizer),
      m_advanceDirection(0),
      m_playlistIndex(-1),
      m_consecutivePlaylistSkips(0) {}

void Application::handleButtonClick(const ButtonId buttonId) {
    switch (buttonId) {
    case PLAY_PAUSE:
        switch (m_player.getState()) {
        case PlayerState::PLAYING:
            m_player.pause();
            break;
        case PlayerState::PAUSED:
            m_player.play();
            break;
        case PlayerState::STOPPED:
            // Nothing loaded: PLAY_PAUSE is a no-op (a track is started by selecting a file in the
            // browser, not by the transport). play() with no path only resumes an existing track.
            break;
        }
        break;
    case STOP:
        m_player.stop();
        // stop() cancels any in-flight load, so consumePlayResult() returns nullopt and never clears
        // this — reset it here (mirroring handleCancelWork) so a suppressed advance overlay does not
        // leak into the next non-fetch play (e.g. PLAY from STOPPED).
        m_advanceLoadInFlight = false;
        break;
    case NEXT:
        advance(+1);
        break;
    case PREVIOUS:
        advance(-1);
        break;
    case QUIT:
        // Intercepted by Platform (it owns the run-loop flag); never reaches the app layer.
        break;
    }
}

void Application::handleFileClick(const FileEntry &entry) {
    if (m_player.isSupported(entry.name)) {
        m_playlistIndex = -1; // a browser click leaves playlist mode: later advance uses the browser
        m_advanceDirection = 0;
        m_pendingPlayName = entry.name; // for a possible error message; a direct click is never silent
        m_fileSystem.requestFile(entry);
    }
}

void Application::handleDirectoryClick(const FileEntry &entry) {
    // No path joining here: at the virtual root, entry.name is a source display name, not a
    // path component — FileSystem interprets it against the active source (or lack of one).
    if (entry.name == "..") {
        m_fileSystem.navigateToParent();
    } else {
        m_fileSystem.navigateToEntry(entry);
    }
}

// Subtrack-first navigation shared by the NEXT/PREVIOUS transport buttons and auto-advance.
// direction: +1 for NEXT, -1 for PREVIOUS. Steps within the current file's subtracks while another
// one exists in that direction; at a boundary it dispatches to advanceTrack — the PLAYLIST's
// next/previous entry when a playlist entry is playing, otherwise the browser's next/previous FILE.
// When nothing is loaded (count 1 / current 0) or a single-track file plays, target is out of
// [0,count) in both directions, so it always falls through to file/playlist navigation — preserving
// today's behavior. PREVIOUS from subtrack 0 lands on the PREVIOUS entry at ITS subtrack 0, not that
// entry's last subtrack: the last-subtrack variant would need to defer a "select last" until the async
// load completes; the simple per-file-first choice is intentional. Subtrack selection performs no
// fetch, so it deliberately does not touch m_advanceDirection/m_pendingPlayName/m_lastRequestedName
// (those are always freshly set before any fetch, so a stale value can never reach a fetch-failure site).
void Application::advance(const int direction) {
    const int count = m_player.getSubtrackCount();
    const int target = m_player.getCurrentSubtrack() + direction;
    if (target >= 0 && target < count) {
        m_player.selectSubtrack(target); // stay in this file, step the subtrack (instant, no fetch)
    } else {
        advanceTrack(direction); // at a boundary: fall through to the next/previous playlist entry or file
    }
}

// File-boundary dispatch: while a playlist entry is playing, NEXT/PREVIOUS and auto-advance traverse
// the PLAYLIST; otherwise they walk the browser's adjacent file. Also used by the fetch/decode-failure
// retry sites so a broken playlist entry is skipped within the playlist, just as a broken browser
// sibling is skipped in the browser.
void Application::advanceTrack(const int direction) {
    if (m_playlistIndex >= 0) {
        advancePlaylist(direction);
    } else {
        playAdjacentTrack(direction);
    }
}

// Playlist counterpart of playAdjacentTrack: requests the playlist's next/previous entry (shuffle/
// repeat decided by PlayList::nextIndex). m_playlistIndex is advanced BEFORE the fetch so a failure
// retry (advanceTrack(m_advanceDirection) again) keeps skipping through the playlist; on PlayResult::Ok
// it already points at the now-playing entry.
void Application::advancePlaylist(const int direction) {
    const auto &entries = m_playList.entries();
    if (entries.empty() || m_playlistIndex < 0 || static_cast<std::size_t>(m_playlistIndex) >= entries.size()) {
        // The list emptied or the index fell out of range (e.g. after removals): detach from playlist mode.
        m_playlistIndex = -1;
        return;
    }

    const auto next = m_playList.nextIndex(static_cast<std::size_t>(m_playlistIndex), direction);
    if (!next) {
        // End of the playlist with repeat off: drop the browser retry cursor (mirroring
        // playAdjacentTrack's tail) and let playback simply end.
        m_lastRequestedName.clear();
        return;
    }

    m_playlistIndex = static_cast<int>(*next);

    // Bound the failure-retry chain: with Repeat/Shuffle on, nextIndex never returns nullopt, so an
    // all-unplayable playlist would loop forever re-fetching. Stop once every entry has been tried once
    // since the last successful play (the counter resets to 0 on PlayResult::Ok, so it only accrues
    // across consecutive failures).
    if (m_consecutivePlaylistSkips >= static_cast<int>(m_playList.size())) {
        m_consecutivePlaylistSkips = 0;
        return; // playback ends
    }
    ++m_consecutivePlaylistSkips;

    const auto &entry = entries[*next];
    m_advanceDirection = direction;
    m_pendingPlayName = entry.name; // tracked for a message even though auto-advance stays silent
    m_fileSystem.requestFileFromSource(entry.sourceIndex, entry.path);
}

// direction: +1 for NEXT, -1 for PREVIOUS. Requests the first playable sibling of the current track;
// success is decided later at the consume site, which retries via this cursor if the fetch fails.
void Application::playAdjacentTrack(const int direction) {
    const auto &entries = m_fileSystem.getContent();
    const auto count = static_cast<int>(entries.size());
    const auto current =
        m_lastRequestedName.empty() ? m_player.getCurrentPath().filename().string() : m_lastRequestedName;

    auto index = -1;
    for (int i = 0; i < count; ++i) {
        if (!entries[i].is_directory && entries[i].name == current) {
            index = i;
            break;
        }
    }

    for (int i = index + direction; i >= 0 && i < count; i += direction) {
        const auto &entry = entries[i];
        if (entry.is_directory || !m_player.isSupported(entry.name)) {
            continue;
        }
        m_advanceDirection = direction;
        m_lastRequestedName = entry.name;
        m_pendingPlayName = entry.name; // tracked for a message even though auto-advance stays silent
        m_fileSystem.requestFile(entry);
        return;
    }

    // No candidate in this direction: drop the retry cursor so a later NEXT/PREVIOUS
    // resolves against the actually-playing track, not the last failed name.
    m_lastRequestedName.clear();
}

// The Gui already applied the theme visually (it owns the ImGui style); here we only persist it.
void Application::handleThemeChange(const Theme theme) {
    m_settings.setString(settingskeys::kUserSection, settingskeys::kTheme, themeToString(theme));
    m_settings.save();
}

// VISUALIZATION-mode render hook: the Gui reports the reserved rect, we read the audio tap, build a
// VisualFrame, and render the active visualizer inside the ImGui frame.
void Application::handleRenderVisualization(const float x, const float y, const float w, const float h) {
    // Zero-initialized so the frameCount==0 (idle) and partial-read tails are silence, never
    // indeterminate stack — a plugin that reads samples without gating on frameCount stays safe.
    float samples[PlayerController::BUFFER_FRAMES * PlayerController::CHANNELS] = {};
    // decode() publishes nothing when idle, so an ungated read returns the last stale block:
    // gate on PLAYING and pass frameCount 0 otherwise so the visual decays to rest.
    const bool playing = m_player.getState() == PlayerState::PLAYING;
    const std::size_t frames = playing ? m_player.readLatestAudio(samples, PlayerController::BUFFER_FRAMES) : 0;
    const VisualFrame frame{x, y, w, h, samples, frames, PlayerController::CHANNELS, PlayerController::SAMPLE_RATE};
    m_visualizer.render(frame);
}

// Settings→Visualizer picker: switch the active visualizer at runtime, then persist the choice as
// its stable plugin name (mirrors handleThemeChange). The startup restore of the persisted name
// stays in Platform (the composition root).
void Application::handleSelectVisualizer(const std::size_t index) {
    if (index < m_visualizerNames.size()) {
        m_visualizer.select(index);
        m_settings.setString(settingskeys::kUserSection, settingskeys::kVisualizer, m_visualizerNames[index]);
        m_settings.save();
    }
}

// Live edit (fires every frame while a slider is dragged, or once per combo change): apply to the
// decoder for immediate audio preview, but do NOT persist. Also patch the cached descriptor's value
// in place so m_pluginSettings tracks the live decoder value — this is what a reopened popup seeds
// from. The in-place int write reallocates nothing and is safe even though makeUiState hands
// m_pluginSettings to the Gui: during the popup the Gui reads its own working copy and only reads
// the cache when seeding on open, before any change fires that frame.
void Application::handlePluginSettingChange(const std::string &pluginName, const std::string &key, const int value) {
    m_player.applyPluginSetting(pluginName, key, value);
    for (auto &[name, descriptors] : m_pluginSettings) {
        if (name == pluginName) {
            for (auto &d : descriptors) {
                if (d.key == key) {
                    d.value = value;
                    break;
                }
            }
        }
    }
}

// Commit (fired per setting by the popup's Save button): the decoder already holds the value from
// the live edits, so only persist it to the INI.
void Application::handlePluginSettingCommit(const std::string &pluginName, const std::string &key, const int value) {
    m_settings.setInt(settingskeys::kPluginSectionPrefix + pluginName, key, value);
    m_settings.save();
}

void Application::handleCancelWork() {
    m_fileSystem.cancel();
    // Cancel covers both stages the overlay can be showing: a download (FileSystem) and a decode
    // (PlayerController). The player parse cannot be interrupted, so its result is merely dropped.
    m_player.cancelLoad();
    // A cancelled download must NOT auto-advance to the next sibling: drop the advance intent so the
    // resulting failed FetchResult (direction 0) just logs instead of kicking off another download.
    m_advanceDirection = 0;
    m_advanceLoadInFlight = false;
    m_lastRequestedName.clear();
    // Clearing the pending name suppresses the error a cancelled download/decode would otherwise
    // pop: the empty-name guard in update() then takes the silent branch (see makeUiState).
    m_pendingPlayName.clear();
}

void Application::handleAddToPlaylist(const FileEntry &entry) {
    // Only file rows carry the context menu, but guard defensively so a directory never lands
    // in the playlist. Capture the source-relative path (getPath()/name) and owning source index
    // now: the browser may later navigate elsewhere or switch source, but replay (28e) must still
    // re-fetch from where the file actually lives.
    if (entry.is_directory) {
        return;
    }
    const int sourceIndex = m_fileSystem.getActiveSourceIndex();
    auto path = m_fileSystem.getPath() / entry.name;
    // Reject duplicates: a file's identity is (sourceIndex, source-relative path), so adding the
    // same file again is a no-op rather than a repeated row.
    for (const auto &existing : m_playList.entries()) {
        if (existing.sourceIndex == sourceIndex && existing.path == path) {
            return;
        }
    }
    m_playList.add(PlaylistEntry{entry.name, std::move(path), sourceIndex});
}

void Application::handleRemoveFromPlaylist(const std::size_t index) {
    m_playList.removeAt(index); // bounds-checked no-op if out of range
    // Keep the playing-entry cursor coherent with the shifted vector: entries before it slide down by
    // one; removing the playing entry itself detaches from playlist mode (the track keeps playing, but
    // a later NEXT falls back to browser advance).
    if (m_playlistIndex >= 0) {
        if (static_cast<int>(index) < m_playlistIndex) {
            m_playlistIndex--;
        } else if (static_cast<int>(index) == m_playlistIndex) {
            m_playlistIndex = -1;
        }
    }
}

void Application::handlePlayPlaylistEntry(const std::size_t index) {
    const auto &entries = m_playList.entries();
    if (index >= entries.size()) {
        return;
    }
    const auto &entry = entries[index];
    m_playlistIndex = static_cast<int>(index); // enter playlist mode: later advance traverses the playlist
    m_consecutivePlaylistSkips = 0;            // a user-initiated play starts the skip guard fresh
    m_advanceDirection = 0;                    // a direct click: errors are surfaced, decode overlay kept (not silent)
    m_pendingPlayName = entry.name;            // for a possible error message; a direct click is never silent
    m_fileSystem.requestFileFromSource(entry.sourceIndex, entry.path);
}

// Shared tail of every play failure (fetch or decode). An auto-advance (direction != 0) silently
// skips to the next candidate — playlist- or browser-aware via advanceTrack. A direct click
// (direction 0) surfaces the reason, unless the pending name was cleared by a user cancel
// (handleCancelWork), which stays silent.
void Application::handlePlayFailure(const std::string_view reason) {
    if (m_advanceDirection != 0) {
        advanceTrack(m_advanceDirection);
    } else if (!m_pendingPlayName.empty()) {
        m_playbackError = "Cannot play " + m_pendingPlayName + ": ";
        m_playbackError += reason;
    }
}

void Application::handleToggleShuffle() {
    m_playList.toggleShuffle();
}

void Application::handleToggleRepeat() {
    m_playList.toggleRepeat();
}

// Builds the cached plugin-setting descriptors from the player (locks the audio mutex, so it is
// called only at settled points — once from main.cpp after the startup push — never per frame and
// never during a draw). Live edits keep the cache current in place, so no rebuild is needed after.
void Application::refreshPluginSettings() {
    m_pluginSettings = m_player.getPluginSettings();
}

// Builds the cached visualizer-name list (getNames() allocates). Called once from Platform::create()
// after VisualizerController::create() registers the plugins — the set never changes afterwards.
void Application::refreshVisualizerNames() {
    m_visualizerNames = m_visualizer.getNames();
}

void Application::update() {
    // Refreshed every frame (like m_pendingNav) so an error lives exactly the frame it is produced
    // and is picked up by makeUiState() that same frame; the Gui latches it into its modal.
    m_playbackError.clear();

    m_fileSystem.update();
    m_pendingNav = m_fileSystem.consumeNavigation();
    // Reap a finished async load: on success it swaps the decoded plugin in and publishes a
    // play result (consumed below); the "Loading..." overlay stays up until this swap-in.
    m_player.update();

    // Resolve a fetched file: play() is now asynchronous (it starts the decode on a player worker
    // and returns), so success/failure is decided later at the consumePlayResult() poll below.
    // play() still clears the track-ended flag synchronously, so an explicit click made just as the
    // current track ends wins over auto-advance rather than being clobbered by it in the same frame.
    if (const auto r = m_fileSystem.consumeFetchResult()) {
        if (r->succeeded) {
            m_player.play(r->localPath); // async; the retry cursor is dropped once it succeeds
            // Suppress the decode overlay only for a boundary/auto advance (direction != 0); a direct
            // click (direction 0) keeps it. m_advanceDirection is coherent at this consume point.
            m_advanceLoadInFlight = (m_advanceDirection != 0);
        } else {
            handlePlayFailure("download failed");
        }
    }

    // Poll the decode outcome. Auto-advance failures (direction != 0) are a SILENT skip — a broken
    // sibling is never popped up, only the next candidate is tried. A direct click (direction 0)
    // surfaces the reason. A cancel produces no result at all, so it is silent either way.
    if (const auto result = m_player.consumePlayResult()) {
        // The advance load resolved (any outcome): clear the overlay-suppression flag. A retrying
        // advance (Unsupported/DecodeError -> playAdjacentTrack) re-sets it at the next play() call.
        m_advanceLoadInFlight = false;
        switch (*result) {
        case PlayResult::Ok:
            m_lastRequestedName.clear();
            m_consecutivePlaylistSkips = 0; // a track played: the playlist-skip guard starts fresh
            break;
        case PlayResult::Unsupported:
            handlePlayFailure("unsupported format");
            break;
        case PlayResult::DecodeError:
            handlePlayFailure("failed to decode");
            break;
        }
    }

    if (m_player.consumeTrackEnded()) {
        advance(+1); // step to the next subtrack if one remains, else the next file
    }

    // Refetch metadata only when the playing path OR the current subtrack changes (manual play,
    // auto-advance, subtrack switch, stop), not per frame — getMetadata() locks the audio mutex. A
    // subtrack switch keeps the same path but GME reports different song/comment per subtrack, so the
    // index is part of the key. A cleared path resets to monostate.
    // Skip while reloading: during a boundary advance the outgoing plugin is closed, so
    // getCurrentSubtrack() reads 0 (its real value, which advance() needs) even though the bar still
    // shows the outgoing track via the reload snapshot. Refetching now would blank the Metadata tab
    // (getMetadata() returns empty with no active plugin); hold the outgoing metadata until swap-in.
    const auto path = m_player.getCurrentPath();
    const int subtrack = m_player.getCurrentSubtrack();
    if (!m_player.isReloading() && (path != m_metadataPath || subtrack != m_metadataSubtrack)) {
        m_metadataPath = path;
        m_metadataSubtrack = subtrack;
        m_trackMetadata = m_player.getMetadata();
    }
}

UiState Application::makeUiState() const {
    const auto &path = m_fileSystem.getPath();

    // Read each working flag once so a flag and its label can't disagree if a worker finishes
    // mid-build. The player's decode load reuses the same browser overlay as the filesystem, with a
    // distinct label taking priority (a decode always follows the fetch that fed it).
    // Suppress the decode "Loading..." overlay for a boundary/auto advance (m_advanceLoadInFlight):
    // reload continuity keeps the outgoing track on screen, so the fast local decode needs no overlay.
    // A remote sibling still shows "Downloading..." via fsWorking during the FTP fetch. The parked
    // fetch result bridges the fetch → decode hand-off window so the overlay never blinks (TODO_35).
    const bool loading = m_player.isLoading() && !m_advanceLoadInFlight;
    const bool fsWorking = m_fileSystem.isWorking();
    // A finished fetch parked unconsumed (the one-frame fetch→decode hand-off window) keeps the
    // overlay up: next frame update() consumes it and play() raises isLoading() synchronously.
    // Must be read AFTER fsWorking — the worker parks the result before clearing m_working, so this
    // order can never see both false while a fetch is pending; reversed, the blink comes back.
    const bool fetchParked = m_fileSystem.hasPendingFetchResult();
    const bool working = loading || fsWorking || fetchParked;
    std::string workingLabel;
    if (loading) {
        workingLabel = "Loading...";
    } else if (fsWorking) {
        workingLabel = m_fileSystem.isFetching() ? "Downloading..." : "Scanning...";
    } else if (fetchParked) {
        workingLabel = "Downloading..."; // only fetches park results, never scans
    }

    return {
        .status = m_player.getStatus(),
        .path = path.empty() ? "Sources" : path.string(),
        .files = m_fileSystem.getContent(),
        .isWorking = working,
        .workingLabel = std::move(workingLabel),
        .metadata = m_trackMetadata,
        .pluginSettings = m_pluginSettings,
        .navKind = m_pendingNav,
        .error = m_playbackError,
        .isAtRoot = path.empty(), // the empty FileSystem path is the virtual root (sources list)
        .playlist = m_playList.entries(),
        .playlistShuffle = m_playList.shuffle(),
        .playlistRepeat = m_playList.repeat(),
        // The cursor itself survives STOP (NEXT still resumes from the playlist), but a stopped
        // player lights no row — matching the browser highlight, which goes dark on an empty fileName.
        .playingPlaylistIndex = m_player.getState() == PlayerState::STOPPED ? -1 : m_playlistIndex,
        .visualizerNames = m_visualizerNames,
        .activeVisualizer = m_visualizer.getActiveIndex()
    };
}

UiActions Application::makeUiActions() {
    return {
        .onButtonClick = [this](const ButtonId buttonId) { handleButtonClick(buttonId); },
        .onFileClick = [this](const FileEntry &entry) { handleFileClick(entry); },
        .onDirectoryClick = [this](const FileEntry &entry) { handleDirectoryClick(entry); },
        .onThemeChange = [this](const Theme theme) { handleThemeChange(theme); },
        .onPluginSettingChange = [this](
                                     const std::string &pluginName, const std::string &key, const int value
                                 ) { handlePluginSettingChange(pluginName, key, value); },
        .onPluginSettingCommit = [this](
                                     const std::string &pluginName, const std::string &key, const int value
                                 ) { handlePluginSettingCommit(pluginName, key, value); },
        .onCancelWork = [this]() { handleCancelWork(); },
        .onAddToPlaylist = [this](const FileEntry &entry) { handleAddToPlaylist(entry); },
        .onRemoveFromPlaylist = [this](const std::size_t index) { handleRemoveFromPlaylist(index); },
        .onPlayPlaylistEntry = [this](const std::size_t index) { handlePlayPlaylistEntry(index); },
        .onToggleShuffle = [this]() { handleToggleShuffle(); },
        .onToggleRepeat = [this]() { handleToggleRepeat(); },
        .onRenderVisualization = [this](
                                     const float x, const float y, const float w, const float h
                                 ) { handleRenderVisualization(x, y, w, h); },
        .onSelectVisualizer = [this](const std::size_t index) { handleSelectVisualizer(index); }
    };
}
