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

#include <filesystem>
#include <string>

#include "filesystem/FileEntry.h"
#include "filesystem/FileSystem.h"
#include "gui/ButtonId.h"
#include "gui/UiActions.h"
#include "gui/UiState.h"
#include "player/Metadata.h"
#include "player/PlayerController.h"


// Use-case layer: turns UI intent into playback/navigation actions and produces
// the per-frame view model. Depends on the domain (PlayerController, FileSystem),
// never on the presentation framework.
class Application final {
private:
    PlayerController &m_player;
    FileSystem &m_fileSystem;

    // Playback-request retry state: m_lastRequestedName is the cursor playAdjacentTrack advances
    // from when a fetched sibling fails; m_advanceDirection is the direction (+1/-1, 0 for a direct click).
    std::string m_lastRequestedName;
    int m_advanceDirection;

    // Track metadata cached across frames; refetched in update() only when the playing path changes
    // (manual play, auto-advance, stop). m_metadataPath is the path m_trackMetadata was fetched for.
    TrackMetadata m_trackMetadata;
    std::filesystem::path m_metadataPath;

public:
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    explicit Application(PlayerController &player, FileSystem &fileSystem);
    ~Application() = default;

public:
    void update();
    [[nodiscard]] UiState makeUiState() const;
    [[nodiscard]] UiActions makeUiActions();

private:
    void handleButtonClick(ButtonId buttonId);
    void handleFileClick(const FileEntry &entry);
    void handleDirectoryClick(const FileEntry &entry);
    void playAdjacentTrack(int direction);
};


#endif //OSP2_APPLICATION_H
