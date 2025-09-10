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

#ifndef OSP2_FILEBROWSER_H
#define OSP2_FILEBROWSER_H

#include <filesystem>
#include <vector>

#include "FileEntry.h"


class FileSystem final {
private:
    std::filesystem::path m_path;
    std::vector<FileEntry> m_content;

public:
    FileSystem(const FileSystem &) = delete;
    FileSystem &operator=(const FileSystem &) = delete;
    explicit FileSystem();
    ~FileSystem() = default;

public:
    [[nodiscard]] const std::filesystem::path &getPath() const;
    [[nodiscard]] const std::vector<FileEntry> &getContent() const;
    [[nodiscard]] bool isWorking() const;
};


#endif //OSP2_FILEBROWSER_H