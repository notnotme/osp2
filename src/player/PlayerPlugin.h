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

#ifndef OSP2_PLAYER_PLUGIN_H
#define OSP2_PLAYER_PLUGIN_H

#include <filesystem>
#include <string>
#include <vector>


class PlayerPlugin {
public:
    PlayerPlugin(const PlayerPlugin &) = delete;
    PlayerPlugin &operator=(const PlayerPlugin &) = delete;
    explicit PlayerPlugin() = default;
    virtual ~PlayerPlugin() = default;

public:
    // Called once from PlayerController::create()/destroy(), main thread.
    virtual void create(int sampleRate) = 0;
    virtual void destroy() = 0;

    // Track lifecycle, main thread. Returns false if the file cannot be parsed; must not throw.
    [[nodiscard]] virtual bool open(const std::filesystem::path &path) = 0;
    virtual void close() = 0;

    // Audio thread. Fills `buffer` with `frames` frames of interleaved stereo float samples.
    // Returns the number of frames written; < frames means end of track.
    [[nodiscard]] virtual int decode(float *buffer, int frames) = 0;

    [[nodiscard]] virtual std::string getName() const = 0;
    // Lowercase, without leading dot.
    [[nodiscard]] virtual const std::vector<std::string> &getSupportedExtensions() const = 0;
    // "" when no track is open or the format has no title.
    [[nodiscard]] virtual std::string getTitle() const = 0;
};


#endif //OSP2_PLAYER_PLUGIN_H
