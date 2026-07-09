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

#ifndef OSP2_GME_PLUGIN_H
#define OSP2_GME_PLUGIN_H

#include <cstdint>
#include <memory>

#include "../PlayerPlugin.h"

struct Music_Emu;


/**
 * Decoder plugin wrapping libgme for classic console/arcade formats (ay, gbs, gym, hes, kss, nsf, nsfe, sap,
 * spc, vgm, vgz).
 *
 * gme_play renders interleaved-stereo 16-bit straight into the output buffer. Many of these formats pack
 * several tunes per file, so this is the plugin that overrides the subtrack virtuals. Quirks:
 * - Duration is a real length only: gme_info_t.length, else intro_length + loop_length, else 0 = unknown.
 *   libgme's play_length is ignored — for a bare header it is a built-in 150000 ms default that would fake a
 *   constant 2:30 on every subsong.
 * - Most GME formats never self-limit, so with loop off startTrack() sets a fade at the track's length to make
 *   the track end and auto-advance work; with loop on, no fade is set and the tune repeats forever.
 * - A sibling ".m3u" playlist, when present, overlays curated per-subtrack titles and lengths onto the emulator
 *   (see overlaySiblingM3u()).
 * - Header strings are transcoded from Shift-JIS to UTF-8 during open() (ASCII tags pass through unchanged).
 */
class GmePlugin final : public PlayerPlugin {
private:
    int m_sampleRate;                      ///< Output sample rate handed to gme_open_data.
    std::vector<std::string> m_extensions; ///< Supported file extensions, fixed at construction.
    /** Custom deleter (&gme_delete, set in the .cpp ctor) so the emulator is torn down via RAII. */
    std::unique_ptr<Music_Emu, void (*)(Music_Emu *)> m_emu;
    /** Captured once in open() so getMetadata() never touches the audio-thread-shared emulator. */
    TrackMetadata m_metadata;
    /** Cached in open() so getTitle() never touches the shared emulator. */
    std::string m_title;
    /** Cached in open() so getDuration() never touches the shared emulator. */
    double m_duration;
    /**
     * Cached subtrack count so getSubtrackCount() never touches the shared emulator; refreshed by startTrack()
     * (mirrors m_title/m_duration/m_metadata).
     */
    int m_trackCount;
    /** Cached 0-based playing subtrack so getCurrentSubtrack() never touches the shared emulator. */
    int m_currentTrack;
    int m_stereoDepth; ///< 0..100 percent (mapped to gme's 0.0..1.0 depth); re-applied to each emulator on open().
    int m_accuracy;    ///< 0/1 (see getSettings()); re-applied to each emulator on open().
    /**
     * Deferred loop flag (0/1): consumed at each startTrack() to disable gme's play-length limit so the track
     * repeats forever. Persists across open/close like m_accuracy; clamped on store.
     */
    int m_loop;

public:
    GmePlugin(const GmePlugin &) = delete;
    GmePlugin &operator=(const GmePlugin &) = delete;
    explicit GmePlugin(int sampleRate);
    ~GmePlugin() override;

public:
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
    /**
     * Applies stereo_depth and accuracy to the live emulator; the loop flag is only stored, consumed at the
     * next startTrack().
     */
    void applySetting(const std::string &key, int value) override;
    [[nodiscard]] int getSubtrackCount() const override;
    [[nodiscard]] int getCurrentSubtrack() const override;
    /**
     * Clamps index to [0, count) and starts it via startTrack(); on failure the current subtrack (and the
     * emulator) is kept, since the file is still valid.
     */
    void selectSubtrack(int index) override;

private:
    /**
     * Overlays a sibling ".m3u" playlist onto the freshly-opened emulator so libgme reports the curated
     * per-subtrack titles/lengths the tune's own header lacks.
     *
     * Handles both real-world layouts (a combined "<stem>.m3u", or several per-track ".m3u" files each
     * referencing the tune by filename). Strictly best-effort: any failure is logged at most, never propagated
     * or thrown.
     */
    void overlaySiblingM3u(const std::filesystem::path &tunePath);

    /**
     * Starts subtrack `index` on the already-open emulator.
     *
     * Re-applies stereo depth (start_track can reset effects), re-reads gme_track_info and rebuilds the cached
     * title/duration/metadata, and sets m_currentTrack.
     * @return false (leaving the caches untouched) if libgme reports an error
     */
    [[nodiscard]] bool startTrack(int index);
};


#endif //OSP2_GME_PLUGIN_H
