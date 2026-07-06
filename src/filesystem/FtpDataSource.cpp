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

#include "FtpDataSource.h"

#include <curl/curl.h>
#include <SDL.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>


namespace {

    // curl progress callback: returning non-zero aborts the transfer. Wired to the cancel flag so the
    // main thread can abort a stuck perform() on the worker.
    int xferInfoCallback(void *clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
        return static_cast<std::atomic<bool> *>(clientp)->load() ? 1 : 0;
    }

    // libcurl write callback: appends the received bytes to the std::string handed via CURLOPT_WRITEDATA.
    size_t writeToString(char *ptr, size_t size, size_t nmemb, void *userdata) {
        const size_t byte_count = size * nmemb;
        auto *accumulator = static_cast<std::string *>(userdata);
        accumulator->append(ptr, byte_count);
        return byte_count;
    }

    // libcurl write callback: streams received bytes to the std::ofstream handed via CURLOPT_WRITEDATA.
    // Returning a short count aborts the transfer (used to surface a disk-write failure to curl).
    size_t writeToFile(char *ptr, size_t size, size_t nmemb, void *userdata) {
        const size_t byte_count = size * nmemb;
        auto *out = static_cast<std::ofstream *>(userdata);
        out->write(ptr, static_cast<std::streamsize>(byte_count));
        return out->good() ? byte_count : 0;
    }

    enum class LineOutcome {
        Parsed,    // out holds a usable entry
        Skip,      // valid but ignored (MLSD cdir/pdir self/parent refs; LIST "total"/device lines)
        Malformed, // unparseable for this listing format — caller logs and skips
    };

    // A curl result that means the control connection never came up (DNS/connect/timeout), as opposed
    // to the server actively rejecting a command. Used to decide whether the LIST fallback is worth it:
    // a rejected MLSD command means the connection is alive, so LIST is cheap; a dead connection must
    // NOT be retried, else we burn the connect/stall timeout a second time and double the UI freeze.
    bool isConnectionFailure(CURLcode code) {
        return code == CURLE_COULDNT_RESOLVE_PROXY || code == CURLE_COULDNT_RESOLVE_HOST ||
            code == CURLE_COULDNT_CONNECT || code == CURLE_OPERATION_TIMEDOUT;
    }

    // One MLSD entry: "fact1=val1;fact2=val2; name". Facts are ';'-terminated key=value pairs, then a
    // single space, then the name (which may itself contain spaces).
    LineOutcome parseMlsdLine(const std::string &line, FileEntry &out) {
        const auto space = line.find(' ');
        if (space == std::string::npos) {
            return LineOutcome::Malformed;
        }

        const std::string name = line.substr(space + 1);
        if (name.empty()) {
            return LineOutcome::Malformed;
        }

        bool is_directory = false;
        bool type_seen = false;
        std::int64_t file_size = 0;

        const std::string facts = line.substr(0, space);
        size_t start = 0;
        while (start < facts.size()) {
            const auto semicolon = facts.find(';', start);
            const auto end = (semicolon == std::string::npos) ? facts.size() : semicolon;
            const std::string fact = facts.substr(start, end - start);
            start = (semicolon == std::string::npos) ? facts.size() : semicolon + 1;

            const auto equals = fact.find('=');
            if (equals == std::string::npos) {
                continue;
            }
            const std::string key = fact.substr(0, equals);
            const std::string value = fact.substr(equals + 1);

            if (key == "type") {
                type_seen = true;
                if (value == "cdir" || value == "pdir") {
                    return LineOutcome::Skip; // "." / ".." self and parent references
                }
                is_directory = (value == "dir");
            } else if (key == "size") {
                file_size = static_cast<std::int64_t>(std::strtoll(value.c_str(), nullptr, 10));
            }
        }

        if (!type_seen) {
            return LineOutcome::Malformed;
        }

        out = FileEntry{name, is_directory ? 0 : file_size, "", is_directory};
        return LineOutcome::Parsed;
    }

    // A cached listing is served without hitting the network while its file is younger than this.
    constexpr std::chrono::minutes kListingCacheTtl{30};

    // Transfer timeouts. Fetches/scans are UI-modal (the overlay blocks the UI), so a dead connection
    // must not freeze the app for long. All are <= 7.32-vintage curl options (switch-curl 7.69.1 ok).
    constexpr long kConnectTimeoutSeconds = 10;
    constexpr long kStallSpeedBytesPerSec = 1; // below this...
    constexpr long kStallTimeoutSeconds = 15;  // ...for this long -> abort (was 30)

    // Splits a raw MLSD response into FileEntry rows (trailing \r stripped, empty lines skipped,
    // malformed lines logged and skipped). Shared by the live-fetch and cache-hit paths.
    std::vector<FileEntry> parseMlsdListing(const std::string &response) {
        std::vector<FileEntry> entries;
        size_t start = 0;
        while (start < response.size()) {
            auto newline = response.find('\n', start);
            const auto end = (newline == std::string::npos) ? response.size() : newline;
            std::string line = response.substr(start, end - start);
            start = (newline == std::string::npos) ? response.size() : newline + 1;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }

            FileEntry entry;
            switch (parseMlsdLine(line, entry)) {
            case LineOutcome::Parsed:
                entries.push_back(std::move(entry));
                break;
            case LineOutcome::Skip:
                break;
            case LineOutcome::Malformed:
                SDL_Log("FtpDataSource: skipping unparseable MLSD line: '%s'", line.c_str());
                break;
            }
        }
        return entries;
    }

    // One Unix `ls -l`-style LIST line, e.g.:
    //   "drwxr-xr-x   2 owner group     4096 Jul  5 12:00 Some Dir Name"
    //   "-rw-r--r--   1 owner group   123456 Jul  5 12:00 tune.mod"
    // Standard 9-field layout: perms links owner group size month day time/year name (name may hold
    // spaces). Only the leading type char and the size field are load-bearing; the date fields are
    // skipped. Used only for arbitrary user servers that reject MLSD (Modland uses MLSD).
    LineOutcome parseListLine(const std::string &line, FileEntry &out) {
        if (line.empty()) {
            return LineOutcome::Malformed;
        }

        const char type = line.front();
        if (type != 'd' && type != '-' && type != 'l') {
            return LineOutcome::Skip; // "total N" header, plus device/socket/fifo entries we don't browse
        }

        // Read the first 8 whitespace-delimited fields; index 4 (0-based) is the size. After the 8th
        // field, the remainder of the line (past the whitespace) is the name, preserving internal spaces.
        std::array<std::string, 8> fields;
        size_t field_count = 0;
        size_t pos = 0;
        while (field_count < 8) {
            while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
                ++pos;
            }
            if (pos >= line.size()) {
                break;
            }
            const size_t start = pos;
            while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t') {
                ++pos;
            }
            fields[field_count++] = line.substr(start, pos - start);
        }
        if (field_count < 8) {
            return LineOutcome::Malformed;
        }

        // Skip the whitespace after the 8th field; the rest is the name.
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
            ++pos;
        }
        std::string name = line.substr(pos);
        if (name.empty()) {
            return LineOutcome::Malformed;
        }

        // A symlink line reads "name -> target"; strip the target and treat the link as a file.
        if (type == 'l') {
            if (const auto arrow = name.find(" -> "); arrow != std::string::npos) {
                name.erase(arrow);
            }
        }

        if (name == "." || name == "..") {
            return LineOutcome::Skip;
        }

        const bool is_directory = (type == 'd');
        const auto file_size = static_cast<std::int64_t>(std::strtoll(fields[4].c_str(), nullptr, 10));
        out = FileEntry{name, is_directory ? 0 : file_size, "", is_directory};
        return LineOutcome::Parsed;
    }

    // Splits a raw LIST response into FileEntry rows, mirroring parseMlsdListing (trailing \r stripped,
    // empty lines skipped, malformed lines logged and skipped) but delegating to parseListLine.
    std::vector<FileEntry> parseListListing(const std::string &response) {
        std::vector<FileEntry> entries;
        size_t start = 0;
        while (start < response.size()) {
            auto newline = response.find('\n', start);
            const auto end = (newline == std::string::npos) ? response.size() : newline;
            std::string line = response.substr(start, end - start);
            start = (newline == std::string::npos) ? response.size() : newline + 1;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }

            FileEntry entry;
            switch (parseListLine(line, entry)) {
            case LineOutcome::Parsed:
                entries.push_back(std::move(entry));
                break;
            case LineOutcome::Skip:
                break;
            case LineOutcome::Malformed:
                SDL_Log("FtpDataSource: skipping unparseable LIST line: '%s'", line.c_str());
                break;
            }
        }
        return entries;
    }

} // namespace


// FAT-illegal characters (and controls) in a path component -> '_', so the cache mirror is writable
// on the Switch's FAT SD card; applied on both platforms so the two cache layouts stay identical.
// Also neutralizes "." / ".." so a hostile or broken server can't escape the cache dir via traversal.
std::string sanitizeCachePathComponent(const std::string &component) {
    if (component == "." || component == "..") {
        // Braced-init would select the initializer_list<char> ctor and narrow size() to a
        // char, changing behavior, so keep the explicit (size_t, char) constructor.
        // NOLINTNEXTLINE(modernize-return-braced-init-list)
        return std::string(component.size(), '_'); // "." -> "_", ".." -> "__"
    }
    std::string sanitized = component;
    for (char &c : sanitized) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|') {
            c = '_';
        }
    }
    return sanitized;
}


FtpDataSource::FtpDataSource(
    std::string displayName, std::string host, std::filesystem::path basePath, std::filesystem::path cacheDir
)
    : m_displayName(std::move(displayName)),
      m_host(std::move(host)),
      m_basePath(std::move(basePath)),
      m_cacheDir(std::move(cacheDir)) {}

FtpDataSource::~FtpDataSource() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
}

std::string FtpDataSource::getDisplayName() const {
    return m_displayName;
}

std::filesystem::path FtpDataSource::getRootPath() const {
    return m_basePath;
}

std::string FtpDataSource::getCacheId() const {
    // m_cacheDir is <cache root>/<id>; the id is the component the caller derived per source.
    return m_cacheDir.filename().string();
}

bool FtpDataSource::ensureHandle() {
    if (m_curl) {
        return true;
    }
    m_curl = curl_easy_init();
    if (!m_curl) {
        SDL_Log("FtpDataSource: curl_easy_init failed");
        return false;
    }
    return true;
}

std::string FtpDataSource::buildUrl(const std::filesystem::path &path, bool trailingSlash) const {
    std::string url = "ftp://" + m_host;
    for (const auto &component : path) {
        const std::string part = component.string();
        if (part.empty() || part == "/") {
            continue; // skip the leading root; segments join with a single '/'
        }
        char *escaped = curl_easy_escape(m_curl, part.c_str(), static_cast<int>(part.size()));
        url += '/';
        if (escaped) {
            url += escaped;
            curl_free(escaped);
        } else {
            url += part;
        }
    }
    if (trailingSlash) {
        url += '/';
    }
    return url;
}

std::filesystem::path FtpDataSource::cacheFileFor(const std::filesystem::path &path) const {
    std::filesystem::path cacheFile = m_cacheDir;
    for (const auto &component : path) {
        const std::string part = component.string();
        if (part.empty() || part == "/") {
            continue; // skip the leading root; mirror only real path components
        }
        cacheFile /= sanitizeCachePathComponent(part);
    }
    return cacheFile;
}

void FtpDataSource::applyCommonOptions() const {
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, kStallSpeedBytesPerSec);
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, kStallTimeoutSeconds);
    // Progress callback abort path: the flag is polled by curl during perform() so a main-thread
    // cancel() aborts the transfer promptly (returns CURLE_ABORTED_BY_CALLBACK).
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, &m_cancelRequested);
}

std::optional<std::string> FtpDataSource::readListingCache(const std::filesystem::path &cacheFile) const {
    std::error_code ec;
    const auto expected = std::filesystem::file_size(cacheFile, ec);
    if (ec) {
        return std::nullopt; // absent or unstatable
    }

    std::ifstream in(cacheFile, std::ios::binary);
    if (!in) {
        return std::nullopt; // unopenable
    }

    // Read exactly the expected size and verify the count: a short read (I/O error, or the file
    // shrinking under us) yields fewer bytes, which we reject rather than serve a truncated listing.
    std::string content(static_cast<std::size_t>(expected), '\0');
    in.read(content.data(), static_cast<std::streamsize>(expected));
    if (static_cast<std::uintmax_t>(in.gcount()) != expected) {
        return std::nullopt;
    }
    return content;
}

bool FtpDataSource::listingCacheFresh(const std::filesystem::path &cacheFile) const {
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(cacheFile, ec);
    if (ec) {
        return false; // absent or unstatable
    }
    return std::filesystem::file_time_type::clock::now() - mtime <= kListingCacheTtl;
}

bool FtpDataSource::commitPart(const std::filesystem::path &partFile, const std::filesystem::path &target) const {
    std::error_code ec;
    std::filesystem::rename(partFile, target, ec);
    if (ec) {
        SDL_Log("FtpDataSource: cannot move '%s' into place: %s", partFile.string().c_str(), ec.message().c_str());
        std::filesystem::remove(partFile, ec);
        return false;
    }
    return true;
}

void FtpDataSource::writeListingCache(const std::filesystem::path &cacheFile, const std::string &response) const {
    std::error_code ec;
    std::filesystem::create_directories(cacheFile.parent_path(), ec);
    if (ec) {
        SDL_Log(
            "FtpDataSource: cannot create cache directory '%s': %s",
            cacheFile.parent_path().string().c_str(),
            ec.message().c_str()
        );
        return; // non-fatal: the live response is still returned to the caller
    }

    // Write to a sibling .part file, then rename on success so the cache never holds a truncated listing.
    std::filesystem::path partFile = cacheFile;
    partFile += ".part";

    std::ofstream out(partFile, std::ios::binary | std::ios::trunc);
    if (!out) {
        SDL_Log("FtpDataSource: cannot open '%s' for writing", partFile.string().c_str());
        return;
    }
    out.write(response.data(), static_cast<std::streamsize>(response.size()));
    out.close();
    if (!out) {
        SDL_Log("FtpDataSource: write/flush failed for '%s'", partFile.string().c_str());
        std::filesystem::remove(partFile, ec);
        return;
    }

    static_cast<void>(commitPart(partFile, cacheFile)); // best-effort; failure already logged and cleaned up
}

std::optional<std::vector<FileEntry>> FtpDataSource::listDirectory(const std::filesystem::path &path) {
    // Clear any stale cancel from a previously aborted call so it never carries into this one.
    m_cancelRequested.store(false);

    const std::filesystem::path cacheFile = cacheFileFor(path) / ".listing";

    // A cached listing (any age) doubles as the offline fallback below; read it once up front.
    const auto cached = readListingCache(cacheFile);

    // Fresh (< 30 min): serve it without touching the network.
    if (cached && listingCacheFresh(cacheFile)) {
        return parseMlsdListing(*cached);
    }

    // Stale, absent, or unreadable: go online (unless the handle can't be created).
    if (ensureHandle()) {
        const std::string url = buildUrl(path, /*trailingSlash=*/true);

        // MLSD first: machine-readable, and the only format we cache (see below).
        curl_easy_reset(m_curl);
        applyCommonOptions();

        std::string response;
        curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "MLSD");
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);

        if (const CURLcode result = curl_easy_perform(m_curl); result == CURLE_OK) {
            writeListingCache(cacheFile, response); // best-effort; refreshes the mtime/TTL
            return parseMlsdListing(response);
        } else if (result == CURLE_ABORTED_BY_CALLBACK) {
            // The user cancelled: abort the navigation entirely and stay put, rather than falling
            // back to a stale cache (which would drop them inside the folder showing old data).
            SDL_Log("FtpDataSource: listing cancelled for '%s'", path.string().c_str());
            return std::nullopt;
        } else if (!isConnectionFailure(result)) {
            // MLSD reached the server and was rejected (server lacks it): the control connection is
            // alive, so a LIST retry is cheap. On a dead connection we skip straight to the stale-cache
            // fallback instead — retrying would burn the connect/stall timeout a second time.
            SDL_Log(
                "FtpDataSource: MLSD failed for '%s': %s — trying LIST",
                path.string().c_str(),
                curl_easy_strerror(result)
            );

            // LIST fallback: a plain LIST on the directory URL (no CUSTOMREQUEST). NOT cached — the
            // .listing cache stores raw MLSD text and the cache-hit path re-parses it with
            // parseMlsdListing, so caching LIST text there would corrupt cache-hit parsing.
            response.clear();
            curl_easy_reset(m_curl);
            applyCommonOptions();

            curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeToString);
            curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);

            if (const CURLcode listResult = curl_easy_perform(m_curl); listResult == CURLE_OK) {
                return parseListListing(response);
            } else if (listResult == CURLE_ABORTED_BY_CALLBACK) {
                SDL_Log("FtpDataSource: listing cancelled for '%s'", path.string().c_str());
                return std::nullopt;
            } else {
                SDL_Log(
                    "FtpDataSource: LIST failed for '%s': %s", path.string().c_str(), curl_easy_strerror(listResult)
                );
            }
        } else {
            SDL_Log("FtpDataSource: MLSD failed for '%s': %s", path.string().c_str(), curl_easy_strerror(result));
        }
    }

    // Network unavailable or the transfer failed (but not a user cancel): fall back to a stale cached
    // listing if we have one, so a recently-visited directory still browses offline. Else keep put.
    if (cached) {
        SDL_Log("FtpDataSource: serving stale cached listing for '%s'", path.string().c_str());
        return parseMlsdListing(*cached);
    }
    return std::nullopt;
}

std::filesystem::path FtpDataSource::fetchFile(const std::filesystem::path &path) {
    // Clear any stale cancel from a previously aborted call so it never carries into this one.
    m_cancelRequested.store(false);

    std::filesystem::path cacheFile = cacheFileFor(path);

    // Cache hit: a non-empty file already mirrors this remote path (!ec also covers non-existence).
    std::error_code ec;
    if (const auto size = std::filesystem::file_size(cacheFile, ec); !ec && size > 0) {
        return cacheFile;
    }

    if (!ensureHandle()) {
        return {};
    }

    std::filesystem::create_directories(cacheFile.parent_path(), ec);
    if (ec) {
        SDL_Log(
            "FtpDataSource: cannot create cache directory '%s': %s",
            cacheFile.parent_path().string().c_str(),
            ec.message().c_str()
        );
        return {};
    }

    // Download to a sibling .part file, then rename on success so the cache never holds a truncated file.
    std::filesystem::path partFile = cacheFile;
    partFile += ".part";

    std::ofstream out(partFile, std::ios::binary | std::ios::trunc);
    if (!out) {
        SDL_Log("FtpDataSource: cannot open '%s' for writing", partFile.string().c_str());
        return {};
    }

    curl_easy_reset(m_curl);
    applyCommonOptions();

    const std::string url = buildUrl(path, /*trailingSlash=*/false);
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &out);

    const CURLcode result = curl_easy_perform(m_curl);
    out.close();

    if (result != CURLE_OK) {
        SDL_Log("FtpDataSource: download failed for '%s': %s", path.string().c_str(), curl_easy_strerror(result));
        std::filesystem::remove(partFile, ec);
        return {};
    }

    // ofstream buffers internally, so the final flush happens at close(); a failure there (e.g. the
    // disk filling on the tail write) leaves a truncated .part that curl still reported as CURLE_OK.
    // Check it before renaming, else the truncated file would satisfy the size>0 cache hit forever.
    if (!out) {
        SDL_Log("FtpDataSource: write/flush failed for '%s'", partFile.string().c_str());
        std::filesystem::remove(partFile, ec);
        return {};
    }

    if (!commitPart(partFile, cacheFile)) {
        return {};
    }
    return cacheFile;
}

void FtpDataSource::cancel() {
    m_cancelRequested.store(true);
}
