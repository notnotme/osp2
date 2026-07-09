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


/**
 * A single playlist row.
 *
 * Kept in its own light header so UiState can depend on the entry shape without pulling in the whole PlayList
 * module. A file's identity in this app is (owning DataSource, source-relative path); both are captured at
 * add-time because the browser may later have navigated elsewhere or switched source — replaying an entry
 * re-fetches it from path + sourceIndex (the same source-relative path FileSystem::requestFile /
 * DataSource::fetchFile expect).
 */
struct PlaylistEntry {
    std::string name;           ///< Basename: display and error text only (the playing row is tracked by index).
    std::filesystem::path path; ///< Source-relative path (getPath()/name) fetchFile expects; used for replay.
    int sourceIndex = -1;       ///< Index of the owning DataSource in FileSystem's source list; used for replay.
};


#endif //OSP2_PLAYLIST_ENTRY_H
