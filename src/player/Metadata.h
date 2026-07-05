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


// Per-plugin track metadata. Each decoder library exposes a different shape, so metadata travels
// as a variant of one struct per plugin family rather than a flat, mostly-empty struct. The Gui
// dispatches with std::visit; adding a plugin forces (compile error) the UI to handle its shape.

struct ModuleMetadata {              // tracker formats via libopenmpt
    std::string title;
    std::string artist;
    std::string format;              // e.g. "ScreamTracker 3" (type_long)
    std::string tracker;             // authoring tool, may be empty
    int channels;
    int patterns;
    int samples;
    int instruments;
    std::string message;             // song message, often multiline, may be empty
};

struct GmeMetadata {                 // console/arcade formats via libgme
    std::string game;
    std::string system;
    std::string author;
    std::string copyright;
    std::string comment;             // often multiline
    int trackCount;
};

struct SidMetadata {                 // Commodore 64 SID files via libsidplayfp
    std::string title;
    std::string author;
    std::string released;            // copyright / release line (PSID info string 2)
    std::string sidModel;            // "MOS 6581" / "MOS 8580", may be empty (unknown)
    std::string clock;               // "PAL" / "NTSC", may be empty (unknown)
};

// One alternative per plugin family; monostate = no track loaded.
// Future: Sc68Metadata added with its plugin.
using TrackMetadata = std::variant<std::monostate, ModuleMetadata, GmeMetadata, SidMetadata>;


#endif //OSP2_METADATA_H
