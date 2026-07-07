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

#include "Sc68Plugin.h"

#include "../Charset.h"

#include <sc68/sc68.h>
#include <SDL_log.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <iterator>
#include <vector>


namespace {
    // Interleaved stereo — sc68_process() fills 4 bytes (two S16 samples) per frame.
    constexpr int CHANNELS = 2;

    // Ceiling on consecutive zero-frame sc68_process() passes tolerated in one decode() before we
    // bail. A deferred track-change/loop event costs exactly one such empty pass (see decode()); the
    // cap only exists so a persistent SC68_IDLE can never spin the audio thread under m_mutex.
    constexpr int MAX_EMPTY_PASSES = 8;

    // Maps a possibly-null C string (libsc68 leaves absent fields as nullptr) to a std::string.
    std::string toString(const char *value) {
        return value != nullptr ? std::string(value) : std::string();
    }

    bool equalsIgnoreCase(const char *a, const char *b) {
        if (a == nullptr || b == nullptr) {
            return false;
        }
        for (; *a != '\0' && *b != '\0'; ++a, ++b) {
            if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b))) {
                return false;
            }
        }
        return *a == *b;
    }

    // Scans a cinfo's tag array for a key case-insensitively equal to `key`, returning its value
    // (empty if absent). sc68 exposes the composer only as a free-form tag, not a predefined field.
    std::string findTag(const sc68_cinfo_t &info, const char *key) {
        for (int i = 0; i < info.tags; ++i) {
            if (equalsIgnoreCase(info.tag[i].key, key)) {
                return toString(info.tag[i].val);
            }
        }
        return {};
    }
} // namespace

Sc68Plugin::Sc68Plugin()
    : m_sampleRate(0) {}

Sc68Plugin::~Sc68Plugin() = default;

void Sc68Plugin::create(const int sampleRate) {
    m_sampleRate = sampleRate;
    m_extensions = {"sc68", "sndh", "snd"};

    // sc68_init toggles a single global init flag; the sole Sc68Plugin instance owns the one
    // init/shutdown pair. A failure here leaves m_sc68 null, so every open() fails defensively
    // rather than crashing, and destroy() must not call sc68_shutdown() (guarded by m_initialized).
    if (sc68_init(nullptr) != SC68_OK) {
        SDL_Log("Sc68Plugin: sc68_init failed — sc68 playback disabled");
        return;
    }
    m_initialized = true;

    sc68_create_t create = {};
    create.sampling_rate = static_cast<unsigned int>(m_sampleRate);
    create.name = "osp2";
    m_sc68 = sc68_create(&create);
    if (m_sc68 == nullptr) {
        SDL_Log("Sc68Plugin: sc68_create failed — sc68 playback disabled");
        return;
    }

    // Force interleaved signed-16-bit output so decode() can hand our int16 buffer straight through;
    // this call is what guarantees the whole int16 pipeline, so a failure must be surfaced.
    if (sc68_cntl(m_sc68, SC68_SET_PCM, SC68_PCM_S16) != SC68_OK) {
        SDL_Log("Sc68Plugin: could not force S16 PCM output");
    }
}

void Sc68Plugin::destroy() {
    if (m_sc68 != nullptr) {
        sc68_close(m_sc68);
        sc68_destroy(m_sc68);
        m_sc68 = nullptr;
    }
    if (m_initialized) {
        sc68_shutdown(); // pair only with a successful sc68_init()
        m_initialized = false;
    }
}

bool Sc68Plugin::open(const std::filesystem::path &path) {
    m_playedFrames = 0;
    m_ended = false;
    m_duration = 0.0;
    m_metadata = std::monostate{};
    m_title.clear();

    if (m_sc68 == nullptr) {
        return false;
    }

    // Read the file into memory ourselves (like SidPlugin) — the read can throw (bad_alloc), so
    // honor PlayerPlugin's "must not throw" contract by catching everything and returning false.
    std::vector<char> data;
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            SDL_Log("Sc68Plugin: cannot open %s", path.c_str());
            return false;
        }
        data.assign(std::istreambuf_iterator(file), std::istreambuf_iterator<char>());
    } catch (const std::exception &e) {
        SDL_Log("Sc68Plugin: failed to read %s: %s", path.c_str(), e.what());
        return false;
    }
    if (data.empty()) {
        SDL_Log("Sc68Plugin: empty file %s", path.c_str());
        return false;
    }

    if (sc68_load_mem(m_sc68, data.data(), static_cast<int>(data.size())) != SC68_OK) {
        SDL_Log("Sc68Plugin: cannot load %s", path.c_str());
        return false;
    }
    // aSIDifier mode is consumed by sc68_play() (it loads the asidifier replay), so set it first.
    // The enum index maps straight onto SC68_ASID_OFF/ON/FORCE (0/1/2).
    sc68_cntl(m_sc68, SC68_SET_ASID, m_asid);
    // The loop count is fixed at sc68_play() time (SC68_CUR_TRACK is a getter, so it can't change
    // live): SC68_INF_LOOP repeats forever, SC68_DEF_LOOP is the default single playthrough.
    if (sc68_play(m_sc68, SC68_DEF_TRACK, m_loop ? SC68_INF_LOOP : SC68_DEF_LOOP) != SC68_OK) {
        SDL_Log("Sc68Plugin: cannot play %s", path.c_str());
        sc68_close(m_sc68);
        return false;
    }

    Sc68Metadata metadata;
    sc68_music_info_t info = {};
    if (sc68_music_info(m_sc68, &info, SC68_DEF_TRACK, nullptr) == SC68_OK) {
        // Fields point into the loaded disk (valid only until sc68_close), so copy every string.
        // Atari ST / Amiga tag text is Latin-1; transcode to UTF-8 for Dear ImGui. hw strings are
        // library-generated but harmless under Latin-1 (ASCII passes through unchanged).
        metadata.title = toUtf8(toString(info.title), Charset::Latin1);
        if (metadata.title.empty()) {
            metadata.title = toUtf8(toString(info.album), Charset::Latin1);
        }
        metadata.author = toUtf8(toString(info.artist), Charset::Latin1);
        metadata.composer = toUtf8(findTag(info.trk, "composer"), Charset::Latin1);
        if (metadata.composer.empty()) {
            metadata.composer = toUtf8(findTag(info.dsk, "composer"), Charset::Latin1);
        }
        metadata.hardware = toUtf8(toString(info.trk.hw), Charset::Latin1);
        if (metadata.hardware.empty()) {
            metadata.hardware = toUtf8(toString(info.dsk.hw), Charset::Latin1);
        }
        metadata.ripper = toUtf8(toString(info.ripper), Charset::Latin1);

        const unsigned int time_ms = info.trk.time_ms != 0 ? info.trk.time_ms : info.dsk.time_ms;
        m_duration = time_ms / 1000.0;
    }

    m_title = metadata.title.empty() ? path.filename().string() : metadata.title;
    m_metadata = metadata;
    return true;
}

void Sc68Plugin::close() {
    if (m_sc68 != nullptr) {
        sc68_stop(m_sc68);
        sc68_close(m_sc68);
    }
    m_ended = false;
    m_metadata = std::monostate{};
    m_title.clear();
}

int Sc68Plugin::decode(std::int16_t *buffer, const int frames) {
    // Runs on the audio thread under PlayerController::m_mutex.
    if (m_sc68 == nullptr || m_ended) {
        return 0;
    }

    // sc68_play() only *posts* the track change; sc68_process() applies it lazily and reports
    // SC68_CHANGE with zero frames on that first pass, before emulating a sample. So one call can
    // fall short of the request without the track having ended — loop until the buffer is full or
    // the track really ends, exactly like SidPlugin drives its variable-yield engine.
    int written = 0;
    int emptyPasses = 0;
    while (written < frames) {
        int n = frames - written; // frames still needed; sc68 writes ≤ 4*n bytes from the cursor
        const int code = sc68_process(m_sc68, buffer + static_cast<std::size_t>(written) * CHANNELS, &n);
        // SC68_ERROR is ~0 (every bit set), so a fatal result must be tested with equality — a bitwise
        // `code & SC68_ERROR` would also match the normal SC68_END/SC68_CHANGE/SC68_IDLE status bits.
        if (code == SC68_ERROR) {
            m_ended = true;
            break;
        }
        if (n > 0) {
            written += n;
            emptyPasses = 0;
        } else if (++emptyPasses >= MAX_EMPTY_PASSES) {
            break; // stall guard: a deferred event yields one empty pass, never an unbounded spin
        }
        if (code & SC68_END) {
            m_ended = true; // signal end; any frames produced this pass are already counted
            break;
        }
    }

    m_playedFrames += static_cast<std::uint64_t>(written);
    return written; // written < frames signals end-of-track to PlayerController
}

std::string Sc68Plugin::getName() const {
    return "sc68";
}

const std::vector<std::string> &Sc68Plugin::getSupportedExtensions() const {
    return m_extensions;
}

std::string Sc68Plugin::getTitle() const {
    return m_title;
}

double Sc68Plugin::getPosition() const {
    // Called under m_mutex. Derived from decoded frames — sc68 has no cheap position query.
    return m_sampleRate > 0 ? static_cast<double>(m_playedFrames) / m_sampleRate : 0.0;
}

double Sc68Plugin::getDuration() const {
    return m_duration;
}

TrackMetadata Sc68Plugin::getMetadata() const {
    return m_metadata;
}

std::vector<PluginSetting> Sc68Plugin::getSettings() const {
    return {
        {"asid", "aSID (YM to SID)", EnumOptions{{"Off", "On", "Force"}}, m_asid, true},
        {"loop", "Loop",             EnumOptions{{"Off", "On"}},          m_loop, true}
    };
}

void Sc68Plugin::applySetting(const std::string &key, const int value) {
    // Clamp on store so a hand-edited INI can never feed an out-of-range value. aSID is loaded at
    // sc68_play() time, so the change takes effect on the next open() (a live change would restart
    // the tune); only YM/ST tracks that advertise aSID caps are affected — a no-op otherwise.
    if (key == "asid") {
        m_asid = std::clamp(value, 0, 2);
    } else if (key == "loop") {
        // Deferred like asid: the loop count is fixed by sc68_play() at open() time, so store it now
        // and let the next open() pick it up.
        m_loop = value == 1 ? 1 : 0;
    }
}
