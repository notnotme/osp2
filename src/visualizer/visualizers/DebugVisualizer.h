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

#ifndef OSP2_DEBUG_VISUALIZER_H
#define OSP2_DEBUG_VISUALIZER_H

#include <string>

#include "../VisualFrame.h"
#include "../VisualizerPlugin.h"


// Trivial pipeline-proof plugin (ImGui backend, no GL): fills the reserved rect, borders it, and
// sweeps one animated vertical line. Proves rect placement and per-frame animation; it does not
// consume audio (that is BarsVisualizer's job).
class DebugVisualizer final : public VisualizerPlugin {
private:
    // Animation clock advanced by the per-frame delta only inside render(). Because render() is
    // called only while the visualizer is visible (VISUALIZATION mode), the sweep freezes when
    // hidden and resumes where it left off — it never runs off the always-ticking global clock.
    float m_time = 0.0f;

public:
    void create() override;
    void destroy() override;
    [[nodiscard]] std::string getName() const override;
    void render(const VisualFrame &frame) override;
};


#endif //OSP2_DEBUG_VISUALIZER_H
