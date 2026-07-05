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

#include "DebugVisualizer.h"

#include <imgui.h>

#include <cmath>


// ImGui-only plugin: nothing to allocate or free.
void DebugVisualizer::create() {}
void DebugVisualizer::destroy() {}

std::string DebugVisualizer::getName() const {
    return "Debug";
}

void DebugVisualizer::render(const VisualFrame &frame) {
    // frame.samples is intentionally unused here — this debug plugin only proves rect placement and
    // per-frame animation. Consuming the waveform is BarsVisualizer's job (chunk 8c).

    // Runs inside the ImGui frame; the background draw list paints behind every window, directly on
    // the reserved rect in screen coordinates.
    ImDrawList *draw = ImGui::GetBackgroundDrawList();
    const ImVec2 topLeft(frame.x, frame.y);
    const ImVec2 bottomRight(frame.x + frame.w, frame.y + frame.h);

    // Subtle translucent fill + a 1px border make the reserved rect visible and prove its placement.
    draw->AddRectFilled(topLeft, bottomRight, IM_COL32(30, 40, 80, 64));
    draw->AddRect(topLeft, bottomRight, IM_COL32(120, 140, 200, 200));

    // One vertical line sweeping left↔right over the rect's full height — proves per-frame animation.
    // Advance our own clock by this frame's delta (not the global ImGui::GetTime()) so the sweep only
    // moves while this plugin is actually rendered, i.e. it stops when the visualizer isn't visible.
    m_time += ImGui::GetIO().DeltaTime;
    const float lineX = frame.x + (0.5f + 0.5f * std::sin(m_time * 2.0f)) * frame.w;
    draw->AddLine(ImVec2(lineX, frame.y), ImVec2(lineX, frame.y + frame.h),
                  IM_COL32(120, 220, 255, 255), 2.0f);
}
