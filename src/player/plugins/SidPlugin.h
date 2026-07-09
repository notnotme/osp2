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

#ifndef OSP2_SID_PLUGIN_H
#define OSP2_SID_PLUGIN_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "../PlayerPlugin.h"
#include "../SongLengthDb.h"

// libsidplayfp types, forward-declared to keep its headers out of this one (mirrors OpenMptPlugin).
class sidplayfp;
class ReSIDfpBuilder;
class SidTune;


class SidPlugin final : public PlayerPlugin {
private:
    int m_sampleRate;
    std::vector<std::string> m_extensions;
    // The engine, the ReSIDfp emulation builder it drives, and the currently loaded tune.
    std::unique_ptr<sidplayfp> m_engine;
    std::unique_ptr<ReSIDfpBuilder> m_builder;
    std::unique_ptr<SidTune> m_tune;
    // libsidplayfp's v3 API produces a variable number of samples per play() call; decode() drains
    // this mixed interleaved-stereo scratch before running the emulation for another chunk.
    std::vector<std::int16_t> m_mixBuffer;
    std::size_t m_mixPos;    // frame cursor into m_mixBuffer
    std::size_t m_mixFrames; // valid frames currently held in m_mixBuffer
    // Captured once in open() so getMetadata() never touches the audio-thread-shared engine.
    TrackMetadata m_metadata;
    // Cached in open() so getTitle() never touches the shared engine.
    std::string m_title;
    // Cached settings, applied on open() (a live change would restart the tune). m_model is
    // 0=Auto/1=6581/2=8580; m_clock is 0=Auto/1=PAL/2=NTSC (see getSettings()).
    int m_model;
    int m_clock;
    // HVSC Songlengths time (seconds) for the current subtune, or 0 when the bundled database is
    // absent or does not know the tune (open-ended, as before). Cached in open(), read by
    // getDuration() — never looked up on the audio thread.
    double m_duration;
    // The Songlengths database. Parsing the ~5 MB file takes long enough on the Switch to be a
    // visible stall, so it is loaded once on a background thread started in the constructor rather
    // than on the open() decode path (which would stretch the loading overlay). m_dbReady
    // (release/acquire) publishes the finished map; open() only reads m_songLengths once it is set,
    // so the two threads never touch it concurrently. The destructor joins m_dbLoader. A tune opened
    // before the load finishes (or with the database absent) just gets an open-ended duration.
    SongLengthDb m_songLengths;
    std::thread m_dbLoader;
    std::atomic<bool> m_dbReady;

    // (Re)build the SidConfig from the cached settings and hand it to the engine.
    [[nodiscard]] bool configure();
    // Load the optional C64 KERNAL/BASIC/CHARGEN ROMs from romfs into the engine, if present.
    void loadRoms();

public:
    SidPlugin(const SidPlugin &) = delete;
    SidPlugin &operator=(const SidPlugin &) = delete;
    explicit SidPlugin(int sampleRate);
    ~SidPlugin() override;

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
    void applySetting(const std::string &key, int value) override;
};


#endif //OSP2_SID_PLUGIN_H
