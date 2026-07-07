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

#include "SongLengthDb.h"

#include <SDL_log.h>

#include <cctype>
#include <exception>
#include <fstream>
#include <sstream>

namespace {
    // HVSC's file has ~60k entries; reserving up front avoids rehashing during the load.
    constexpr std::size_t EXPECTED_ENTRIES = 60000;

    // Parses one "M:SS" or "M:SS.mmm" time token into seconds. The fractional part, when present,
    // is milliseconds. Returns std::nullopt for a malformed token so a single bad entry is skipped
    // rather than discarding the whole line.
    std::optional<double> parseTime(const std::string &token) {
        const std::string::size_type colon = token.find(':');
        if (colon == std::string::npos) {
            return std::nullopt;
        }
        try {
            const int minutes = std::stoi(token.substr(0, colon));
            // std::stod handles both "SS" and "SS.mmm" — the fractional seconds carry the ms.
            const double seconds = std::stod(token.substr(colon + 1));
            return minutes * 60 + seconds;
        } catch (const std::exception &) {
            return std::nullopt;
        }
    }
} // namespace

bool SongLengthDb::load(const std::filesystem::path &path) {
    m_lengths.clear();
    try {
        // Binary so the CRLF line endings survive the read; the trailing '\r' is stripped per line.
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return false; // absent database is expected — durations stay open-ended, no log noise
        }
        m_lengths.reserve(EXPECTED_ENTRIES);

        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            // Skip blank lines, the "[Database]" header, and "; /path" comment lines.
            if (line.empty() || line.front() == '[' || line.front() == ';') {
                continue;
            }
            const std::string::size_type equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }

            std::string md5 = line.substr(0, equals);
            // Keys are already lowercase hex; lowercase defensively so a lookup can rely on it.
            for (char &c : md5) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            std::vector<double> times;
            std::istringstream tokens(line.substr(equals + 1));
            std::string token;
            while (tokens >> token) {
                if (const std::optional<double> seconds = parseTime(token)) {
                    times.push_back(*seconds);
                }
            }
            if (!times.empty()) {
                m_lengths.emplace(std::move(md5), std::move(times));
            }
        }
        return true;
    } catch (const std::exception &e) {
        SDL_Log("SongLengthDb: failed to load %s: %s", path.string().c_str(), e.what());
        m_lengths.clear();
        return false;
    }
}

std::optional<double> SongLengthDb::lookup(const std::string &md5, const unsigned int subtuneIndex) const {
    const auto it = m_lengths.find(md5);
    if (it == m_lengths.end() || subtuneIndex >= it->second.size()) {
        return std::nullopt;
    }
    return it->second[subtuneIndex];
}
