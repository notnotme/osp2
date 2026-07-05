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

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Metadata.h"
#include "PluginSetting.h"


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

    // Audio thread. Fills `buffer` with `frames` frames of interleaved stereo signed-16-bit
    // (int16) samples. Returns the number of frames written; < frames means end of track.
    [[nodiscard]] virtual int decode(std::int16_t *buffer, int frames) = 0;

    [[nodiscard]] virtual std::string getName() const = 0;
    // Lowercase, without leading dot.
    [[nodiscard]] virtual const std::vector<std::string> &getSupportedExtensions() const = 0;
    // "" when no track is open or the format has no title.
    [[nodiscard]] virtual std::string getTitle() const = 0;
    // Playback position and total duration in seconds. Called ONLY under
    // PlayerController::m_mutex — the decoder object is shared with the audio thread.
    [[nodiscard]] virtual double getPosition() const = 0;
    [[nodiscard]] virtual double getDuration() const = 0;
    // Returns a value CACHED during open() — must not read the decoder object, which is shared
    // with the audio thread. close() clears it to monostate. Main thread only.
    [[nodiscard]] virtual TrackMetadata getMetadata() const = 0;

    // Setting descriptors for the UI. Plain cached values, main thread, no decoder access.
    // Plugins with no config return {} (the default).
    [[nodiscard]] virtual std::vector<PluginSetting> getSettings() const { return {}; }
    // Apply a setting to the live decoder. Called ONLY under PlayerController::m_mutex
    // (may touch the audio-thread-shared decoder object). No-op default for plugins with no config.
    virtual void applySetting(const std::string &key, int value) {}
};


#endif //OSP2_PLAYER_PLUGIN_H
