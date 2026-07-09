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

#ifndef OSP2_APPLICATION_H
#define OSP2_APPLICATION_H

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "filesystem/FileEntry.h"
#include "filesystem/FileSystem.h"
#include "filesystem/NavKind.h"
#include "gui/ButtonId.h"
#include "gui/Theme.h"
#include "gui/UiActions.h"
#include "gui/UiState.h"
#include "player/Metadata.h"
#include "player/PlayerController.h"
#include "player/PluginSetting.h"
#include "playlist/PlayList.h"
#include "settings/Settings.h"

class VisualizerController;


/**
 * Use-case layer: turns UI intent into playback/navigation actions and produces the per-frame view model.
 *
 * Depends on the domain (PlayerController, FileSystem), never on the presentation framework. See
 * docs/application.md.
 */
class Application final {
private:
    PlayerController &m_player;
    FileSystem &m_fileSystem;
    Settings &m_settings;
    PlayList &m_playList;
    VisualizerController &m_visualizer;

    /** Playback-request retry cursor: the name playAdjacentTrack advances from when a fetched sibling fails. */
    std::string m_lastRequestedName;
    int m_advanceDirection; ///< Direction of the pending request: +1/-1 while auto-advancing, 0 for a direct click.

    /**
     * Index of the currently-playing playlist entry, or -1 when playback originated from the browser (not the
     * playlist). While >= 0, NEXT/PREVIOUS and auto-advance traverse the PLAYLIST instead of the browser's
     * adjacent file; a browser file click resets it to -1 (leaving playlist mode).
     */
    int m_playlistIndex;

    /**
     * Count of consecutive playlist entries fetched without one successfully playing. Bounds the failure-retry
     * chain: Repeat/Shuffle make PlayList::nextIndex never return nullopt, so an all-unplayable playlist would
     * otherwise loop forever. advancePlaylist stops once this reaches the playlist size (every entry tried once
     * since the last success); reset to 0 on a successful decode and at the start of a user-initiated playlist
     * play.
     */
    int m_consecutivePlaylistSkips;

    /**
     * Main-thread only: true while a boundary/auto advance load is in flight, so makeUiState() suppresses the
     * decode "Loading..." overlay for the seamless fast local case (a direct click keeps it; a remote sibling
     * still shows the "Downloading..." fetch overlay). Set at the play() call site when m_advanceDirection != 0,
     * cleared when the play result is consumed/cancelled.
     */
    bool m_advanceLoadInFlight = false;

    /**
     * Name of the file whose playback was last requested (set in handleFileClick / playAdjacentTrack), used
     * only to compose a user-facing error message. Distinct from m_lastRequestedName (the auto-advance retry
     * cursor); cleared by handleCancelWork so a user cancel never pops an error.
     */
    std::string m_pendingPlayName;
    /**
     * Pending user-facing playback error for this frame (empty = none). Refreshed at the top of update() every
     * frame so it lives exactly the frame it is produced, then handed to makeUiState().
     */
    std::string m_playbackError;

    /**
     * Track metadata cached across frames; refetched in update() only when the playing path changes (manual
     * play, auto-advance, stop).
     */
    TrackMetadata m_trackMetadata;
    std::filesystem::path m_metadataPath; ///< Path m_trackMetadata was fetched for.
    /**
     * Subtrack index m_trackMetadata was fetched for. A subtrack switch keeps the same path, so the metadata
     * refetch is keyed on path AND this index (GME's per-subtrack song/comment differ). -1 = no metadata
     * fetched yet, so the first update() always refetches.
     */
    int m_metadataSubtrack = -1;

    /**
     * Plugin setting descriptors cached across frames (getPluginSettings() locks the audio mutex and allocates,
     * so it must stay off the per-frame path). Built once at startup via refreshPluginSettings(); thereafter
     * each live edit patches the matching descriptor's value in place (handlePluginSettingChange), so the cache
     * tracks the decoder and a reopened popup seeds from current values. No full rebuild after edits — the Gui
     * iterates this vector by reference while drawing, so reassigning it mid-draw would dangle.
     */
    std::vector<std::pair<std::string, std::vector<PluginSetting>>> m_pluginSettings;

    /**
     * Visualizer plugin names cached across frames (getNames() allocates a vector<string>, so it must stay off
     * the per-frame path). The set is fixed after VisualizerController::create(), so the cache is built once
     * via refreshVisualizerNames() and never rebuilt; makeUiState() hands it to the Gui as a non-owning view.
     */
    std::vector<std::string> m_visualizerNames;

    /** Per-frame browser nav signal relayed from FileSystem to the Gui via UiState (scroll restore). */
    NavKind m_pendingNav = NavKind::None;

public:
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    /** Binds the use-case layer to its subsystems; every referenced subsystem must outlive the Application. */
    explicit Application(
        PlayerController &player,
        FileSystem &fileSystem,
        Settings &settings,
        PlayList &playList,
        VisualizerController &visualizer
    );
    ~Application() = default;

public:
    /**
     * Per-frame orchestration step. Main thread, once per frame, before makeUiState().
     *
     * Clears the per-frame error, pumps FileSystem and PlayerController, consumes a resolved fetch (starting
     * the async decode) and the decode's PlayResult (driving failure retries), polls track-ended for
     * auto-advance, and refetches track metadata when the playing track or subtrack changes.
     */
    void update();
    /**
     * Rebuilds the cached plugin-setting descriptors from the player.
     *
     * Called once at startup (after Platform pushes the persisted values); afterwards live edits patch the
     * cache in place, so no further rebuild happens. getPluginSettings() locks the audio mutex and allocates,
     * which is why the descriptors are cached at all.
     */
    void refreshPluginSettings();
    /**
     * Rebuilds the cached visualizer name list.
     *
     * Called once, right after VisualizerController::create() — the plugin set never changes afterwards.
     */
    void refreshVisualizerNames();
    /**
     * Builds the immutable per-frame view snapshot the Gui draws from. Called once per frame, after update().
     *
     * The references it carries (files, metadata, plugin settings, visualizer names) point into Application
     * and FileSystem caches and stay valid for the current frame only.
     */
    [[nodiscard]] UiState makeUiState() const;
    /**
     * Builds the bundle of UI-intent callbacks the Gui fires.
     *
     * The lambdas capture `this`, so the bundle must not outlive the Application; Platform::run() builds it
     * once at startup.
     */
    [[nodiscard]] UiActions makeUiActions();

private:
    void handleButtonClick(ButtonId buttonId);
    void handleFileClick(const FileEntry &entry);
    void handleDirectoryClick(const FileEntry &entry);
    void handleThemeChange(Theme theme);
    void handleRenderVisualization(float x, float y, float w, float h);
    void handleSelectVisualizer(std::size_t index);
    void handlePluginSettingChange(const std::string &pluginName, const std::string &key, int value);
    void handlePluginSettingCommit(const std::string &pluginName, const std::string &key, int value);
    void handleCancelWork();
    /**
     * Shared tail of every play failure (fetch or decode): silent skip on auto-advance, a
     * "Cannot play <name>: <reason>" error on a direct click (silent after a user cancel).
     */
    void handlePlayFailure(std::string_view reason);
    void handleAddToPlaylist(const FileEntry &entry);
    void handleRemoveFromPlaylist(std::size_t index);
    void handlePlayPlaylistEntry(std::size_t index);
    void handleToggleShuffle();
    void handleToggleRepeat();
    /**
     * Subtrack-first advance shared by the NEXT/PREVIOUS transport buttons and auto-advance: steps within the
     * file when the target subtrack exists, otherwise falls through to advanceTrack(direction).
     */
    void advance(int direction);
    /**
     * File-boundary advance dispatch: traverses the playlist when a playlist entry is playing
     * (m_playlistIndex >= 0), otherwise the browser's adjacent file.
     */
    void advanceTrack(int direction);
    void advancePlaylist(int direction);
    void playAdjacentTrack(int direction);
};


#endif //OSP2_APPLICATION_H
