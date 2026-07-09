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


// RAII lifecycle: implementations take the output sample rate as a constructor parameter and
// fully tear down (decoder object, worker threads, library shutdown) in their destructor —
// by convention destructors do their own teardown directly, never via the virtual close(),
// so no destructor depends on dynamic dispatch mid-destruction. Construction and
// destruction happen on the main thread (PlayerController::create()/destroy()); the audio
// device is closed before plugins are destroyed, so destructors never race decode().
class PlayerPlugin {
public:
    PlayerPlugin(const PlayerPlugin &) = delete;
    PlayerPlugin &operator=(const PlayerPlugin &) = delete;
    explicit PlayerPlugin() = default;
    virtual ~PlayerPlugin() = default;

public:
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
    [[nodiscard]] virtual std::vector<PluginSetting> getSettings() const {
        return {};
    }
    // Apply a setting to the live decoder. Called ONLY under PlayerController::m_mutex
    // (may touch the audio-thread-shared decoder object). No-op default for plugins with no config.
    virtual void applySetting(const std::string &key, int value) {}

    // Subtrack support for formats that pack several tunes per file (e.g. GME's NSF/GBS/…).
    // Single-track plugins use these defaults. getSubtrackCount()/getCurrentSubtrack() are
    // cached values (no decoder access); selectSubtrack() is called ONLY under
    // PlayerController::m_mutex, like applySetting (it touches the audio-thread-shared decoder).
    [[nodiscard]] virtual int getSubtrackCount() const {
        return 1;
    }
    [[nodiscard]] virtual int getCurrentSubtrack() const {
        return 0;
    }
    virtual void selectSubtrack(int index) {}
};


#endif //OSP2_PLAYER_PLUGIN_H
