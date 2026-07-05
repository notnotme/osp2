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

#ifndef OSP2_SC68_PLUGIN_H
#define OSP2_SC68_PLUGIN_H

#include <cstdint>
#include <vector>

#include "../PlayerPlugin.h"

// libsc68's opaque instance type, forward-declared to keep <sc68/sc68.h> out of this header
// (mirrors SidPlugin). It is a C API, so the typedef must carry C linkage-compatible naming.
typedef struct _sc68_s sc68_t;


class Sc68Plugin final : public PlayerPlugin {
private:
    int m_sampleRate;
    std::vector<std::string> m_extensions;
    // The libsc68 instance; holds the currently loaded disk between open() and close().
    sc68_t *m_sc68 = nullptr;
    // True once sc68_init() succeeded in create(), so destroy() only pairs a shutdown with it.
    bool m_initialized = false;
    // Captured once in open() so getMetadata() never touches the audio-thread-shared instance.
    TrackMetadata m_metadata;
    // Cached in open() so getTitle() never touches the shared instance.
    std::string m_title;
    // Track length in seconds, from music_info time_ms; 0 = unknown.
    double m_duration = 0.0;
    // Stereo frames produced so far, accumulated in decode() to derive getPosition().
    std::uint64_t m_playedFrames = 0;
    // Set once sc68_process() reports SC68_END (or an error), so decode() stops feeding.
    bool m_ended = false;
    // aSIDifier mode (0=off, 1=on, 2=force), applied on the next open() and clamped on store.
    // Maps directly onto SC68_ASID_OFF/ON/FORCE (see getSettings()).
    int m_asid = 0;

public:
    Sc68Plugin(const Sc68Plugin &) = delete;
    Sc68Plugin &operator=(const Sc68Plugin &) = delete;
    explicit Sc68Plugin();
    ~Sc68Plugin() override;

public:
    void create(int sampleRate) override;
    void destroy() override;
    [[nodiscard]] bool open(const std::filesystem::path &path) override;
    void close() override;
    [[nodiscard]] int decode(std::int16_t *buffer, int frames) override;
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] const std::vector<std::string> &getSupportedExtensions() const override;
    [[nodiscard]] std::string getTitle() const override;
    [[nodiscard]] double getPosition() const override;
    [[nodiscard]] double getDuration() const override;
    [[nodiscard]] TrackMetadata getMetadata() const override;
    [[nodiscard]] std::vector<PluginSetting> getSettings() const override;
    void applySetting(const std::string &key, int value) override;
};


#endif //OSP2_SC68_PLUGIN_H
