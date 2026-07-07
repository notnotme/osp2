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

#ifndef OSP2_SONG_LENGTH_DB_H
#define OSP2_SONG_LENGTH_DB_H

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


// Parses and holds HVSC's Songlengths.md5 database: a map from a tune's MD5 (as produced by
// SidTune::createMD5New()) to its per-subtune play times in seconds, in song order. Deals only in
// strings and times — it has no dependency on libsidplayfp. Owned outside the audio path; callers
// look up a length once (off the audio thread) and cache it.
class SongLengthDb final {
private:
    std::unordered_map<std::string, std::vector<double>> m_lengths;

public:
    // Load and parse the database at path, replacing any previously loaded contents. Returns false
    // when the file cannot be opened (a missing database is a normal, non-error condition — the
    // caller simply reports open-ended durations) or when parsing fails; never throws.
    bool load(const std::filesystem::path &path);

    // The play time in seconds of the given 0-based subtune, or std::nullopt when the MD5 is not in
    // the database or the subtune index is out of range.
    [[nodiscard]] std::optional<double> lookup(const std::string &md5, unsigned int subtuneIndex) const;
};


#endif //OSP2_SONG_LENGTH_DB_H
