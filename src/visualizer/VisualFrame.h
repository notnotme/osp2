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

#ifndef OSP2_VISUAL_FRAME_H
#define OSP2_VISUAL_FRAME_H

#include <cstddef>


/**
 * Per-frame input handed to a VisualizerPlugin: the reserved screen rect plus a read-only view of the most
 * recently decoded audio.
 *
 * Deliberately free of ImGui/GL types so the value can be built outside the presentation backend (Application
 * builds it from the audio tap) without dragging that backend into the audio wiring. samples may be nullptr when
 * frameCount == 0; frameCount == 0 means nothing is playing, so a visual should decay to rest rather than react.
 */
struct VisualFrame {
    float x, y, w, h;       ///< Reserved rect in screen coordinates.
    const float *samples;   ///< Interleaved stereo, frameCount frames (may be nullptr if frameCount == 0).
    std::size_t frameCount; ///< 0 when nothing is playing → the visual should decay to rest.
    int channels;           ///< Interleaved channel count of samples.
    int sampleRate;         ///< Sample rate of samples, in Hz.
};


#endif //OSP2_VISUAL_FRAME_H
