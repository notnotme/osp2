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

#ifndef OSP2_DATA_SOURCE_H
#define OSP2_DATA_SOURCE_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "FileEntry.h"


/**
 * A browsable origin of directories and files (local disk, remote server, ...).
 *
 * FileSystem serializes calls: at most one listDirectory/fetchFile is in flight at any time (worker thread, or the
 * main thread while no worker runs), so sources need no locking.
 */
class DataSource {
public:
    DataSource() = default;
    DataSource(const DataSource &) = delete;
    DataSource &operator=(const DataSource &) = delete;
    virtual ~DataSource() = default;

    /** Source name shown to the user ("Local files", "Modland (FTP)"). */
    [[nodiscard]] virtual std::string getDisplayName() const = 0;
    /** Top of the source; ".." here exits to the virtual root. */
    [[nodiscard]] virtual std::filesystem::path getRootPath() const = 0;

    /**
     * Identifier of this source's on-disk cache subdirectory (the component under the cache root).
     *
     * Used to keep per-source caches from colliding. Empty for sources with no cache (local disk).
     */
    [[nodiscard]] virtual std::string getCacheId() const {
        return {};
    }

    /**
     * Lists the raw entries (name, byte size, is_directory) of one directory.
     *
     * Blocking. Hidden entries are already skipped; no filtering/sorting (FileSystem applies isPlayable, type
     * derivation, and sort).
     * @return nullopt on hard failure (already SDL_Logged) — FileSystem keeps the current listing
     */
    [[nodiscard]] virtual std::optional<std::vector<FileEntry>> listDirectory(const std::filesystem::path &path) = 0;

    /**
     * Resolves a source path to a locally-openable file for PlayerController::play().
     *
     * Blocking. Local source: returns the path itself. Remote sources: download to cache, return the cache path.
     * @return an empty path on failure (already SDL_Logged)
     */
    [[nodiscard]] virtual std::filesystem::path fetchFile(const std::filesystem::path &path) = 0;

    /**
     * Requests that an in-flight listDirectory/fetchFile abort ASAP.
     *
     * Called from the MAIN thread and may run concurrently with a blocking call on the worker, so implementations
     * must be thread-safe (an atomic flag). Default: no-op (local/instant sources have nothing to cancel).
     */
    virtual void cancel() {}
};


#endif //OSP2_DATA_SOURCE_H
