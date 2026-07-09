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

/**
 * libsc68's opaque instance type, forward-declared to keep <sc68/sc68.h> out of this header (mirrors
 * SidPlugin).
 *
 * It is a C API, so the typedef must carry C linkage-compatible naming.
 */
typedef struct _sc68_s sc68_t;


/**
 * Decoder plugin wrapping libsc68 for Atari ST (YM-2149/STE) and Amiga (Paula) formats (sc68, sndh, snd).
 *
 * The constructor runs the global sc68_init() (paired with exactly one sc68_shutdown() in the destructor via
 * m_initialized; a failure leaves m_sc68 null and every open() fails defensively) and forces signed-16-bit
 * stereo PCM so sc68_process() writes straight into decode()'s buffer — no conversion. open() reads the whole
 * file into memory for sc68_load_mem() and starts the disk's default track only. Captured strings are
 * transcoded from Latin-1 to UTF-8 during open(); metadata is copied out of the disk info immediately, because
 * those pointers are only valid until sc68_close(). Both settings (asid, loop) apply on the next open().
 */
class Sc68Plugin final : public PlayerPlugin {
private:
    int m_sampleRate;                      ///< Output sample rate the instance is created at.
    std::vector<std::string> m_extensions; ///< Supported file extensions, fixed at construction.
    sc68_t *m_sc68 = nullptr; ///< The libsc68 instance; holds the currently loaded disk between open() and close().
    /** True once sc68_init() succeeded in the constructor, so the destructor only pairs a shutdown with it. */
    bool m_initialized = false;
    /** Captured once in open() so getMetadata() never touches the audio-thread-shared instance. */
    TrackMetadata m_metadata;
    /** Cached in open() so getTitle() never touches the shared instance. */
    std::string m_title;
    double m_duration = 0.0; ///< Track length in seconds, from music_info time_ms; 0 = unknown.
    /** Stereo frames produced so far, accumulated in decode() to derive getPosition(). */
    std::uint64_t m_playedFrames = 0;
    bool m_ended = false; ///< Set once sc68_process() reports SC68_END (or an error), so decode() stops feeding.
    /**
     * aSIDifier mode (0=off, 1=on, 2=force), applied on the next open() and clamped on store. Maps directly
     * onto SC68_ASID_OFF/ON/FORCE (see getSettings()).
     */
    int m_asid = 0;
    /**
     * Deferred loop flag (0/1): passed to sc68_play() in open(), so it takes effect on the next open() (like
     * m_asid). 1 -> SC68_INF_LOOP (infinite), 0 -> SC68_DEF_LOOP (default).
     */
    int m_loop = 0;

public:
    Sc68Plugin(const Sc68Plugin &) = delete;
    Sc68Plugin &operator=(const Sc68Plugin &) = delete;
    explicit Sc68Plugin(int sampleRate);
    ~Sc68Plugin() override;

public:
    [[nodiscard]] bool open(const std::filesystem::path &path) override;
    void close() override;
    [[nodiscard]] int decode(std::int16_t *buffer, int frames) override;
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] const std::vector<std::string> &getSupportedExtensions() const override;
    [[nodiscard]] std::string getTitle() const override;
    /** Derives the position from the accumulated decoded-frame counter — sc68 has no cheap position query. */
    [[nodiscard]] double getPosition() const override;
    [[nodiscard]] double getDuration() const override;
    [[nodiscard]] TrackMetadata getMetadata() const override;
    [[nodiscard]] std::vector<PluginSetting> getSettings() const override;
    /** Stores the clamped value only — both settings are consumed by the next open(). */
    void applySetting(const std::string &key, int value) override;
};


#endif //OSP2_SC68_PLUGIN_H
