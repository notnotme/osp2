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

#include "VisualizerController.h"

#include "visualizers/BarsVisualizer.h"


VisualizerController::VisualizerController()
    : m_activeIndex(0) {}

void VisualizerController::create() {
    // Registration order = selector order (and dispatch index). Add one emplace_back per plugin.
    m_plugins.emplace_back(std::make_unique<BarsVisualizer>());

    for (const auto &plugin : m_plugins) {
        plugin->create();
    }
}

void VisualizerController::destroy() {
    for (const auto &plugin : m_plugins) {
        plugin->destroy();
    }
    m_plugins.clear();
}

std::vector<std::string> VisualizerController::getNames() const {
    std::vector<std::string> names;
    names.reserve(m_plugins.size());
    for (const auto &plugin : m_plugins) {
        names.push_back(plugin->getName());
    }
    return names;
}

void VisualizerController::select(const std::size_t index) {
    if (index < m_plugins.size()) {
        m_activeIndex = index;
    }
}

void VisualizerController::render(const VisualFrame &frame) {
    // Guarded so render() is a safe no-op before create() / after destroy() and if the active index
    // ever falls out of range (e.g. plugins removed at runtime).
    if (m_activeIndex < m_plugins.size()) {
        m_plugins[m_activeIndex]->render(frame);
    }
}
