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


// Per-frame view model: rebuilt every frame, never stored across frames.
struct UiState {
    PlaybackStatus status;
    std::string path;
    const std::vector<FileEntry> &files; // non-owning view, valid for the frame
    bool isWorking;
    std::string workingLabel;      // overlay text while isWorking ("Scanning..." / "Downloading...")
    const TrackMetadata &metadata; // non-owning view, valid for the frame
    const std::vector<std::pair<std::string, std::vector<PluginSetting>>>
        &pluginSettings;             // non-owning view, valid for the frame
    NavKind navKind = NavKind::None; // one-frame descend/ascend signal driving the browser scroll restore
    std::string error;               // one-frame playback error message; opens the error modal when newly non-empty
    // Default-initialized so Application's aggregate `return {…}` in makeUiState() stays valid untouched;
    // main.cpp (the visualizer bridge) fills these before drawUserInterface (see visualization.md).
    std::vector<std::string> visualizerNames{}; // for the Settings→Visualizer picker
    std::size_t activeVisualizer = 0;           // currently-selected visualizer index
};


#endif //OSP2_UI_STATE_H
