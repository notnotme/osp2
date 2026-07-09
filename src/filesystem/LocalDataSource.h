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

#ifndef OSP2_LOCAL_DATA_SOURCE_H
#define OSP2_LOCAL_DATA_SOURCE_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "DataSource.h"
#include "FileEntry.h"


/**
 * Browses the platform filesystem.
 *
 * A local path is already openable, so fetchFile is the identity. listDirectory never fails hard: it skips
 * dot-prefixed and unreadable entries (skip_permission_denied + the error_code overloads) and returns whatever was
 * readable, never nullopt. The root is "/" on both platforms — on the Switch, sdmc is libnx's default device, so
 * "/" resolves to the SD card and decomposes like any POSIX path.
 */
class LocalDataSource final : public DataSource {
public:
    [[nodiscard]] std::string getDisplayName() const override;
    [[nodiscard]] std::filesystem::path getRootPath() const override;
    [[nodiscard]] std::optional<std::vector<FileEntry>> listDirectory(const std::filesystem::path &path) override;
    [[nodiscard]] std::filesystem::path fetchFile(const std::filesystem::path &path) override {
        return path;
    }
};


#endif //OSP2_LOCAL_DATA_SOURCE_H
