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

#ifndef OSP2_METADATA_H
#define OSP2_METADATA_H

#include <string>
#include <variant>


/** Metadata of tracker formats, via libopenmpt. */
struct ModuleMetadata {
    std::string title;
    std::string artist;
    std::string format;  ///< e.g. "ScreamTracker 3" (type_long).
    std::string tracker; ///< Authoring tool, may be empty.
    int channels;
    int patterns;
    int samples;
    int instruments;
    std::string message; ///< Song message, often multiline, may be empty.
};

/** Metadata of console/arcade formats, via libgme. */
struct GmeMetadata {
    std::string game;
    std::string system;
    std::string author;
    std::string copyright;
    std::string comment; ///< Often multiline.
    int trackCount;
};

/** Metadata of Commodore 64 SID files, via libsidplayfp. */
struct SidMetadata {
    std::string title;
    std::string author;
    std::string released; ///< Copyright / release line (PSID info string 2).
    std::string sidModel; ///< "MOS 6581" / "MOS 8580", may be empty (unknown).
    std::string clock;    ///< "PAL" / "NTSC", may be empty (unknown).
};

/** Metadata of Atari ST / Amiga / SNDH files, via libsc68. */
struct Sc68Metadata {
    std::string title;
    std::string author;
    std::string composer; ///< From a "composer" tag, may be empty (absent).
    std::string hardware; ///< e.g. "Atari ST", may be empty (unknown).
    std::string ripper;   ///< Who ripped the tune, may be empty.
};

/**
 * Per-plugin track metadata: one alternative per plugin family; monostate = no track loaded.
 *
 * Each decoder library exposes a different shape, so metadata travels as a variant of one struct per plugin
 * family rather than a flat, mostly-empty struct. The Gui dispatches with std::visit; adding a plugin forces
 * (compile error) the UI to handle its shape. The variant is complete: sc68 is the last planned decoder, so every
 * supported format maps to one alternative.
 */
using TrackMetadata = std::variant<std::monostate, ModuleMetadata, GmeMetadata, SidMetadata, Sc68Metadata>;


#endif //OSP2_METADATA_H
