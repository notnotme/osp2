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

#include "BarsVisualizer.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>


// ImGui-only plugin: nothing to allocate or free.
void BarsVisualizer::create() {}
void BarsVisualizer::destroy() {}

std::string BarsVisualizer::getName() const {
    return "Bars";
}

void BarsVisualizer::render(const VisualFrame &frame) {
    // (a) Per-bar target amplitude, time-domain (no FFT). Bucket the frameCount frames into BAR_COUNT
    // contiguous buckets and take the peak of |mono| per bucket — peak, not RMS, so transients pop.
    // GAIN is an empirical visual boost so quiet modules aren't all stubs; kept modest so hotter,
    // near-full-scale content (e.g. GME chiptunes) doesn't slam every bar to the ceiling. When
    // frameCount == 0 (nothing playing) every target stays 0, driving the decay to rest.
    constexpr float GAIN = 1.15f;
    std::array<float, BAR_COUNT> targets{};
    if (frame.frameCount > 0 && frame.samples != nullptr) {
        const int channels = std::max(frame.channels, 1);
        for (int b = 0; b < BAR_COUNT; ++b) {
            const std::size_t begin = frame.frameCount * static_cast<std::size_t>(b) / BAR_COUNT;
            const std::size_t end = frame.frameCount * static_cast<std::size_t>(b + 1) / BAR_COUNT;
            float peak = 0.0f;
            for (std::size_t f = begin; f < end; ++f) {
                float mono = 0.0f;
                for (int c = 0; c < channels; ++c) {
                    mono += frame.samples[f * channels + c];
                }
                mono /= static_cast<float>(channels);
                peak = std::max(peak, std::fabs(mono));
            }
            targets[b] = std::min(peak * GAIN, 1.0f);
        }
    }

    // (b) Attack/decay smoothing, per bar, frame-rate aware. dt comes from the render delta (not
    // ImGui::GetTime()) so the smoothing only advances while this plugin is actually rendered — the
    // documented "freeze when hidden" contract. Fast attack = snappy rise on transients; gentle decay
    // = smooth fall to rest on stop/pause.
    constexpr float ATTACK_SPEED = 40.0f; // 1/s toward a rising target
    constexpr float DECAY_SPEED = 5.0f;   // 1/s toward a falling target
    const float dt = ImGui::GetIO().DeltaTime;
    for (int b = 0; b < BAR_COUNT; ++b) {
        const float speed = targets[b] > m_levels[b] ? ATTACK_SPEED : DECAY_SPEED;
        const float rate = std::clamp(dt * speed, 0.0f, 1.0f);
        m_levels[b] += (targets[b] - m_levels[b]) * rate;
        // Geometric decay only asymptotes toward 0; snap sub-pixel values to a true rest so the bars
        // stop entirely (and the zero-height skip below actually fires) instead of trailing denormals.
        if (m_levels[b] < 1e-4f) {
            m_levels[b] = 0.0f;
        }
    }

    // (c) Draw into the reserved rect. Runs inside the ImGui frame; the background draw list paints
    // behind every window, directly on the reserved rect in screen coordinates. Bars grow bottom→up
    // from the baseline. The accent color matches the player bar. GAP is the space *between* bars, so
    // the row tiles flush on both edges (BAR_COUNT bars + BAR_COUNT-1 gaps span exactly frame.w) —
    // no black margin against the right window border.
    constexpr float GAP = 2.0f;
    const float barW = (frame.w - GAP * (BAR_COUNT - 1)) / BAR_COUNT;
    if (barW <= 0.0f) {
        return; // reserved rect too narrow for 64 bars; nothing sensible to draw
    }
    ImDrawList *draw = ImGui::GetBackgroundDrawList();
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    const float baseline = frame.y + frame.h;
    for (int b = 0; b < BAR_COUNT; ++b) {
        const float height = std::clamp(m_levels[b], 0.0f, 1.0f) * frame.h;
        if (height <= 0.0f) {
            continue;
        }
        const float left = frame.x + static_cast<float>(b) * (barW + GAP);
        const float right = std::min(left + barW, frame.x + frame.w);
        draw->AddRectFilled(ImVec2(left, baseline - height), ImVec2(right, baseline), color);
    }
}
