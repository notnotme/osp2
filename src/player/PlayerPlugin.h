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


/**
 * Decoder plugin interface: one implementation per decoder library, selected by file extension.
 *
 * RAII lifecycle: implementations take the output sample rate as a constructor parameter and fully tear down
 * (decoder object, worker threads, library shutdown) in their destructor — by convention destructors do their own
 * teardown directly, never via the virtual close(), so no destructor depends on dynamic dispatch mid-destruction.
 * Construction and destruction happen on the main thread (PlayerController::create()/destroy()); the audio device
 * is closed before plugins are destroyed, so destructors never race decode().
 */
class PlayerPlugin {
public:
    PlayerPlugin(const PlayerPlugin &) = delete;
    PlayerPlugin &operator=(const PlayerPlugin &) = delete;
    explicit PlayerPlugin() = default;
    virtual ~PlayerPlugin() = default;

public:
    /**
     * Opens the track at path, parsing the module and caching title/metadata.
     *
     * PlayerController runs it on its load worker, off m_mutex — safe because the plugin being opened is never the
     * active one, so the audio thread cannot touch it. Never called on the audio thread.
     * @return false if the file cannot be parsed; must not throw
     */
    [[nodiscard]] virtual bool open(const std::filesystem::path &path) = 0;
    /**
     * Closes the open track and clears the cached metadata to monostate.
     *
     * Main thread only — track teardown never happens on the audio thread.
     */
    virtual void close() = 0;

    /**
     * Fills `buffer` with `frames` frames of interleaved stereo signed-16-bit (int16) samples.
     *
     * Called on the SDL audio thread under PlayerController::m_mutex.
     * @param buffer destination, interleaved stereo
     * @param frames number of frames to render
     * @return the number of frames written; fewer than `frames` means end of track
     */
    [[nodiscard]] virtual int decode(std::int16_t *buffer, int frames) = 0;

    /** Plugin display name; also the key PlayerController::applyPluginSetting() matches against. */
    [[nodiscard]] virtual std::string getName() const = 0;
    /** Supported file extensions, lowercase, without leading dot. */
    [[nodiscard]] virtual const std::vector<std::string> &getSupportedExtensions() const = 0;
    /** Track title; "" when no track is open or the format has no title. */
    [[nodiscard]] virtual std::string getTitle() const = 0;
    /**
     * Playback position in seconds.
     *
     * Called ONLY under PlayerController::m_mutex — the decoder object is shared with the audio thread.
     */
    [[nodiscard]] virtual double getPosition() const = 0;
    /**
     * Total duration in seconds; 0 means unknown.
     *
     * Called ONLY under PlayerController::m_mutex — the decoder object is shared with the audio thread.
     */
    [[nodiscard]] virtual double getDuration() const = 0;
    /**
     * Per-plugin track metadata; monostate when no track is open.
     *
     * Returns a value CACHED during open() — must not read the decoder object, which is shared with the audio
     * thread. close() clears it to monostate. Main thread only.
     */
    [[nodiscard]] virtual TrackMetadata getMetadata() const = 0;

    /**
     * Setting descriptors for the UI.
     *
     * Plain cached values, main thread, no decoder access. Plugins with no config return {} (the default).
     */
    [[nodiscard]] virtual std::vector<PluginSetting> getSettings() const {
        return {};
    }
    /**
     * Applies a setting to the live decoder.
     *
     * Called ONLY under PlayerController::m_mutex (may touch the audio-thread-shared decoder object).
     * No-op default for plugins with no config.
     */
    virtual void applySetting(const std::string &key, int value) {}

    /**
     * Number of subtracks, for formats that pack several tunes per file (e.g. GME's NSF/GBS/…).
     *
     * A cached value (no decoder access). Single-track plugins use the default of 1.
     */
    [[nodiscard]] virtual int getSubtrackCount() const {
        return 1;
    }
    /**
     * 0-based index of the playing subtrack.
     *
     * A cached value (no decoder access). Single-track plugins use the default of 0.
     */
    [[nodiscard]] virtual int getCurrentSubtrack() const {
        return 0;
    }
    /**
     * Starts the subtrack at index.
     *
     * Called ONLY under PlayerController::m_mutex, like applySetting (it touches the audio-thread-shared decoder).
     * No-op default for single-track plugins.
     */
    virtual void selectSubtrack(int index) {}
};


#endif //OSP2_PLAYER_PLUGIN_H
