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

#include "FileSystem.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <utility>


namespace {
    // Uppercase extension without the dot ("s3m" -> "S3M"); empty when the name has no extension.
    std::string deriveType(const std::string &name) {
        auto extension = std::filesystem::path(name).extension().string();
        if (extension.empty()) {
            return "";
        }
        extension.erase(0, 1);
        std::ranges::transform(extension, extension.begin(), [](const unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return extension;
    }

    bool caseInsensitiveLess(const std::string &a, const std::string &b) {
        return std::ranges::lexicographical_compare(a, b, [](const unsigned char x, const unsigned char y) {
            return std::tolower(x) < std::tolower(y);
        });
    }

    // Directories first, then files; ties broken case-insensitively by name.
    bool entryLess(const FileEntry &a, const FileEntry &b) {
        if (a.is_directory != b.is_directory) {
            return a.is_directory;
        }
        return caseInsensitiveLess(a.name, b.name);
    }
}


FileSystem::FileSystem()
    : m_activeSource(nullptr),
      m_sourceBeforeScan(nullptr),
      m_working(false),
      m_scanSucceeded(false) {}

void FileSystem::create(std::vector<std::unique_ptr<DataSource>> sources, const std::filesystem::path &startPath,
                        std::function<bool(const std::filesystem::path &)> isPlayable) {
    m_sources = std::move(sources);
    m_isPlayable = std::move(isPlayable);

    if (m_sources.empty()) {
        showVirtualRoot();
        return;
    }

    // sources[0] is the startup source, activated immediately and scanned at startPath.
    m_activeSource = m_sources.front().get();
    m_sourceBeforeScan = m_activeSource;
    startScan(startPath);
}

void FileSystem::destroy() {
    if (m_worker.joinable()) {
        m_worker.join();
    }
    // Release the sources now, with the worker joined, so any source teardown (e.g. FtpDataSource's
    // curl_easy_cleanup) runs before main.cpp's curl_global_cleanup()/socketExit() — not at static
    // destruction, which would tear the handle down after the global curl/socket stack is already gone.
    m_activeSource = nullptr;
    m_sourceBeforeScan = nullptr;
    m_sources.clear();
}

void FileSystem::navigateToEntry(const FileEntry &entry) {
    if (m_working.load()) {
        return;
    }

    if (m_activeSource == nullptr) {
        // At the virtual root: entry.name is a source display name, not a path component.
        for (const auto &source : m_sources) {
            if (source->getDisplayName() == entry.name) {
                m_sourceBeforeScan = m_activeSource;   // nullptr: restore the virtual root if the scan fails
                m_activeSource = source.get();
                startScan(source->getRootPath());
                return;
            }
        }
        return;
    }

    m_sourceBeforeScan = m_activeSource;
    startScan(m_path / entry.name);
}

void FileSystem::navigateToParent() {
    if (m_working.load()) {
        return;
    }

    if (m_activeSource == nullptr) {
        return;   // virtual root: nowhere higher to go
    }

    // parent_path() of a source root (e.g. "sdmc:/") returns itself, so detect the root by value.
    if (m_path == m_activeSource->getRootPath()) {
        m_activeSource = nullptr;
        showVirtualRoot();
        return;
    }

    m_sourceBeforeScan = m_activeSource;
    startScan(m_path.parent_path());
}

void FileSystem::requestFile(const FileEntry &entry) {
    if (m_working.load() || m_activeSource == nullptr) {
        return;
    }

    startFetch(m_path / entry.name);
}

std::optional<FetchResult> FileSystem::consumeFetchResult() {
    std::scoped_lock lock(m_mutex);
    auto result = std::move(m_fetchResult);
    m_fetchResult.reset();
    return result;
}

void FileSystem::update() {
    // A scan is pending exactly when a worker exists and has finished (cleared m_working).
    if (!m_worker.joinable() || m_working.load()) {
        return;
    }
    m_worker.join();

    // A fetch leaves its result in m_fetchResult (consumed via consumeFetchResult()); only a scan
    // swaps the listing here.
    if (m_workKind == WorkKind::Scan) {
        std::scoped_lock lock(m_mutex);
        if (m_scanSucceeded) {
            m_path = std::move(m_pendingPath);
            m_content = std::move(m_pendingContent);
        } else {
            // nullopt from the source: keep the current listing and undo the source activation.
            m_activeSource = m_sourceBeforeScan;
            SDL_Log("FileSystem: scan failed, staying put");
        }
    }
    m_workKind = WorkKind::None;
}

const std::filesystem::path &FileSystem::getPath() const {
    return m_path;
}

const std::vector<FileEntry> &FileSystem::getContent() const {
    return m_content;
}

bool FileSystem::isWorking() const {
    return m_working.load();
}

void FileSystem::startScan(const std::filesystem::path &path) {
    // Called on the main thread with the worker idle; join any finished-but-unswapped worker first.
    if (m_worker.joinable()) {
        m_worker.join();
    }
    auto *source = m_activeSource;
    m_workKind = WorkKind::Scan;
    m_working.store(true);
    m_worker = std::thread(&FileSystem::scan, this, source, path);
}

void FileSystem::startFetch(const std::filesystem::path &path) {
    // Called on the main thread with the worker idle; join any finished-but-unswapped worker first.
    if (m_worker.joinable()) {
        m_worker.join();
    }
    auto *source = m_activeSource;
    m_workKind = WorkKind::Fetch;
    m_working.store(true);
    m_worker = std::thread(&FileSystem::fetch, this, source, path);
}

void FileSystem::showVirtualRoot() {
    // Built synchronously on the main thread while !m_working (nothing to scan).
    m_path.clear();
    m_content.clear();
    for (const auto &source : m_sources) {
        m_content.push_back(FileEntry{source->getDisplayName(), 0, "Source", true});
    }
}

void FileSystem::scan(DataSource *source, std::filesystem::path path) {
    auto raw = source->listDirectory(path);

    std::vector<FileEntry> content;
    const auto succeeded = raw.has_value();
    if (succeeded) {
        content = std::move(*raw);
        std::erase_if(content, [this](const FileEntry &entry) {
            return !entry.is_directory && !m_isPlayable(entry.name);
        });
        for (auto &entry : content) {
            entry.type = entry.is_directory ? "Folder" : deriveType(entry.name);
        }
        std::ranges::sort(content, entryLess);
    }

    {
        std::scoped_lock lock(m_mutex);
        m_scanSucceeded = succeeded;
        if (succeeded) {
            m_pendingPath = std::move(path);
            m_pendingContent = std::move(content);
        }
    }
    m_working.store(false);
}

void FileSystem::fetch(DataSource *source, std::filesystem::path path) {
    const auto localPath = source->fetchFile(path);
    {
        std::scoped_lock lock(m_mutex);
        m_fetchResult = FetchResult{localPath, !localPath.empty()};
    }
    m_working.store(false);   // store last, mirrors scan()
}
