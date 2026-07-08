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

#ifndef OSP2_SETTINGS_KEYS_H
#define OSP2_SETTINGS_KEYS_H


// Single source of truth for the INI schema's section and key names. Save and restore sites live
// in different files (Application persists, Platform restores, Settings seeds defaults) — sharing
// these constants keeps a typo from silently forking a key and resetting the setting every launch.
namespace settingskeys {
    inline constexpr const char *kUserSection = "user";
    inline constexpr const char *kTheme = "theme";
    inline constexpr const char *kVisualizer = "visualizer";
    inline constexpr const char *kDefaultFolder = "default_folder";
    // Per-plugin sections are "plugin." + the plugin's name; user-defined FTP sources are
    // "source." + the source's display name (hand-edited, see Platform::buildDataSources).
    inline constexpr const char *kPluginSectionPrefix = "plugin.";
    inline constexpr const char *kSourceSectionPrefix = "source.";
    inline constexpr const char *kSourceHost = "host";
    inline constexpr const char *kSourcePath = "path";
} // namespace settingskeys


#endif //OSP2_SETTINGS_KEYS_H
