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

#include "LocalDataSource.h"

#include <SDL.h>

#include <system_error>


std::string LocalDataSource::getDisplayName() const {
    return "Local files";
}

std::filesystem::path LocalDataSource::getRootPath() const {
    // On the Switch, sdmc is libnx's default device, so plain "/" paths resolve to the SD card and
    // std::filesystem decomposes them like any POSIX path (the "sdmc:" device prefix would confuse
    // parent_path(), breaking upward navigation). Only romfs needs its explicit "romfs:/" prefix.
    return "/";
}

std::optional<std::vector<FileEntry>> LocalDataSource::listDirectory(const std::filesystem::path &path) {
    std::vector<FileEntry> entries;

    std::error_code ec;
    auto iterator =
        std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec) {
        SDL_Log("LocalDataSource: cannot open '%s': %s", path.string().c_str(), ec.message().c_str());
        return entries; // partial (here empty) is fine — never nullopt for the local source
    }

    const std::filesystem::directory_iterator end;
    for (; iterator != end; iterator.increment(ec)) {
        if (ec) {
            SDL_Log("LocalDataSource: error listing '%s': %s", path.string().c_str(), ec.message().c_str());
            break;
        }

        const auto name = iterator->path().filename().string();
        if (!name.empty() && name.front() == '.') {
            continue; // skip hidden (dot-prefixed) entries
        }

        std::error_code directory_ec;
        const auto is_directory = iterator->is_directory(directory_ec);

        std::int64_t file_size = 0;
        if (!is_directory) {
            std::error_code size_ec;
            const auto size = iterator->file_size(size_ec);
            if (!size_ec) {
                file_size = static_cast<std::int64_t>(size);
            }
        }

        entries.push_back(FileEntry{name, file_size, "", is_directory});
    }

    return entries;
}
