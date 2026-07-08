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

#ifndef OSP2_PLAYLIST_ENTRY_H
#define OSP2_PLAYLIST_ENTRY_H

#include <filesystem>
#include <string>


// A single playlist row. Kept in its own light header so UiState can depend on the entry shape
// without pulling in the whole PlayList module. Later chunks (28c/28e) may extend this with the
// source context needed to re-fetch remote entries; 28a keeps it to path + basename.
struct PlaylistEntry {
    std::filesystem::path path; // full path identifying the file (playback + identity)
    std::string name;           // basename shown in the tab and matched against PlaybackStatus.fileName
};


#endif //OSP2_PLAYLIST_ENTRY_H
