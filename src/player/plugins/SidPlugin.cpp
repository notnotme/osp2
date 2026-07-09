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

#include "SidPlugin.h"

#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidConfig.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/builders/residfp.h>
#include <SDL_log.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iterator>
#include <cstring>

#include "../../Paths.h"
#include "../Charset.h"
#include "PluginUtil.h"


namespace {
    // Interleaved stereo — libsidplayfp is initialized with initMixer(true).
    constexpr int CHANNELS = 2;

    // CPU cycles emulated per play() step (~10ms at the C64's ~1MHz clock). One step yields a
    // variable, bounded number of samples that decode() buffers in m_mixBuffer.
    constexpr unsigned int PLAY_CYCLES = 10000;

    std::string sidModelString(const SidTuneInfo::model_t model) {
        switch (model) {
        case SidTuneInfo::SIDMODEL_6581:
            return "MOS 6581";
        case SidTuneInfo::SIDMODEL_8580:
            return "MOS 8580";
        case SidTuneInfo::SIDMODEL_ANY:
            return "Any";
        default:
            return {}; // SIDMODEL_UNKNOWN → empty, so the metadata row is skipped
        }
    }

    std::string clockString(const SidTuneInfo::clock_t clock) {
        switch (clock) {
        case SidTuneInfo::CLOCK_PAL:
            return "PAL";
        case SidTuneInfo::CLOCK_NTSC:
            return "NTSC";
        case SidTuneInfo::CLOCK_ANY:
            return "PAL/NTSC";
        default:
            return {}; // CLOCK_UNKNOWN → empty
        }
    }

    // Reads a whole ROM file into a byte vector, or returns empty if it can't be opened or has the
    // wrong size. C64 ROMs are fixed-size; a size mismatch means a wrong/corrupt file, so skip it.
    std::vector<std::uint8_t> loadRom(const std::filesystem::path &path, const std::size_t expectedSize) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return {}; // absent ROM is expected (they are optional) — no log noise
        }
        std::vector<std::uint8_t> data{std::istreambuf_iterator(file), std::istreambuf_iterator<char>()};
        if (data.size() != expectedSize) {
            SDL_Log(
                "SidPlugin: ignoring ROM %s (expected %zu bytes, got %zu)",
                path.string().c_str(),
                expectedSize,
                data.size()
            );
            return {};
        }
        return data;
    }
} // namespace

SidPlugin::SidPlugin(const int sampleRate)
    : m_sampleRate(sampleRate),
      m_extensions{"sid", "psid", "rsid"},
      m_engine(std::make_unique<sidplayfp>()),
      m_builder(std::make_unique<ReSIDfpBuilder>("ReSIDfp")),
      m_mixPos(0),
      m_mixFrames(0),
      m_model(0),
      m_clock(0),
      m_duration(0.0),
      m_dbReady(false) {
    loadRoms();

    // Parse the ~5 MB Songlengths database off the decode path so it never stalls the first SID's
    // open() (and stretches the loading overlay). It starts here, at startup, and typically finishes
    // long before the user browses to and picks a tune; a tune opened before it is ready simply
    // shows an open-ended duration. The destructor joins this thread; open() reads the map only
    // once m_dbReady. Started last so no later constructor step can throw past a joinable thread.
    m_dbLoader = std::thread([this] {
        if (m_songLengths.load(assetPath("sidlength/Songlengths.md5"))) {
            SDL_Log("SidPlugin: loaded Songlengths database (%zu tunes) — SID durations enabled", m_songLengths.size());
        }
        m_dbReady.store(true, std::memory_order_release);
    });
}

SidPlugin::~SidPlugin() {
    // Join the background database loader before tearing down (a still-joinable std::thread would
    // std::terminate at destruction); it only touches m_songLengths, so this is safe any time.
    if (m_dbLoader.joinable()) {
        m_dbLoader.join();
    }
    // Reset the engine first: it holds locked SID emulations owned by the builder. This also
    // unloads any tune still open.
    m_engine.reset();
    m_builder.reset();
    m_tune.reset();
}

void SidPlugin::loadRoms() {
    // Optional C64 KERNAL/BASIC/CHARGEN images under romfs/roms. They are copyrighted Commodore
    // firmware, not shipped in the repo (.gitignore) — baked into the .nro only if the user supplies
    // them. Without any ROM, PSID tunes still play; RSID tunes that boot like a real C64 need at
    // least the KERNAL. setRoms() copies the data internally, so these buffers can go out of scope
    // immediately (as libsidplayfp's own demo does). Loaded once — the ROMs are engine-global.
    const std::vector<std::uint8_t> kernal = loadRom(assetPath("roms/kernal-901227-03.bin"), 8192);
    const std::vector<std::uint8_t> basic = loadRom(assetPath("roms/basic-901226-01.bin"), 8192);
    const std::vector<std::uint8_t> chargen = loadRom(assetPath("roms/chargen-901225-01.bin"), 4096);
    m_engine->setRoms(
        kernal.empty() ? nullptr : kernal.data(),
        basic.empty() ? nullptr : basic.data(),
        chargen.empty() ? nullptr : chargen.data()
    );
    if (!kernal.empty()) {
        SDL_Log("SidPlugin: loaded C64 KERNAL ROM — RSID tunes enabled");
    }
}

bool SidPlugin::configure() {
    // A default-constructed SidConfig carries sane defaults; we override only what we drive.
    SidConfig cfg;
    cfg.frequency = static_cast<uint_least32_t>(m_sampleRate);
    cfg.samplingMethod = SidConfig::INTERPOLATE; // cheaper than resampling — kinder to the Switch
    cfg.sidEmulation = m_builder.get();
    if (m_model != 0) {
        cfg.forceSidModel = true;
        cfg.defaultSidModel = m_model == 2 ? SidConfig::MOS8580 : SidConfig::MOS6581;
    }
    if (m_clock != 0) {
        cfg.forceC64Model = true;
        cfg.defaultC64Model = m_clock == 2 ? SidConfig::NTSC : SidConfig::PAL;
    }
    if (!m_engine->config(cfg)) {
        SDL_Log("SidPlugin: engine config failed: %s", m_engine->error());
        return false;
    }
    return true;
}

bool SidPlugin::open(const std::filesystem::path &path) {
    // We read the file ourselves (rather than SidTune's own path loader) to match GmePlugin and to
    // sidestep path-handling quirks on the Switch; the file read can throw (bad_alloc), so honor
    // PlayerPlugin's "must not throw" contract. No external ROMs are set — PSID tunes need none.
    m_duration = 0.0; // open-ended until the Songlengths database resolves a time below

    try {
        const auto data = readFileBytes(path);
        if (!data) {
            SDL_Log("SidPlugin: cannot open %s", path.c_str());
            return false;
        }

        auto tune = std::make_unique<SidTune>(
            reinterpret_cast<const uint_least8_t *>(data->data()), static_cast<uint_least32_t>(data->size())
        );
        if (!tune->getStatus()) {
            SDL_Log("SidPlugin: cannot parse %s: %s", path.c_str(), tune->statusString());
            return false;
        }
        tune->selectSong(0); // 0 = the tune's own default starting song
        m_tune = std::move(tune);

        // Order per libsidplayfp: config the engine, load the tune, then initialize the mixer.
        if (!configure() || !m_engine->load(m_tune.get())) {
            if (m_engine->error()[0] != '\0') {
                SDL_Log("SidPlugin: engine load failed: %s", m_engine->error());
            }
            m_engine->load(nullptr);
            m_tune.reset();
            return false;
        }
        m_engine->initMixer(true);

        // Size the mix scratch once here (after initMixer) so decode() never allocates on the audio
        // thread; one play(PLAY_CYCLES) step never yields more than getBufSize() shorts. NB: despite
        // its header saying "bytes", getBufSize() returns a short-element count in v3 (the demo sizes
        // a std::vector<short> with it); decode()'s resize is the safety net should that ever change.
        const int bufSize = m_engine->getBufSize(PLAY_CYCLES);
        m_mixBuffer.assign(bufSize > 0 ? static_cast<std::size_t>(bufSize) : 0, 0);
        m_mixPos = 0;
        m_mixFrames = 0;

        SidMetadata metadata;
        if (const SidTuneInfo *info = m_tune->getInfo(); info != nullptr) {
            const unsigned int strings = info->numberOfInfoStrings();
            // PSID/STIL info strings are Latin-1; transcode to UTF-8 for Dear ImGui. sidModel/clock
            // below are library-generated ASCII and need no transcoding.
            if (strings > 0) {
                metadata.title = toUtf8(toString(info->infoString(0)), Charset::Latin1);
            }
            if (strings > 1) {
                metadata.author = toUtf8(toString(info->infoString(1)), Charset::Latin1);
            }
            if (strings > 2) {
                metadata.released = toUtf8(toString(info->infoString(2)), Charset::Latin1);
            }
            metadata.sidModel = sidModelString(info->sidModel(0));
            metadata.clock = clockString(info->clockSpeed());

            // Resolve the tune's real length from the Songlengths database. createMD5New() writes a
            // 32-char lowercase-hex MD5 + NUL that matches the database's keys; currentSong() is the
            // 1-based default song resolved by selectSong(0), and the database's times are 0-indexed
            // in song order. Absent tune / empty database → m_duration stays 0 (open-ended).
            // Only read m_songLengths once the background loader has published it (acquire pairs with
            // the release in create()); a tune opened before then stays open-ended.
            if (m_dbReady.load(std::memory_order_acquire)) {
                char md5[33] = {};
                m_tune->createMD5New(md5);
                if (md5[0] != '\0') {
                    m_duration = m_songLengths.lookup(md5, info->currentSong() - 1).value_or(0.0);
                }
            }
        }
        m_title = metadata.title.empty() ? path.filename().string() : metadata.title;
        m_metadata = metadata;
        return true;
    } catch (const std::exception &e) {
        SDL_Log("SidPlugin: failed to open %s: %s", path.c_str(), e.what());
        if (m_engine) {
            m_engine->load(nullptr);
        }
        m_tune.reset();
        return false;
    }
}

void SidPlugin::close() {
    if (m_engine) {
        m_engine->load(nullptr); // 0 unloads the current tune from the engine
    }
    m_tune.reset();
    m_mixBuffer.clear();
    m_mixPos = 0;
    m_mixFrames = 0;
    m_metadata = std::monostate{};
    m_title.clear();
    m_duration = 0.0;
}

int SidPlugin::decode(std::int16_t *buffer, const int frames) {
    if (!m_engine || !m_tune) {
        return 0;
    }

    int written = 0;
    while (written < frames) {
        // Drain whatever the last play()/mix() produced before emulating another chunk.
        if (m_mixPos < m_mixFrames) {
            const std::size_t take =
                std::min<std::size_t>(m_mixFrames - m_mixPos, static_cast<std::size_t>(frames - written));
            std::memcpy(
                buffer + static_cast<std::size_t>(written) * CHANNELS,
                m_mixBuffer.data() + m_mixPos * CHANNELS,
                take * CHANNELS * sizeof(std::int16_t)
            );
            m_mixPos += take;
            written += static_cast<int>(take);
            continue;
        }

        // Run the emulation for a fixed slice, then mix the raw SID output to interleaved stereo.
        const int samples = m_engine->play(PLAY_CYCLES);
        if (samples <= 0) {
            break; // negative = engine error; zero would spin forever
        }
        const std::size_t needed = static_cast<std::size_t>(samples) * CHANNELS;
        if (m_mixBuffer.size() < needed) {
            m_mixBuffer.resize(needed); // safety net; open() pre-sizes so this should never fire
        }
        const unsigned int mixed = m_engine->mix(m_mixBuffer.data(), static_cast<unsigned int>(samples));
        m_mixFrames = mixed / CHANNELS;
        m_mixPos = 0;
    }
    return written;
}

std::string SidPlugin::getName() const {
    return "libsidplayfp";
}

const std::vector<std::string> &SidPlugin::getSupportedExtensions() const {
    return m_extensions;
}

std::string SidPlugin::getTitle() const {
    return m_title;
}

double SidPlugin::getPosition() const {
    return m_engine ? m_engine->timeMs() / 1000.0 : 0.0;
}

double SidPlugin::getDuration() const {
    // The HVSC Songlengths time for the current subtune, cached in open() when the bundled database
    // knows the tune. SID tunes carry no intrinsic length, so this is 0 = unknown (open-ended) when
    // the database is absent or does not list the tune — exactly the previous behaviour.
    return m_duration;
}

TrackMetadata SidPlugin::getMetadata() const {
    return m_metadata;
}

std::vector<PluginSetting> SidPlugin::getSettings() const {
    return {
        {"sid_model", "SID model", EnumOptions{{"Auto", "MOS 6581", "MOS 8580"}}, m_model, true},
        {"clock",     "Clock",     EnumOptions{{"Auto", "PAL", "NTSC"}},          m_clock, true}
    };
}

void SidPlugin::applySetting(const std::string &key, const int value) {
    // Clamp on store so a hand-edited INI can never feed an out-of-range value. Model/clock changes
    // take effect on the next open() — applying them live would restart the current tune.
    if (key == "sid_model") {
        m_model = std::clamp(value, 0, 2);
    } else if (key == "clock") {
        m_clock = std::clamp(value, 0, 2);
    }
}
