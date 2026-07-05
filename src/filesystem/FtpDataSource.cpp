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

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>


namespace {

// libcurl write callback: appends the received bytes to the std::string handed via CURLOPT_WRITEDATA.
size_t writeToString(char *ptr, size_t size, size_t nmemb, void *userdata) {
    const size_t byte_count = size * nmemb;
    auto *accumulator = static_cast<std::string *>(userdata);
    accumulator->append(ptr, byte_count);
    return byte_count;
}

enum class LineOutcome {
    Parsed,      // out holds a usable entry
    Skip,        // valid but ignored (cdir/pdir self/parent reference)
    Malformed,   // no space or no type= fact — caller logs
};

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
                return LineOutcome::Skip;   // "." / ".." self and parent references
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

}   // namespace


FtpDataSource::FtpDataSource(std::string displayName, std::string host,
                             std::filesystem::path basePath, std::filesystem::path cacheDir)
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
            continue;   // skip the leading root; segments join with a single '/'
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

void FtpDataSource::applyCommonOptions() const {
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(m_curl, CURLOPT_LOW_SPEED_TIME, 30L);
}

std::optional<std::vector<FileEntry>> FtpDataSource::listDirectory(const std::filesystem::path &path) {
    if (!ensureHandle()) {
        return std::nullopt;
    }

    curl_easy_reset(m_curl);
    applyCommonOptions();

    const std::string url = buildUrl(path, /*trailingSlash=*/true);
    std::string response;
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "MLSD");
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);

    const CURLcode result = curl_easy_perform(m_curl);
    if (result != CURLE_OK) {
        SDL_Log("FtpDataSource: MLSD failed for '%s': %s",
                path.string().c_str(), curl_easy_strerror(result));
        return std::nullopt;
    }

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

std::filesystem::path FtpDataSource::fetchFile(const std::filesystem::path &path) {
    // Chunk 7b implements download-to-cache; this chunk lists only.
    SDL_Log("FtpDataSource::fetchFile not implemented yet (chunk 7b): %s", path.string().c_str());
    return {};
}
