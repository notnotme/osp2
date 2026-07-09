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


/**
 * Interchangeable visualization, mirroring PlayerPlugin.
 *
 * create()/destroy() give a raw-GL plugin a clear lifecycle for its shader/VBO (called once from
 * VisualizerController::create()/destroy() on the main thread, with a live GL context — the controller itself is
 * constructed before GL exists, so this two-phase lifecycle is load-bearing); ImGui-only plugins keep the no-op
 * defaults. render() runs inside the ImGui frame (between NewFrame and Render) once per frame while in
 * VISUALIZATION mode.
 */
class VisualizerPlugin {
public:
    VisualizerPlugin(const VisualizerPlugin &) = delete;
    VisualizerPlugin &operator=(const VisualizerPlugin &) = delete;
    explicit VisualizerPlugin() = default;
    virtual ~VisualizerPlugin() = default;

    /** Allocates the plugin's GL objects, if any (default: none); runs with a live GL context, main thread. */
    virtual void create() {}
    /** Frees the GL objects allocated by create(); runs while the GL context is still alive, main thread. */
    virtual void destroy() {}
    /** Stable display name; also the value persisted under [user] visualizer and matched by indexOf(). */
    [[nodiscard]] virtual std::string getName() const = 0;
    /**
     * Draws one frame into the reserved rect, with ImGui primitives or raw GL.
     *
     * Runs inside the ImGui frame (between NewFrame and Render) once per frame while in VISUALIZATION mode.
     * Animation state must advance from ImGui::GetIO().DeltaTime inside render() — never from the always-ticking
     * ImGui::GetTime() — so a hidden visualizer freezes and resumes where it left off. frame.frameCount == 0 means
     * nothing is playing: decay to rest rather than react.
     */
    virtual void render(const VisualFrame &frame) = 0;
};


#endif //OSP2_VISUALIZER_PLUGIN_H
