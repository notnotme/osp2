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


/** Outcome of resolving an entry to a locally-openable file; consumed once by Application. */
struct FetchResult {
    std::filesystem::path localPath; ///< the locally-openable file; empty when the fetch failed
    bool succeeded;                  ///< false when the fetch failed or was cancelled
};


/**
 * Threaded directory browser over a set of DataSources.
 *
 * Directory scans and file downloads run on a worker thread; the main thread reads m_path/m_content (handed to the
 * Gui) and swaps in finished scans via update(). See docs/filesystem.md for the full threading contract.
 */
class FileSystem final {
private:
    /** Immutable after create(); m_isPlayable is called on the worker. */
    std::vector<std::unique_ptr<DataSource>> m_sources;
    std::function<bool(const std::filesystem::path &)> m_isPlayable;

    /** Main thread only (update(), or the synchronous virtual-root swap while !m_working). */
    std::filesystem::path m_path;
    std::vector<FileEntry> m_content;

    /** Main thread only, mutated only while the worker is idle; the worker uses the source captured at launch. */
    DataSource *m_activeSource;
    DataSource *m_sourceBeforeScan; ///< restored by update() when a scan fails (nullopt)

    /** Kind of work a worker run performs. */
    enum class WorkKind { None, Scan, Fetch };
    WorkKind m_workKind = WorkKind::None; ///< main thread only: what the current/last worker is doing

    /**
     * Main thread only: the source captured at worker start, targeted by cancel().
     *
     * May differ from m_activeSource for playlist-replay fetches (requestFileFromSource downloads from a
     * non-browsed source). The cross-thread part of cancellation is DataSource::cancel()'s own atomic.
     */
    DataSource *m_workSource = nullptr;

    /**
     * Set by navigateToParent() when it reaches the virtual root, applied by update() next frame.
     *
     * Deferred (not swapped in place) because navigation callbacks fire mid-draw while the Gui is iterating
     * m_content: clearing it synchronously would shrink the vector under the list clipper. Main thread only.
     */
    bool m_pendingVirtualRoot = false;

    /**
     * Browser scroll-restore signalling (main thread only).
     *
     * The scan that carries a navigation is asynchronous: the direction is known at the navigate call but the
     * listing only swaps in later, in update(). m_pendingNavDirection records the in-flight scan's direction;
     * update() promotes it to m_navSignal exactly when the listing swaps (a failed scan discards it, emitting
     * nothing).
     */
    NavKind m_pendingNavDirection = NavKind::None; ///< direction of the in-flight scan, consumed by update()
    NavKind m_navSignal = NavKind::None;           ///< emitted to the app layer; read+cleared by consumeNavigation()

    /** Worker synchronization. */
    std::atomic_bool m_working;
    std::thread m_worker;
    mutable std::mutex m_mutex;
    std::filesystem::path m_pendingPath;      ///< m_mutex
    std::vector<FileEntry> m_pendingContent;  ///< m_mutex
    bool m_scanSucceeded;                     ///< m_mutex
    std::optional<FetchResult> m_fetchResult; ///< m_mutex

public:
    FileSystem(const FileSystem &) = delete;
    FileSystem &operator=(const FileSystem &) = delete;
    explicit FileSystem();
    ~FileSystem() = default;

public:
    /**
     * Takes ownership of the sources, activates sources[0], and scans it at startPath.
     *
     * With no sources it shows the virtual root directly. isPlayable filters files out of scan listings; it runs
     * on the worker thread, so it must only read state that stays immutable while the browser is alive.
     */
    void create(
        std::vector<std::unique_ptr<DataSource>> sources,
        const std::filesystem::path &startPath,
        std::function<bool(const std::filesystem::path &)> isPlayable
    );
    /**
     * Joins the worker, then releases the sources.
     *
     * Ordering matters: releasing the sources runs FtpDataSource's curl_easy_cleanup, so this must run before
     * curl_global_cleanup(); and the worker's isPlayable predicate calls into PlayerController, so it must run
     * before PlayerController::destroy().
     */
    void destroy();

    /**
     * Descends into entry: a directory starts a scan; a virtual-root source is activated and scanned at its root.
     *
     * Ignored while a scan or fetch is in flight. Main thread only.
     */
    void navigateToEntry(const FileEntry &entry);
    /**
     * Ascends one directory; from a source root it defers the virtual-root transition to the next update().
     *
     * Ignored while a scan or fetch is in flight. Main thread only.
     */
    void navigateToParent();
    /**
     * Resolves entry (in the current directory) to a locally-openable file on the worker; the result is parked
     * for consumeFetchResult().
     *
     * Ignored while a scan or fetch is in flight. Main thread only.
     */
    void requestFile(const FileEntry &entry);
    /**
     * Fetches a file addressed by (source index, source-relative path) for playlist replay, without changing the
     * browsed location (m_activeSource / m_path stay put).
     *
     * Ignored while work is in flight or when sourceIndex is out of range. Main thread only.
     */
    void requestFileFromSource(int sourceIndex, const std::filesystem::path &path);
    /**
     * Asks the in-flight worker's source to abort its blocking listDirectory/fetchFile ASAP.
     *
     * Main thread only; no-op while idle. The worker still finishes normally — the aborted transfer surfaces as a
     * failed scan (the current listing is kept) or a failed FetchResult.
     */
    void cancel();
    /**
     * Returns a finished fetch's result and clears it (consume-once); nullopt when none is parked.
     *
     * Main thread only.
     */
    [[nodiscard]] std::optional<FetchResult> consumeFetchResult();
    /**
     * True while a finished fetch's result is parked awaiting consumeFetchResult() — the one-frame window after
     * the worker clears m_working and before the next update() consumes the result.
     *
     * Lets the UI keep the download overlay up across the fetch → decode hand-off.
     */
    [[nodiscard]] bool hasPendingFetchResult() const;
    /**
     * Returns the one-frame navigation signal and clears it (consume-once); NavKind::None when no listing swapped
     * in since the last call.
     *
     * Main thread only.
     */
    [[nodiscard]] NavKind consumeNavigation();
    /**
     * Joins a finished worker, swaps a successful scan into m_path/m_content, and applies the deferred
     * virtual-root transition.
     *
     * Called once per frame on the main thread, between draws — the only place the listing handed to the Gui
     * changes after create(). A failed scan keeps the current listing and restores m_activeSource; a finished
     * fetch leaves its result parked for consumeFetchResult().
     */
    void update();

public:
    /** Current location within the active source; empty at the virtual root. Main thread only. */
    [[nodiscard]] const std::filesystem::path &getPath() const;
    /**
     * Index of the source that produced the current listing, for capturing playlist entries; -1 at the virtual
     * root.
     *
     * Main thread only (like getPath()).
     */
    [[nodiscard]] int getActiveSourceIndex() const;
    /** Current listing; a reference that update() may swap between frames. Main thread only. */
    [[nodiscard]] const std::vector<FileEntry> &getContent() const;
    /** True while a scan or fetch runs on the worker; navigation and fetch requests are ignored meanwhile. */
    [[nodiscard]] bool isWorking() const;
    /** True while a file download (not a scan) is in flight; only meaningful while isWorking() is true. */
    [[nodiscard]] bool isFetching() const;

private:
    /** Joins any finished worker, records m_workKind/m_workSource for cancel(), then spawns scan()/fetch(). */
    void startWorker(WorkKind kind, DataSource *source, std::filesystem::path path);
    /** Launches a Scan of path on m_activeSource. */
    void startScan(const std::filesystem::path &path);
    /** Launches a Fetch of path on m_activeSource. */
    void startFetch(const std::filesystem::path &path);
    /**
     * Rebuilds m_content as the list of sources and clears m_activeSource.
     *
     * Mutates the listing synchronously, so it must never run during a draw (the Gui's list clipper iterates
     * m_content); it is only called from create() and update(), both outside a frame's draw.
     */
    void showVirtualRoot();
    /**
     * Worker thread: lists the directory, filters through m_isPlayable, derives type, sorts, and parks the result
     * under m_mutex; stores m_working = false last.
     */
    void scan(DataSource *source, std::filesystem::path path);
    /**
     * Worker thread: resolves path via source->fetchFile() and parks the FetchResult under m_mutex; stores
     * m_working = false last, so the parked result is already visible whenever m_working reads false.
     */
    void fetch(DataSource *source, const std::filesystem::path &path);
};


#endif //OSP2_FILEBROWSER_H
