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

#ifndef OSP2_PLUGIN_SETTING_H
#define OSP2_PLUGIN_SETTING_H

#include <string>
#include <variant>
#include <vector>


struct IntRange    { int min; int max; };
// value (in PluginSetting) is an index into labels: 0 selects labels[0], and so on.
struct EnumOptions { std::vector<std::string> labels; };

struct PluginSetting {
    std::string key;                             // INI key, e.g. "stereo_separation"
    std::string label;                           // UI label, e.g. "Stereo separation"
    std::variant<IntRange, EnumOptions> shape;   // drives the widget in 6c
    int value;                                   // current value
};


#endif //OSP2_PLUGIN_SETTING_H
