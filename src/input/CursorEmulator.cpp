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
    // Cursor travel in pixels per frame at full stick deflection (before the held L/R speed modifier).
    constexpr float kBaseSpeed = 12.0f;
    // Held-shoulder speed multipliers applied to both cursor and scroll: L = slow/precise, R = fast.
    // If both are held, slow wins (precision is the safer default). Neither held = 1.0f.
    constexpr float kSlowMul = 0.35f;
    constexpr float kFastMul = 2.5f;
    // Mouse-wheel units per frame at full right-stick deflection. ImGui scrolls a fixed number of items
    // per wheel unit, so this stays small; may need tuning against real Switch hardware feel.
    constexpr float kScrollSpeed = 0.5f;
    // A trigger (ZL/ZR) counts as "held" past this point of its 0..32767 travel. On the Switch the
    // triggers are digital (0 or full), so any positive threshold works; a mid value suits analog pads.
    constexpr Sint16 kTriggerThreshold = 8000;

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
} // namespace

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

    // Held L/R shoulder OR trigger on a side modulates both cursor and scroll speed — either input on
    // that side works, whichever is easier to reach. The physical L/R shoulders and triggers are NOT
    // rotated by SDL's position naming the way the face buttons are, so they map to the physical
    // left/right side directly on both platforms — no #ifdef needed. Both sides held: slow wins.
    const bool slowHeld = SDL_GameControllerGetButton(m_pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0 ||
        SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > kTriggerThreshold;
    const bool fastHeld = SDL_GameControllerGetButton(m_pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0 ||
        SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > kTriggerThreshold;
    const float speedMul = slowHeld ? kSlowMul : (fastHeld ? kFastMul : 1.0f);

    // SDL LEFTY is positive when the stick is pushed down and screen Y grows downward, so adding the
    // scaled raw deflection directly moves the cursor in the intuitive direction on both axes.
    m_x += deflection(SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_LEFTX)) * kBaseSpeed * speedMul;
    m_y += deflection(SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_LEFTY)) * kBaseSpeed * speedMul;
    m_x = std::clamp(m_x, 0.0f, static_cast<float>(m_w));
    m_y = std::clamp(m_y, 0.0f, static_cast<float>(m_h));
    io.AddMousePosEvent(m_x, m_y);

    // Right stick scrolls. ImGui's AddMouseWheelEvent(wheel_x, wheel_y) treats positive wheel_y as
    // scroll-up and (counter-intuitively) positive wheel_x as scroll-LEFT, so both axes are negated to
    // map an up/right stick push to an up/right scroll. The held L/R speed modifier scales scroll too.
    // Gate on real deflection so we don't emit a zero wheel event every frame at rest.
    const float wheelX =
        -deflection(SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_RIGHTX)) * kScrollSpeed * speedMul;
    const float wheelY =
        -deflection(SDL_GameControllerGetAxis(m_pad, SDL_CONTROLLER_AXIS_RIGHTY)) * kScrollSpeed * speedMul;
    if (wheelX != 0.0f || wheelY != 0.0f) {
        io.AddMouseWheelEvent(wheelX, wheelY);
    }

    // Physical A = left click (button 0), physical X = right click (button 1). SDL names buttons by
    // Xbox *position*, but the Switch's labels are rotated: its physical A sits at the east position
    // (SDL_..._B) and physical X at the north position (SDL_..._Y). Map to the labels so the Switch's
    // usual confirm button (A) left-clicks. Emit only on edges to mirror the physical state cleanly.
#if defined(__SWITCH__)
    constexpr auto leftClickButton = SDL_CONTROLLER_BUTTON_B;
    constexpr auto rightClickButton = SDL_CONTROLLER_BUTTON_Y;
#else
    constexpr auto leftClickButton = SDL_CONTROLLER_BUTTON_A;
    constexpr auto rightClickButton = SDL_CONTROLLER_BUTTON_X;
#endif
    const bool leftDown = SDL_GameControllerGetButton(m_pad, leftClickButton) != 0;
    const bool rightDown = SDL_GameControllerGetButton(m_pad, rightClickButton) != 0;
    if (leftDown != m_leftDown) {
        io.AddMouseButtonEvent(0, leftDown);
        m_leftDown = leftDown;
    }
    if (rightDown != m_rightDown) {
        io.AddMouseButtonEvent(1, rightDown);
        m_rightDown = rightDown;
    }
}
