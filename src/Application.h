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


// Use-case layer: turns UI intent into playback/navigation actions and produces
// the per-frame view model. Depends on the domain (PlayerController, FileSystem),
// never on the presentation framework.
class Application final {
private:
    PlayerController &m_player;
    FileSystem &m_fileSystem;
    Settings &m_settings;
    PlayList &m_playList;

    // Playback-request retry state: m_lastRequestedName is the cursor playAdjacentTrack advances
    // from when a fetched sibling fails; m_advanceDirection is the direction (+1/-1, 0 for a direct click).
    std::string m_lastRequestedName;
    int m_advanceDirection;

    // Main-thread only: true while a boundary/auto advance load is in flight, so makeUiState()
    // suppresses the decode "Loading..." overlay for the seamless fast local case (a direct click
    // keeps it; a remote sibling still shows the "Downloading..." fetch overlay). Set at the play()
    // call site when m_advanceDirection != 0, cleared when the play result is consumed/cancelled.
    bool m_advanceLoadInFlight = false;

    // Name of the file whose playback was last requested (set in handleFileClick / playAdjacentTrack),
    // used only to compose a user-facing error message. Distinct from m_lastRequestedName (the
    // auto-advance retry cursor); cleared by handleCancelWork so a user cancel never pops an error.
    std::string m_pendingPlayName;
    // Pending user-facing playback error for this frame (empty = none). Refreshed at the top of
    // update() every frame so it lives exactly the frame it is produced, then handed to makeUiState().
    std::string m_playbackError;

    // Track metadata cached across frames; refetched in update() only when the playing path changes
    // (manual play, auto-advance, stop). m_metadataPath is the path m_trackMetadata was fetched for.
    TrackMetadata m_trackMetadata;
    std::filesystem::path m_metadataPath;
    // Subtrack index m_trackMetadata was fetched for. A subtrack switch keeps the same path, so the
    // metadata refetch is keyed on path AND this index (GME's per-subtrack song/comment differ). -1 =
    // no metadata fetched yet, so the first update() always refetches.
    int m_metadataSubtrack = -1;

    // Plugin setting descriptors cached across frames (getPluginSettings() locks the audio mutex and
    // allocates, so it must stay off the per-frame path). Built once at startup via
    // refreshPluginSettings(); thereafter each live edit patches the matching descriptor's value in
    // place (handlePluginSettingChange), so the cache tracks the decoder and a reopened popup seeds
    // from current values. No full rebuild after edits — the Gui iterates this vector by reference
    // while drawing, so reassigning it mid-draw would dangle.
    std::vector<std::pair<std::string, std::vector<PluginSetting>>> m_pluginSettings;

    // Per-frame browser nav signal relayed from FileSystem to the Gui via UiState (scroll restore).
    NavKind m_pendingNav = NavKind::None;

public:
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    explicit Application(PlayerController &player, FileSystem &fileSystem, Settings &settings, PlayList &playList);
    ~Application() = default;

public:
    void update();
    void refreshPluginSettings();
    [[nodiscard]] UiState makeUiState() const;
    [[nodiscard]] UiActions makeUiActions();

private:
    void handleButtonClick(ButtonId buttonId);
    void handleFileClick(const FileEntry &entry);
    void handleDirectoryClick(const FileEntry &entry);
    void handleThemeChange(Theme theme);
    void handlePluginSettingChange(const std::string &pluginName, const std::string &key, int value);
    void handlePluginSettingCommit(const std::string &pluginName, const std::string &key, int value);
    void handleCancelWork();
    // Playlist action handlers. 28a wires them through UiActions but leaves the bodies as stubs;
    // the real behavior lands in the noted later chunks.
    void handleAddToPlaylist(const FileEntry &entry);
    void handleRemoveFromPlaylist(std::size_t index);
    void handlePlayPlaylistEntry(std::size_t index);
    void handleToggleShuffle();
    void handleToggleRepeat();
    void advance(int direction);
    void playAdjacentTrack(int direction);
};


#endif //OSP2_APPLICATION_H
