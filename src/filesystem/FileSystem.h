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

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "DataSource.h"
#include "FileEntry.h"
#include "NavKind.h"


// Outcome of resolving an entry to a locally-openable file; consumed once by Application.
struct FetchResult {
    std::filesystem::path localPath;
    bool succeeded;
};


// Threaded directory browser over a set of DataSources. Directory scans run on a worker
// thread; the main thread reads m_path/m_content (handed to the Gui) and swaps in finished
// scans via update(). See docs/filesystem.md for the full threading contract.
class FileSystem final {
private:
    // Immutable after create().
    std::vector<std::unique_ptr<DataSource>> m_sources;
    std::function<bool(const std::filesystem::path &)> m_isPlayable;

    // Main thread only (update(), or the synchronous virtual-root swap while !m_working).
    std::filesystem::path m_path;
    std::vector<FileEntry> m_content;

    // Main thread only, mutated only while the worker is idle; the worker uses the source captured at launch.
    DataSource *m_activeSource;
    DataSource *m_sourceBeforeScan; // restored by update() when a scan fails (nullopt)

    enum class WorkKind { None, Scan, Fetch };
    WorkKind m_workKind = WorkKind::None; // main thread only: what the current/last worker is doing

    // Set by navigateToParent() when it reaches the virtual root, applied by update() next frame.
    // Deferred (not swapped in place) because navigation callbacks fire mid-draw while the Gui is
    // iterating m_content: clearing it synchronously would shrink the vector under the list clipper.
    bool m_pendingVirtualRoot = false; // main thread only

    // Browser scroll-restore signalling (main thread only). The scan that carries a navigation is
    // asynchronous: the direction is known at the navigate call but the listing only swaps in later,
    // in update(). m_pendingNavDirection records the in-flight scan's direction; update() promotes it
    // to m_navSignal exactly when the listing swaps (a failed scan discards it, emitting nothing).
    NavKind m_pendingNavDirection = NavKind::None; // direction of the in-flight scan, consumed by update()
    NavKind m_navSignal = NavKind::None;           // emitted to the app layer; read+cleared by consumeNavigation()

    // Worker synchronization.
    std::atomic_bool m_working;
    std::thread m_worker;
    mutable std::mutex m_mutex;
    std::filesystem::path m_pendingPath;      // m_mutex
    std::vector<FileEntry> m_pendingContent;  // m_mutex
    bool m_scanSucceeded;                     // m_mutex
    std::optional<FetchResult> m_fetchResult; // m_mutex

public:
    FileSystem(const FileSystem &) = delete;
    FileSystem &operator=(const FileSystem &) = delete;
    explicit FileSystem();
    ~FileSystem() = default;

public:
    void create(
        std::vector<std::unique_ptr<DataSource>> sources,
        const std::filesystem::path &startPath,
        std::function<bool(const std::filesystem::path &)> isPlayable
    );
    void destroy();

    void navigateToEntry(const FileEntry &entry);
    void navigateToParent();
    void requestFile(const FileEntry &entry);
    void cancel();
    [[nodiscard]] std::optional<FetchResult> consumeFetchResult();
    [[nodiscard]] NavKind consumeNavigation();
    void update();

public:
    [[nodiscard]] const std::filesystem::path &getPath() const; // empty at the virtual root
    // Index of the source that produced the current listing, for capturing playlist entries; -1 at the
    // virtual root. Main-thread only (like getPath()).
    [[nodiscard]] int getActiveSourceIndex() const;
    [[nodiscard]] const std::vector<FileEntry> &getContent() const;
    [[nodiscard]] bool isWorking() const;
    [[nodiscard]] bool isFetching() const; // true while a file download (not a scan) is in flight

private:
    void startScan(const std::filesystem::path &path);
    void startFetch(const std::filesystem::path &path);
    void showVirtualRoot();
    void scan(DataSource *source, std::filesystem::path path);
    void fetch(DataSource *source, const std::filesystem::path &path);
};


#endif //OSP2_FILEBROWSER_H
