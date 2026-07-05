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

#ifndef OSP2_CURSOREMULATOR_H
#define OSP2_CURSOREMULATOR_H

#include <SDL.h>

struct ImGuiIO;

// Drives the ImGui mouse cursor from an SDL_GameController: the left stick moves a virtual cursor,
// the right stick scrolls, A = left click, X = right click, and the L / R shoulders are held-only
// speed modifiers on both movement and scroll (L = slow/precise, R = fast). Platform layer —
// main-thread-only, called once per frame from the render loop between ImGui_ImplSDL2_NewFrame() and
// ImGui::NewFrame() (the ImGui IO injection point).
class CursorEmulator final {
public:
    CursorEmulator(int windowWidth, int windowHeight);
    ~CursorEmulator();
    CursorEmulator(const CursorEmulator &) = delete;
    CursorEmulator &operator=(const CursorEmulator &) = delete;

    // Reads the pad, integrates the left-stick deflection into the cursor position (scaled by the held
    // L/R speed modifier), and injects the cursor position, button state, and right-stick scroll into
    // ImGui via io.AddMouse*Event.
    void update(ImGuiIO &io);

private:
    SDL_GameController *m_pad;
    float m_x, m_y;
    int m_w, m_h;
    bool m_leftDown = false;
    bool m_rightDown = false;
};

#endif //OSP2_CURSOREMULATOR_H
