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

#ifndef OSP2_FILE_ENTRY_H
#define OSP2_FILE_ENTRY_H

#include <cstdint>
#include <string>


/**
 * One row of a directory listing: a file, a folder, or a source at the virtual root.
 *
 * A bare aggregate; ".." is never an entry — the Gui pins it on top of the listing itself.
 */
struct FileEntry {
    std::string name;
    std::int64_t file_size; ///< bytes
    /**
     * Uppercase extension without dot ("S3M"), "Folder", or "Source"; empty when a source hands it over
     * pre-derivation.
     */
    std::string type;
    bool is_directory;
};


#endif //OSP2_FILE_ENTRY_H
