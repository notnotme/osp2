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

#ifndef OSP2_UI_STATE_H
#define OSP2_UI_STATE_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "../filesystem/FileEntry.h"
#include "../filesystem/NavKind.h"
#include "../player/Metadata.h"
#include "../player/PlaybackStatus.h"
#include "../player/PluginSetting.h"
#include "../playlist/PlaylistEntry.h"


/**
 * Per-frame view model: rebuilt every frame by Application::makeUiState(), never stored across frames.
 *
 * The reference members are non-owning views into Application-owned data, valid only for the frame they were
 * built on.
 */
struct UiState {
    PlaybackStatus status;               ///< Playback snapshot driving the player bar and browser highlight.
    std::string path;                    ///< Current browse path shown above the file browser.
    const std::vector<FileEntry> &files; ///< Directory listing; non-owning view, valid for the frame.
    bool isWorking;                      ///< True while a scan/download/decode is in flight; disables the browser.
    std::string workingLabel;            ///< Overlay text while isWorking ("Scanning..." / "Downloading...").
    const TrackMetadata &metadata;       ///< Metadata of the loaded track; non-owning view, valid for the frame.
    /** Per-plugin setting descriptors (plugin name -> descriptors); non-owning view, valid for the frame. */
    const std::vector<std::pair<std::string, std::vector<PluginSetting>>> &pluginSettings;
    NavKind navKind = NavKind::None; ///< One-frame descend/ascend signal driving the browser scroll restore.
    std::string error;               ///< One-frame playback error message; opens the error modal when newly non-empty.
    bool isAtRoot;                   ///< True at the virtual root (sources list): the browser hides its ".." row.
    /** Playlist tab slice, populated by makeUiState(); non-owning view, valid for the frame. */
    const std::vector<PlaylistEntry> &playlist;
    bool playlistShuffle; ///< Playlist Shuffle flag mirrored into the tab's checkbox.
    bool playlistRepeat;  ///< Playlist Repeat flag mirrored into the tab's checkbox.
    /**
     * Index of the currently-playing playlist entry, -1 when none (stopped, or playback originated from the
     * browser). Drives the playlist tab's "now playing" row: exactly the row advance follows.
     */
    int playingPlaylistIndex;
    /**
     * Visualizer picker slice (Settings→Visualizer): the selectable visualizer names; non-owning view of
     * Application's startup-built cache, valid for the frame.
     */
    const std::vector<std::string> &visualizerNames;
    std::size_t activeVisualizer; ///< Currently-selected visualizer index.
};


#endif //OSP2_UI_STATE_H
