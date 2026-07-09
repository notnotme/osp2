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

#ifndef OSP2_PLUGIN_UTIL_H
#define OSP2_PLUGIN_UTIL_H

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @file
 * Header-only helpers shared by the player domain (decoder plugins, PlayerController, SongLengthDb).
 *
 * Player domain only: FileSystem::deriveType uppercases for display and deliberately stays separate.
 */

// Several decoders (libgme's gme_play, libsidplayfp's mix) emit `short` output that the plugins hand
// straight through as the int16 audio pipeline; pin the equivalence once for all of them.
static_assert(
    sizeof(short) == sizeof(std::int16_t),
    "player plugins pass decoder `short` output directly through the int16 audio pipeline"
);

/** Maps a possibly-null C string (the decoder libraries leave absent fields as nullptr) to a std::string. */
[[nodiscard]] inline std::string toString(const char *value) {
    return value != nullptr ? std::string(value) : std::string();
}

/**
 * Lowercases a string, ASCII-only, for case-insensitive filename/extension matching.
 *
 * The cast through unsigned char is required: std::tolower has undefined behavior on a plain char that is
 * negative.
 */
[[nodiscard]] inline std::string asciiToLower(const std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered;
}

/**
 * Reads a whole file into a byte vector.
 *
 * Deliberately does NOT catch: the read can throw (bad_alloc on a large file under the Switch's constrained
 * RAM), and each caller keeps its own must-not-throw try block — which also covers its library calls — and its
 * own SDL_Log prefix.
 * @return the file's bytes, or std::nullopt when the file cannot be opened
 */
[[nodiscard]] inline std::optional<std::vector<char>> readFileBytes(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }
    return std::vector<char>{std::istreambuf_iterator(file), std::istreambuf_iterator<char>()};
}

#endif // OSP2_PLUGIN_UTIL_H
