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
#include <string_view>


struct FileEntry {
    std::string name;
    int32_t file_size;
    bool is_directory;

    FileEntry(const std::string_view name, const int32_t fileSize, const bool isDirectory)
        : name(name), file_size(fileSize), is_directory(isDirectory) {}
};


#endif //OSP2_FILE_ENTRY_H