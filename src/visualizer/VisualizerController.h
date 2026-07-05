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

#ifndef OSP2_VISUALIZER_CONTROLLER_H
#define OSP2_VISUALIZER_CONTROLLER_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "VisualFrame.h"
#include "VisualizerPlugin.h"


// Owns the registered visualizer plugins and dispatches rendering to the active one. Mirrors
// PlayerController's ownership pattern: one emplace_back per plugin in create(), each given a
// create()/destroy() lifecycle. Independent of the player and Gui domains — main.cpp wires them.
class VisualizerController final {
private:
    std::vector<std::unique_ptr<VisualizerPlugin>> m_plugins;
    std::size_t m_activeIndex;

public:
    VisualizerController(const VisualizerController &) = delete;
    VisualizerController &operator=(const VisualizerController &) = delete;
    explicit VisualizerController();
    ~VisualizerController() = default;

    void create();
    void destroy();

    [[nodiscard]] std::vector<std::string> getNames() const;
    void select(std::size_t index);
    void render(const VisualFrame &frame);
};


#endif //OSP2_VISUALIZER_CONTROLLER_H
