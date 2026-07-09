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

#ifndef OSP2_CHARSET_H
#define OSP2_CHARSET_H

#include <string>
#include <string_view>

/**
 * Legacy source charsets in which decoder libraries hand OSP2 their metadata bytes.
 *
 * Dear ImGui only renders UTF-8, so every non-UTF-8 field is transcoded at the plugin boundary before it is
 * stored.
 */
enum class Charset {
    Latin1,  ///< ISO-8859-1 — libsidplayfp (PSID/STIL) and libsc68 tags.
    ShiftJis ///< Shift-JIS — libgme (NSF/GBS/… header fields, ASCII-compatible).
};

/**
 * Transcodes raw metadata bytes from the given source charset to UTF-8.
 *
 * Robust to malformed or truncated input: undecodable bytes become U+FFFD (replacement character); never reads
 * out of bounds. OpenMPT metadata is already UTF-8 and must not pass through this.
 */
[[nodiscard]] std::string toUtf8(std::string_view bytes, Charset charset);

#endif // OSP2_CHARSET_H
