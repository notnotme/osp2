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


class GmePlugin final : public PlayerPlugin {
private:
    int m_sampleRate;
    std::vector<std::string> m_extensions;
    // Custom deleter (&gme_delete, set in the .cpp ctor) so the emulator is torn down via RAII.
    std::unique_ptr<Music_Emu, void (*)(Music_Emu *)> m_emu;
    // Captured once in open() so getMetadata() never touches the audio-thread-shared emulator.
    TrackMetadata m_metadata;
    // Cached in open() so getTitle()/getDuration() never touch the shared emulator.
    std::string m_title;
    double m_duration;
    // Cached subtrack navigation state so getSubtrackCount()/getCurrentSubtrack() never touch the
    // shared emulator; refreshed by startTrack() (mirrors m_title/m_duration/m_metadata).
    int m_trackCount;
    int m_currentTrack;
    // Cached render settings, re-applied to each emulator on open(). m_stereoDepth is a 0..100
    // percent (mapped to gme's 0.0..1.0 depth); m_accuracy is 0/1 (see getSettings()).
    int m_stereoDepth;
    int m_accuracy;
    // Deferred loop flag (0/1): consumed at each startTrack() to disable gme's play-length limit so
    // the track repeats forever. Persists across open/close like m_accuracy; clamped on store.
    int m_loop;

public:
    GmePlugin(const GmePlugin &) = delete;
    GmePlugin &operator=(const GmePlugin &) = delete;
    explicit GmePlugin();
    ~GmePlugin() override;

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
    [[nodiscard]] int getSubtrackCount() const override;
    [[nodiscard]] int getCurrentSubtrack() const override;
    void selectSubtrack(int index) override;

private:
    // Starts subtrack `index` on the already-open emulator: re-applies stereo depth (start_track can
    // reset effects), re-reads gme_track_info and rebuilds the cached title/duration/metadata, and
    // sets m_currentTrack. Returns false (leaving the caches untouched) if libgme reports an error.
    [[nodiscard]] bool startTrack(int index);
};


#endif //OSP2_GME_PLUGIN_H
