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

#ifndef OSP2_SHADER_QUAD_VISUALIZER_H
#define OSP2_SHADER_QUAD_VISUALIZER_H

#include <string>

#include "../VisualFrame.h"
#include "../VisualizerPlugin.h"

// Forward-declared so <imgui.h> stays out of this header (the callback signature is the only place it
// leaks); <glad/glad.h> stays out too — GL object names live as unsigned int / int, resolved in the .cpp.
struct ImDrawList;
struct ImDrawCmd;


/**
 * The raw-GL plugin: an animated plasma rendered by a fullscreen-triangle fragment shader, bridged into ImGui's
 * draw data via ImDrawList::AddCallback (executed by the GL3 backend before it draws its own geometry).
 *
 * It exercises the GL render path the ImGui BarsVisualizer does not. Uses #version 330 core (proven portable on
 * desktop + Switch here) and confines its draw to the reserved rect with glViewport + glScissor (Y-flipped, since
 * GL's framebuffer origin is bottom-left).
 */
class ShaderQuadVisualizer final : public VisualizerPlugin {
private:
    unsigned int m_program = 0; ///< GL program (0 = not built / build failed → render() is a no-op).
    unsigned int m_vao = 0;     ///< Empty VAO required by core profile for the attribute-less draw.
    int m_locTime = -1;         ///< Uniform location for the animation clock (-1 = not found).
    int m_locLevel = -1;        ///< Uniform location for the audio level (-1 = not found).
    // Captured in render() for the deferred GL callback, which runs later inside RenderDrawData:
    float m_time = 0.0f;  ///< Accumulated render-delta (freeze-when-hidden contract, not GetTime()).
    float m_level = 0.0f; ///< Audio amplitude in [0, 1], drives the plasma's pulse.
    int m_vpX = 0, m_vpY = 0, m_vpW = 0, m_vpH = 0; ///< Reserved rect in framebuffer pixels, Y-flipped.

public:
    /**
     * Compiles and links the plasma program.
     *
     * On failure it logs the compile/link info log and leaves m_program at 0, making render() a safe no-op.
     */
    void create() override;
    void destroy() override;
    [[nodiscard]] std::string getName() const override;
    /**
     * Schedules the GL draw instead of drawing immediately: captures this frame's uniforms/viewport, then queues
     * glDrawCallback (followed by ImDrawCallback_ResetRenderState) on the background draw list.
     */
    void render(const VisualFrame &frame) override;

private:
    /**
     * ImDrawList::AddCallback trampoline: unpacks `this` from cmd->UserCallbackData and issues the raw-GL draw.
     *
     * Runs during ImGui_ImplOpenGL3_RenderDrawData, so the GL context is current.
     */
    static void glDrawCallback(const ImDrawList *parentList, const ImDrawCmd *cmd);
};


#endif //OSP2_SHADER_QUAD_VISUALIZER_H
