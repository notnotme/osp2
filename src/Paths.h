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

#ifndef OSP2_PATHS_H
#define OSP2_PATHS_H

#include <filesystem>

#include <SDL_filesystem.h>
#include <SDL_stdinc.h>


// Single source of path truth for OSP2.

// Absolute path to a read-only asset bundled under the romfs root, e.g.
// assetPath("font/Roboto-Regular.ttf"). romfs is mounted at "romfs:/" on the Switch and shipped
// alongside the executable as "romfs/" on desktop.
inline std::filesystem::path assetPath(const std::filesystem::path &relative) {
#if defined(__SWITCH__)
    return std::filesystem::path("romfs:/") / relative;
#else
    return std::filesystem::path("romfs/") / relative;
#endif
}

// A writable file next to the executable on desktop; falls back to a bare relative path when
// SDL cannot resolve the base directory. Switch callers do not use this — romfs is read-only,
// so they target fixed SD-card ("/switch/…") locations instead.
inline std::filesystem::path nextToExecutable(const char *relative) {
    char *base = SDL_GetBasePath();
    if (!base) {
        return relative;
    }
    std::filesystem::path path = std::filesystem::path(base) / relative;
    SDL_free(base);
    return path;
}

// The INI lives next to the executable on desktop (git-ignored build dir); romfs is read-only
// on the Switch, so it goes to writable SD-card storage instead ("/" is libnx's default sdmc device).
inline std::filesystem::path configPath() {
#if defined(__SWITCH__)
    return "/switch/OSP2/osp2.ini";
#else
    return nextToExecutable("osp2.ini");
#endif
}

// Remote sources download to this writable cache root. Mirrors configPath()'s convention:
// SD-card storage on the Switch (romfs is read-only), next to the executable on desktop.
inline std::filesystem::path cachePath() {
#if defined(__SWITCH__)
    return "/switch/OSP2/cache/";
#else
    return nextToExecutable("cache/");
#endif
}


#endif //OSP2_PATHS_H
