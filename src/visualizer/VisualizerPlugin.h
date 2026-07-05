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

#ifndef OSP2_VISUALIZER_PLUGIN_H
#define OSP2_VISUALIZER_PLUGIN_H

#include <string>

#include "VisualFrame.h"


// Interchangeable visualization, mirroring PlayerPlugin. create()/destroy() give a raw-GL plugin a
// clear lifecycle for its shader/VBO (called once from VisualizerController::create()/destroy() on
// the main thread, with a live GL context); ImGui plugins leave them empty. render() runs inside the
// ImGui frame (between NewFrame and Render) once per frame while in VISUALIZATION mode.
class VisualizerPlugin {
public:
    VisualizerPlugin(const VisualizerPlugin &) = delete;
    VisualizerPlugin &operator=(const VisualizerPlugin &) = delete;
    explicit VisualizerPlugin() = default;
    virtual ~VisualizerPlugin() = default;

    virtual void create() = 0;                    // allocate GL objects if any (ImGui plugins: empty)
    virtual void destroy() = 0;                   // free them
    [[nodiscard]] virtual std::string getName() const = 0;
    virtual void render(const VisualFrame &frame) = 0;
};


#endif //OSP2_VISUALIZER_PLUGIN_H
