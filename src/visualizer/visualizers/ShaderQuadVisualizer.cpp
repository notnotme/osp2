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

#include "ShaderQuadVisualizer.h"

#include <glad/glad.h>
#include <imgui.h>
#include <SDL.h>

#include <algorithm>
#include <cmath>


namespace {
    // #version 330 core: the same version main.cpp feeds ImGui_ImplOpenGL3_Init, and proven portable
    // on both desktop GL and the Switch here. Attribute-less: the fullscreen triangle is derived from
    // gl_VertexID, so no VBO/attributes are needed (only a bound VAO, which core profile still requires).
    const auto kVertexShader = R"(#version 330 core
out vec2 v_uv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_uv = p;                          // 0..1 across the visible viewport
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

    // A simple, robust plasma: summed sines of the UV and time. u_time is a loudness-paced clock (it
    // advances faster when the music is loud, see render()), so the whole field visibly churns with the
    // music. u_level (a smoothed audio envelope) additionally swells the radial ripple and lifts the
    // brightness. Deliberately cheap (a handful of sines, no loops, no derivatives) so it's Switch-safe.
    const auto kFragmentShader = R"(#version 330 core
in vec2 v_uv;
out vec4 frag_color;
uniform float u_time;
uniform float u_level;
void main() {
    vec2 p = v_uv * 6.2831853;                 // 0..2*PI across the rect
    float v = sin(p.x + u_time)
            + sin(p.y + u_time * 1.3)
            + sin((p.x + p.y) * 0.5 + u_time * 0.7)
            + sin(length(v_uv - 0.5) * 12.0 - u_time * 2.0) * (0.3 + 1.4 * u_level);
    v *= 0.25;                                  // fold the 4-sine sum back toward [-1, 1]
    vec3 col = 0.5 + 0.5 * cos(vec3(0.0, 2.094, 4.188) + v * 3.1416 + u_time * 0.3);
    col *= 0.7 + 0.5 * u_level;                // breathes with loudness; baseline stays lit, not dark
    frag_color = vec4(col, 1.0);               // opaque: the reserved rect is fully painted
}
)";

    // Compiles one shader stage; logs the info log and returns 0 on failure (caller aborts the build).
    unsigned int compileShader(const GLenum type, const char *const source) {
        const GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            char log[512] = {};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            SDL_Log("ShaderQuadVisualizer: shader compile failed: %s", log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }
} // namespace


void ShaderQuadVisualizer::create() {
    const unsigned int vertex = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vertex == 0 || fragment == 0) {
        // A stage failed to compile; free whichever succeeded and leave m_program == 0 (render no-op).
        if (vertex != 0) {
            glDeleteShader(vertex);
        }
        if (fragment != 0) {
            glDeleteShader(fragment);
        }
        return;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    // Shader objects are no longer needed once linked (or on link failure): detach + delete either way.
    glDetachShader(program, vertex);
    glDetachShader(program, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char log[512] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        SDL_Log("ShaderQuadVisualizer: program link failed: %s", log);
        glDeleteProgram(program);
        return;
    }

    m_program = program;
    m_locTime = glGetUniformLocation(m_program, "u_time");
    m_locLevel = glGetUniformLocation(m_program, "u_level");

    // Core profile requires a bound VAO for glDrawArrays even with no vertex attributes.
    glGenVertexArrays(1, &m_vao);
}

void ShaderQuadVisualizer::destroy() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
}

std::string ShaderQuadVisualizer::getName() const {
    return "Plasma";
}

void ShaderQuadVisualizer::render(const VisualFrame &frame) {
    if (m_program == 0) {
        return; // shader failed to build → nothing to draw
    }

    const float dt = ImGui::GetIO().DeltaTime;

    // Instantaneous loudness: mean of |mono| over the sample window, with an empirical gain. Raw, this
    // fluctuates every frame and makes the visual flicker on transients, so we don't feed it directly.
    float target = 0.0f;
    if (frame.frameCount > 0 && frame.samples != nullptr) {
        constexpr float GAIN = 4.0f;
        const int channels = std::max(frame.channels, 1);
        float sum = 0.0f;
        for (std::size_t f = 0; f < frame.frameCount; ++f) {
            float mono = 0.0f;
            for (int c = 0; c < channels; ++c) {
                mono += frame.samples[f * channels + c];
            }
            mono /= static_cast<float>(channels);
            sum += std::fabs(mono);
        }
        target = std::clamp(sum / static_cast<float>(frame.frameCount) * GAIN, 0.0f, 1.0f);
    }

    // Envelope-follow the target: quick attack, gentle decay (frame-rate aware). This turns the jittery
    // per-frame loudness into a smooth loudness *contour*, so the visual reacts without blinking. When
    // idle (target 0) it eases back toward 0.
    constexpr float ATTACK_SPEED = 8.0f; // 1/s toward a louder target
    constexpr float DECAY_SPEED = 3.0f;  // 1/s toward a quieter target
    const float rate = std::clamp(dt * (target > m_level ? ATTACK_SPEED : DECAY_SPEED), 0.0f, 1.0f);
    m_level += (target - m_level) * rate;

    // Advance the animation clock at a loudness-paced rate: the plasma visibly churns faster when loud
    // and calms when quiet — the clearest "reacting to the music" cue, and blink-free. Accumulating the
    // rate (not scaling the whole clock) keeps motion smooth with no phase jump. Freeze-when-hidden
    // still holds: render() only runs in VISUALIZATION mode, so the clock stops while hidden.
    constexpr float BASE_SPEED = 0.5f; // idle churn so it's never frozen-looking
    constexpr float SPEED_GAIN = 2.5f; // extra churn at full loudness
    m_time += dt * (BASE_SPEED + SPEED_GAIN * m_level);

    // Convert the reserved rect (ImGui screen coords) to framebuffer pixels, Y-flipped: GL's framebuffer
    // origin is bottom-left, ImGui's is top-left. DisplayFramebufferScale handles HiDPI/retina backing.
    const ImGuiIO &io = ImGui::GetIO();
    const float sx = io.DisplayFramebufferScale.x;
    const float sy = io.DisplayFramebufferScale.y;
    const int fbH = static_cast<int>(io.DisplaySize.y * sy);
    m_vpX = static_cast<int>(frame.x * sx);
    m_vpY = fbH - static_cast<int>((frame.y + frame.h) * sy);
    m_vpW = static_cast<int>(frame.w * sx);
    m_vpH = static_cast<int>(frame.h * sy);

    // Schedule the raw-GL draw into the background draw list (painted behind every window), then queue
    // ImGui's built-in ResetRenderState so the backend restores its own viewport/scissor/program/blend
    // before drawing the following ImGui geometry — our callback deliberately leaves state dirtied.
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    dl->AddCallback(&ShaderQuadVisualizer::glDrawCallback, this);
    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void ShaderQuadVisualizer::glDrawCallback(const ImDrawList * /*parentList*/, const ImDrawCmd *cmd) {
    const auto *self = static_cast<ShaderQuadVisualizer *>(cmd->UserCallbackData);

    // Confine the draw to the reserved rect: viewport places the fullscreen triangle over it, scissor
    // clips anything outside it. Both use the same Y-flipped framebuffer rect captured in render().
    glViewport(self->m_vpX, self->m_vpY, self->m_vpW, self->m_vpH);
    glEnable(GL_SCISSOR_TEST);
    glScissor(self->m_vpX, self->m_vpY, self->m_vpW, self->m_vpH);

    glUseProgram(self->m_program);
    glUniform1f(self->m_locTime, self->m_time);
    glUniform1f(self->m_locLevel, self->m_level);

    glBindVertexArray(self->m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // No state restore here: the queued ImDrawCallback_ResetRenderState makes the ImGui GL3 backend
    // restore its own viewport/scissor/program/blend for the geometry that follows.
}
