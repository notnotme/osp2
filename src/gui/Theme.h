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

#ifndef OSP2_THEME_H
#define OSP2_THEME_H

#include <string>

// Color theme, mapped to ImGui's three built-in palettes by Gui::applyTheme.
enum class Theme { DARK, LIGHT, CLASSIC };

// Lossless round-trip for INI persistence: every variant serializes so CLASSIC survives.
inline std::string themeToString(const Theme theme) {
    switch (theme) {
    case Theme::LIGHT:
        return "light";
    case Theme::CLASSIC:
        return "classic";
    case Theme::DARK:
    default:
        return "dark";
    }
}

// Unknown/garbage values fall back to DARK (the safe default).
inline Theme themeFromString(const std::string &value) {
    if (value == "light") {
        return Theme::LIGHT;
    }
    if (value == "classic") {
        return Theme::CLASSIC;
    }
    return Theme::DARK;
}

#endif //OSP2_THEME_H
