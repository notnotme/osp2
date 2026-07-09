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

#ifndef OSP2_BARS_VISUALIZER_H
#define OSP2_BARS_VISUALIZER_H

#include <array>
#include <string>

#include "../VisualFrame.h"
#include "../VisualizerPlugin.h"


// The basic plugin (chunk 8c, ImGui backend, no GL): N vertical bars whose heights track per-band
// audio amplitude. Bands come from bucketing the sample window and taking the time-domain peak per
// bucket (no FFT → no dependency, Switch-safe). Bar heights are smoothed with a fast attack / gentle
// decay and persisted across frames in m_levels, so transients pop and the bars fall smoothly to
// rest when nothing plays (frameCount == 0). An FFT spectrum and a GL shader-quad are follow-ons.
class BarsVisualizer final : public VisualizerPlugin {
private:
    static constexpr int BAR_COUNT = 64;

    // Smoothed bar heights in [0, 1], persisted across frames for attack/decay smoothing. Advanced
    // only inside render() from ImGui::GetIO().DeltaTime, so the decay freezes when the visualizer
    // is hidden and resumes where it left off (see docs/visualization.md "Animation freezes ...").
    std::array<float, BAR_COUNT> m_levels{};

public:
    [[nodiscard]] std::string getName() const override;
    void render(const VisualFrame &frame) override;
};


#endif //OSP2_BARS_VISUALIZER_H
