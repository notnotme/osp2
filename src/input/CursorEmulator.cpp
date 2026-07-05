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

#include "CursorEmulator.h"

#include <imgui.h>

#include <algorithm>

namespace {
    // SDL analog axes are signed 16-bit; ignore small resting deflection so the cursor sits still.
    constexpr float kDeadZone = 8000.0f;
    constexpr float kAxisMax = 32767.0f;
    // Cursor travel in pixels per frame at full stick deflection (no speed modifier in 10a).
    constexpr float kBaseSpeed = 12.0f;

    // Maps a raw axis reading to a [-1, 1] deflection with a dead-zone at rest. The magnitude is
    // rescaled so motion starts smoothly from zero at the dead-zone edge rather than jumping.
    float deflection(const Sint16 axis) {
        const float value = static_cast<float>(axis);
        if (value > kDeadZone) {
            return (value - kDeadZone) / (kAxisMax - kDeadZone);
        }
        if (value < -kDeadZone) {
            return (value + kDeadZone) / (kAxisMax - kDeadZone);
        }
        return 0.0f;
    }
}

CursorEmulator::CursorEmulator(const int windowWidth, const int windowHeight)
    : m_pad(SDL_GameControllerOpen(0)),
      m_x(static_cast<float>(windowWidth) / 2.0f),
      m_y(static_cast<float>(windowHeight) / 2.0f),
      m_w(windowWidth),
      m_h(windowHeight) {}

CursorEmulator::~CursorEmulator() {
    if (m_pad) {
        SDL_GameControllerClose(m_pad);
    }
}

void CursorEmulator::update(ImGuiIO &io) {
    if (!m_pad) {
        return;
    }

    // SDL LEFTY is positive when the stick is pushed down and screen Y grows downward, so adding the
    // scaled raw deflection directly moves the cursor in the intuitive direction on both axes.
    m_x += deflection(SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_LEFTX)) * kBaseSpeed;
    m_y += deflection(SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_LEFTY)) * kBaseSpeed;
    m_x = std::clamp(m_x, 0.0f, static_cast<float>(m_w));
    m_y = std::clamp(m_y, 0.0f, static_cast<float>(m_h));
    io.AddMousePosEvent(m_x, m_y);

    // A = left click (button 0), X = right click (button 1). Emit only on edges to mirror the physical
    // button state cleanly, though ImGui tolerates repeated same-state events.
    const bool aDown = SDL_GameControllerGetButton(m_pad, SDL_CONTROLLER_BUTTON_A) != 0;
    const bool xDown = SDL_GameControllerGetButton(m_pad, SDL_CONTROLLER_BUTTON_X) != 0;
    if (aDown != m_aDown) {
        io.AddMouseButtonEvent(0, aDown);
        m_aDown = aDown;
    }
    if (xDown != m_xDown) {
        io.AddMouseButtonEvent(1, xDown);
        m_xDown = xDown;
    }
}
