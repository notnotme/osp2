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

#include "Charset.h"

#include <algorithm>
#include <cstdint>

namespace {
    // Unicode replacement character emitted for any byte sequence we cannot decode.
    constexpr std::uint32_t kReplacement = 0xFFFD;

    // One Shift-JIS double-byte value paired with its Unicode BMP codepoint.
    struct SjisEntry {
        std::uint16_t sjis;
        std::uint16_t cp;
    };

    // Vendored, generated table (scripts/gen_sjis.py → ShiftJisTable.inc): only double-byte
    // sequences, sorted ascending by the SJIS value so it can be binary-searched.
    constexpr SjisEntry kShiftJisTable[] = {
#include "ShiftJisTable.inc"
    };

    // Appends `cp` (a BMP codepoint, ≤ 0xFFFF) to `out` as UTF-8 (1, 2 or 3 bytes).
    void appendUtf8(std::string &out, const std::uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::string latin1ToUtf8(const std::string_view bytes) {
        std::string out;
        out.reserve(bytes.size());
        for (const char c : bytes) {
            // Latin-1 codepoint == byte value; 0x00–0x7F stays one byte, 0x80–0xFF becomes two.
            appendUtf8(out, static_cast<std::uint8_t>(c));
        }
        return out;
    }

    std::string shiftJisToUtf8(const std::string_view bytes) {
        std::string out;
        out.reserve(bytes.size());
        std::size_t i = 0;
        while (i < bytes.size()) {
            const auto b = static_cast<std::uint8_t>(bytes[i]);
            if (b < 0x80) {
                // ASCII passthrough (0x5C is a plain backslash — no yen special-casing).
                out.push_back(static_cast<char>(b));
                ++i;
            } else if (b >= 0xA1 && b <= 0xDF) {
                // Halfwidth katakana single-byte range.
                appendUtf8(out, 0xFF61 + (b - 0xA1));
                ++i;
            } else if ((b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xFC)) {
                // Lead byte of a double-byte sequence; the trail must follow.
                if (i + 1 >= bytes.size()) {
                    appendUtf8(out, kReplacement); // truncated at end of input
                    ++i;
                    continue;
                }
                const auto trail = static_cast<std::uint8_t>(bytes[i + 1]);
                const auto sjis = static_cast<std::uint16_t>((b << 8) | trail);
                const auto *end = kShiftJisTable + std::size(kShiftJisTable);
                const auto *hit =
                    std::lower_bound(kShiftJisTable, end, sjis, [](const SjisEntry &entry, const std::uint16_t value) {
                        return entry.sjis < value;
                    });
                if (hit != end && hit->sjis == sjis) {
                    appendUtf8(out, hit->cp);
                } else {
                    appendUtf8(out, kReplacement); // unmapped double-byte sequence
                }
                i += 2; // the trail is consumed whether or not the lookup hit
            } else {
                // Invalid single byte (0x80, 0xA0, 0xFD–0xFF).
                appendUtf8(out, kReplacement);
                ++i;
            }
        }
        return out;
    }
} // namespace

std::string toUtf8(const std::string_view bytes, const Charset charset) {
    switch (charset) {
    case Charset::ShiftJis:
        return shiftJisToUtf8(bytes);
    case Charset::Latin1:
    default:
        return latin1ToUtf8(bytes);
    }
}
