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

#ifndef OSP2_FTP_DATA_SOURCE_H
#define OSP2_FTP_DATA_SOURCE_H

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "DataSource.h"
#include "FileEntry.h"

// Mirror curl's own opaque handle typedef so the header stays free of <curl/curl.h>.
typedef void CURL;


// Browses a remote archive over anonymous FTP via libcurl (Modland: ftp.modland.com/pub/modules).
// Directory listings use MLSD; fetchFile downloads to a local cache mirror (chunk 7b).
// Holds one reusable easy handle for FTP control-connection reuse; no locking, as FileSystem
// serializes DataSource calls (see DataSource.h).
class FtpDataSource final : public DataSource {
public:
    FtpDataSource(std::string displayName, std::string host,
                  std::filesystem::path basePath, std::filesystem::path cacheDir);
    ~FtpDataSource() override;

    [[nodiscard]] std::string getDisplayName() const override;
    [[nodiscard]] std::filesystem::path getRootPath() const override;
    [[nodiscard]] std::string getCacheId() const override;   // the cache dir's final component
    [[nodiscard]] std::optional<std::vector<FileEntry>> listDirectory(const std::filesystem::path &path) override;
    [[nodiscard]] std::filesystem::path fetchFile(const std::filesystem::path &path) override;
    void cancel() override;

private:
    // Lazily creates m_curl on first use; false (already SDL_Logged) on allocation failure.
    [[nodiscard]] bool ensureHandle();
    // "ftp://<host>" + each component of path percent-escaped and joined by '/'.
    // trailingSlash appends '/' (required for an MLSD directory URL).
    [[nodiscard]] std::string buildUrl(const std::filesystem::path &path, bool trailingSlash) const;
    // m_cacheDir + each non-root component of the remote path, mirroring the remote layout.
    [[nodiscard]] std::filesystem::path cacheFileFor(const std::filesystem::path &path) const;
    // Whole contents of a cached listing file, regardless of age; nullopt if absent, unreadable, or
    // a read error truncated it. Freshness is a separate concern (listingCacheFresh).
    [[nodiscard]] std::optional<std::string> readListingCache(const std::filesystem::path &cacheFile) const;
    // True iff cacheFile exists and its mtime is within kListingCacheTtl (→ serve without the network).
    [[nodiscard]] bool listingCacheFresh(const std::filesystem::path &cacheFile) const;
    // Best-effort atomic (.part → rename) write of a raw MLSD response; refreshes the mtime/TTL.
    // Failures are logged and swallowed — the live response is still returned to the caller.
    void writeListingCache(const std::filesystem::path &cacheFile, const std::string &response) const;
    // Renames a fully-written <target>.part into target; removes it and logs on failure. Returns
    // success. Shared by writeListingCache and fetchFile's download staging.
    [[nodiscard]] bool commitPart(const std::filesystem::path &partFile,
                                  const std::filesystem::path &target) const;
    // NOSIGNAL + connect/stall timeouts, re-applied after every curl_easy_reset.
    void applyCommonOptions() const;

    std::string m_displayName;
    std::string m_host;
    std::filesystem::path m_basePath;
    std::filesystem::path m_cacheDir;   // download target root (chunk 7b)
    CURL *m_curl = nullptr;
    // Set from the main thread (cancel()), read on the worker by the curl progress callback to abort
    // a blocking perform(). mutable so the const applyCommonOptions() can hand its address to curl.
    mutable std::atomic<bool> m_cancelRequested{false};
};


// Maps one path component to a cache-safe form: FAT-illegal chars and controls -> '_', "."/".."
// neutralized (traversal guard). Shared by the cache-path mirror (cacheFileFor) and Platform's
// per-source cache-subdir derivation so both layouts sanitize identically.
[[nodiscard]] std::string sanitizeCachePathComponent(const std::string &component);


#endif //OSP2_FTP_DATA_SOURCE_H
