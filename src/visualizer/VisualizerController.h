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
#include <optional>
#include <string>
#include <vector>

#include "VisualFrame.h"
#include "VisualizerPlugin.h"


/**
 * Owns the registered visualizer plugins and dispatches rendering to the active one.
 *
 * Mirrors PlayerController's ownership pattern: one emplace_back per plugin in create(), each given a
 * create()/destroy() lifecycle. Independent of the player and Gui domains — Application bridges them.
 */
class VisualizerController final {
private:
    std::vector<std::unique_ptr<VisualizerPlugin>> m_plugins; ///< Registration order = selector/dispatch index.
    std::size_t m_activeIndex;                                ///< Plugin render() dispatches to; defaults to 0.

public:
    VisualizerController(const VisualizerController &) = delete;
    VisualizerController &operator=(const VisualizerController &) = delete;
    explicit VisualizerController();
    ~VisualizerController() = default;

    /**
     * Registers the plugins and runs each one's create().
     *
     * Main thread, with a live GL context — the controller itself is constructed before GL exists. Every
     * registered plugin is created, not just the active one; selecting only changes which one renders.
     */
    void create();
    /**
     * Runs each plugin's destroy() and drops the plugins.
     *
     * Main thread; must run while the GL context is still alive so plugins can free their GL objects.
     */
    void destroy();

    /** Plugin display names in registration (= selection) order. Allocates — cache it rather than call per frame. */
    [[nodiscard]] std::vector<std::string> getNames() const;
    /**
     * Index of the plugin whose getName() equals `name`, or nullopt if none match.
     *
     * Lets the platform layer resolve a persisted stable plugin name back to a selectable index.
     */
    [[nodiscard]] std::optional<std::size_t> indexOf(const std::string &name) const;
    [[nodiscard]] std::size_t getActiveIndex() const;
    /** Makes the plugin at index the render target; bounds-checked no-op when index is out of range. */
    void select(std::size_t index);
    /**
     * Forwards the frame to the active plugin.
     *
     * Called inside the ImGui frame, once per frame while in VISUALIZATION mode. The out-of-range guard makes it a
     * safe no-op before create() / after destroy().
     */
    void render(const VisualFrame &frame) const;
};


#endif //OSP2_VISUALIZER_CONTROLLER_H
