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

#include "GmePlugin.h"

#include "../Charset.h"

#include <gme/gme.h>
#include <SDL_log.h>

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>


// decode() renders straight into the int16 output buffer via gme_play, which takes short*.
static_assert(
    sizeof(short) == sizeof(std::int16_t),
    "GmePlugin decodes gme_play's short output directly into the int16 audio buffer"
);

namespace {
    // Auto-advance play limit (see startTrack): with loop off, a track is faded out so it ends and
    // playback advances. Most GME formats (NSF/GBS/KSS/…) never self-limit — only SPC does — so without
    // an explicit fade a looping tune plays forever. Fade start = the track's length (or this default
    // when the length is unknown, e.g. a bare NSF); fade duration scales with the length, clamped so
    // full songs get a gentle tail and short jingles are not dragged out.
    constexpr long kUnknownLengthPlayLimitMs = 150000; // 2:30, libgme's historical default guess
    constexpr long kMinFadeMs = 500;
    constexpr long kMaxFadeMs = 8000;

    // Maps a possibly-null C string (libgme leaves absent fields as nullptr) to a std::string.
    std::string toString(const char *value) {
        return value != nullptr ? std::string(value) : std::string();
    }

    // ASCII lowercase for case-insensitive filename/extension matching. The cast through unsigned
    // char is required: std::tolower has undefined behavior on a plain char that is negative.
    std::string toLower(std::string_view value) {
        std::string lowered(value);
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return lowered;
    }
} // namespace

GmePlugin::GmePlugin()
    : m_sampleRate(0),
      m_emu(nullptr, &gme_delete),
      m_duration(0.0),
      m_trackCount(0),
      m_currentTrack(0),
      m_stereoDepth(0),
      m_accuracy(0),
      m_loop(0) {}

GmePlugin::~GmePlugin() = default;

void GmePlugin::create(const int sampleRate) {
    m_sampleRate = sampleRate;
    m_extensions = {"ay", "gbs", "gym", "hes", "kss", "nsf", "nsfe", "sap", "spc", "vgm", "vgz"};
}

void GmePlugin::destroy() {
    m_emu.reset();
}

bool GmePlugin::open(const std::filesystem::path &path) {
    // libgme is a C library and never throws, but the file read into a vector can (bad_alloc on a
    // large VGM under the Switch's constrained RAM); honor PlayerPlugin's "must not throw" contract.
    try {
        auto file = std::ifstream(path, std::ios::binary);
        if (!file.is_open()) {
            SDL_Log("GmePlugin: cannot open %s", path.c_str());
            return false;
        }

        const std::vector<char> data{std::istreambuf_iterator(file), std::istreambuf_iterator<char>()};

        Music_Emu *raw = nullptr;
        if (const gme_err_t error = gme_open_data(data.data(), static_cast<long>(data.size()), &raw, m_sampleRate)) {
            SDL_Log("GmePlugin: failed to parse %s: %s", path.c_str(), error);
            return false;
        }
        m_emu.reset(raw);

        // Overlay a sibling ".m3u" playlist when one sits next to the tune: it carries per-subtune
        // titles and lengths (the "NSF M3U" convention) that a bare header lacks, populating the
        // gme_info_t.song/length fields startTrack() reads. Optional enrichment — a missing or
        // malformed playlist is logged and ignored, never a reason to fail the open. Must run before
        // m_trackCount is read below, since a loaded playlist redefines gme_track_count.
        overlaySiblingM3u(path);

        // Accuracy is best set before the track starts; stereo depth is applied after.
        gme_enable_accuracy(m_emu.get(), m_accuracy);

        m_trackCount = gme_track_count(m_emu.get());
        if (!startTrack(0)) {
            m_emu.reset();
            return false;
        }
        return true;
    } catch (const std::exception &e) {
        SDL_Log("GmePlugin: failed to open %s: %s", path.c_str(), e.what());
        m_emu.reset();
        return false;
    }
}

void GmePlugin::overlaySiblingM3u(const std::filesystem::path &tunePath) {
    // Self-contained best-effort: the emulator is already fully loaded, so this enrichment must never
    // fail the open. The error_code filesystem overloads below cover directory/stat errors; this catch
    // covers the rest (bad_alloc while gathering lines under the Switch's constrained RAM, etc.) so a
    // throw here is swallowed rather than propagating into open()'s catch and aborting the track.
    try {
        // Combined layout: a single "<stem>.m3u" next to the tune, one line per subtrack. gme_load_m3u
        // parses it directly; on success we are done and never look at the exploded layout.
        auto combined = tunePath;
        combined.replace_extension(".m3u");
        std::error_code ec;
        if (std::filesystem::is_regular_file(combined, ec)) {
            if (const gme_err_t error = gme_load_m3u(m_emu.get(), combined.string().c_str())) {
                SDL_Log("GmePlugin: ignoring sibling m3u %s: %s", combined.string().c_str(), error);
            } else {
                return;
            }
        }

        // Exploded layout (common Zophar dumps): no combined playlist, but several ".m3u" files in the
        // same directory, each named by a track title and holding ONE line that references this tune by
        // filename + track index (e.g. "TUNE.gbs::GBS,0,Title,2:29,,10"). Concatenating those
        // referencing lines, ordered by their source ".m3u" filename, rebuilds the curated album
        // playlist for gme_load_m3u_data.
        const auto tuneFilename = toLower(tunePath.filename().string());
        const auto combinedFilename = toLower(combined.filename().string());

        // (source ".m3u" filename, referencing line) — sorted by source filename to recover album order.
        std::vector<std::pair<std::string, std::string>> entries;
        for (auto it = std::filesystem::directory_iterator(tunePath.parent_path(), ec);
             !ec && it != std::filesystem::directory_iterator();
             it.increment(ec)) {
            const auto &entryPath = it->path();
            if (!it->is_regular_file(ec) || ec) {
                continue;
            }
            if (toLower(entryPath.extension().string()) != ".m3u" ||
                toLower(entryPath.filename().string()) == combinedFilename) {
                continue;
            }

            auto file = std::ifstream(entryPath, std::ios::binary);
            if (!file.is_open()) {
                continue;
            }
            const auto sourceFilename = entryPath.filename().string();
            std::string line;
            while (std::getline(file, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                // Keep only lines referencing THIS tune: the field before the first "::" is the
                // referenced filename. Other lines (comments, blanks, entries for sibling tunes) are
                // dropped so the rebuilt buffer holds exactly this tune's curated subtracks.
                const auto separator = line.find("::");
                if (separator != std::string::npos && toLower(line.substr(0, separator)) == tuneFilename) {
                    entries.emplace_back(sourceFilename, line);
                }
            }
        }

        if (entries.empty()) {
            return;
        }
        std::sort(entries.begin(), entries.end());

        std::string buffer;
        for (const auto &[source, line] : entries) {
            buffer += line;
            buffer += '\n';
        }

        if (const gme_err_t error =
                gme_load_m3u_data(m_emu.get(), buffer.data(), static_cast<long>(buffer.size()))) {
            SDL_Log(
                "GmePlugin: ignoring exploded m3u playlist in %s: %s",
                tunePath.parent_path().string().c_str(),
                error
            );
        }
    } catch (const std::exception &e) {
        SDL_Log("GmePlugin: sibling m3u overlay for %s failed: %s", tunePath.string().c_str(), e.what());
    }
}

void GmePlugin::close() {
    m_emu.reset();
    m_metadata = std::monostate{};
    m_title.clear();
    m_duration = 0.0;
    m_trackCount = 0;
    m_currentTrack = 0;
}

bool GmePlugin::startTrack(const int index) {
    // Governs SPC's built-in length limit (the only format that self-fades from its stated length):
    // loop on -> off so an SPC repeats forever. Every other GME format ignores this and is bounded by
    // the explicit fade set below instead. Deferred: it only affects tracks started from here.
    gme_set_autoload_playback_limit(m_emu.get(), m_loop ? 0 : 1);

    if (const gme_err_t error = gme_start_track(m_emu.get(), index)) {
        SDL_Log("GmePlugin: failed to start track %d: %s", index, error);
        return false;
    }

    // gme_start_track can reset per-track effects, so re-apply the cached stereo depth every time.
    gme_set_stereo_depth(m_emu.get(), m_stereoDepth / 100.0);

    gme_info_t *info = nullptr;
    if (const gme_err_t error = gme_track_info(m_emu.get(), &info, index)) {
        SDL_Log("GmePlugin: failed to read track %d info: %s", index, error);
        return false;
    }

    // game/system/author/copyright/comment are file-level (identical across subtracks); song and
    // length are per-track, so title and duration are refreshed for the selected subtrack.
    // NSF/GBS/… header fields are Shift-JIS (ASCII tags pass through unchanged); transcode to UTF-8
    // for Dear ImGui at the plugin boundary, where the source charset is known.
    m_metadata = GmeMetadata{
        toUtf8(toString(info->game), Charset::ShiftJis),
        toUtf8(toString(info->system), Charset::ShiftJis),
        toUtf8(toString(info->author), Charset::ShiftJis),
        toUtf8(toString(info->copyright), Charset::ShiftJis),
        toUtf8(toString(info->comment), Charset::ShiftJis),
        gme_track_count(m_emu.get())
    };

    m_title = toUtf8(toString(info->song), Charset::ShiftJis);
    if (m_title.empty()) {
        m_title = toUtf8(toString(info->game), Charset::ShiftJis);
    }
    // Report a real length only. A bare NSF header carries no per-track length, so libgme fills
    // play_length with its built-in default (150000 ms = 2:30) for every subsong; trusting it would
    // fake a constant 2:30. Prefer the explicit length (NSFe/.m3u), then intro+loop, else unknown (0).
    if (info->length > 0) {
        m_duration = info->length / 1000.0;
    } else {
        const auto intro = info->intro_length > 0 ? info->intro_length : 0;
        const auto loop = info->loop_length > 0 ? info->loop_length : 0;
        m_duration = intro + loop > 0 ? (intro + loop) / 1000.0 : 0.0;
    }
    m_currentTrack = index;

    // End-of-track for auto-advance. gme_start_track resets any fade, so (re)apply it here after the
    // length is known. With loop off, fade out at the track's length — or a default when the length is
    // unknown — so the track ends and playback advances to the next subtrack/file; without this a
    // non-SPC tune loops forever. With loop on, leave it running (no fade). set_fade must come after
    // gme_start_track, which it does.
    if (!m_loop) {
        const long limitMs = m_duration > 0.0 ? static_cast<long>(m_duration * 1000.0) : kUnknownLengthPlayLimitMs;
        const long fadeMs = std::clamp(limitMs / 10, kMinFadeMs, kMaxFadeMs);
        gme_set_fade_msecs(m_emu.get(), static_cast<int>(limitMs), static_cast<int>(fadeMs));
    }

    gme_free_info(info);
    return true;
}

int GmePlugin::decode(std::int16_t *buffer, const int frames) {
    if (!m_emu || gme_track_ended(m_emu.get())) {
        return 0;
    }

    // gme_play writes frames*2 interleaved-stereo 16-bit samples straight into the caller buffer.
    if (gme_play(m_emu.get(), frames * 2, buffer)) {
        return 0;
    }
    return frames;
}

std::string GmePlugin::getName() const {
    return "libgme";
}

const std::vector<std::string> &GmePlugin::getSupportedExtensions() const {
    return m_extensions;
}

std::string GmePlugin::getTitle() const {
    return m_title;
}

double GmePlugin::getPosition() const {
    return m_emu ? gme_tell(m_emu.get()) / 1000.0 : 0.0;
}

double GmePlugin::getDuration() const {
    return m_duration;
}

TrackMetadata GmePlugin::getMetadata() const {
    return m_metadata;
}

std::vector<PluginSetting> GmePlugin::getSettings() const {
    return {
        {"stereo_depth", "Stereo depth", IntRange{0, 100}, m_stereoDepth},
        {"accuracy", "Emulation accuracy", EnumOptions{{"Fast", "Accurate"}}, m_accuracy},
        {"loop", "Loop", EnumOptions{{"Off", "On"}}, m_loop, true}
    };
}

void GmePlugin::applySetting(const std::string &key, const int value) {
    // Clamp on store so a hand-edited INI can never feed an out-of-range value, then apply to the
    // live emulator if one is open. Stored values are always valid for open()/getSettings().
    if (key == "stereo_depth") {
        m_stereoDepth = std::clamp(value, 0, 100);
        if (m_emu) {
            gme_set_stereo_depth(m_emu.get(), m_stereoDepth / 100.0);
        }
    } else if (key == "accuracy") {
        m_accuracy = value == 1 ? 1 : 0;
        if (m_emu) {
            gme_enable_accuracy(m_emu.get(), m_accuracy);
        }
    } else if (key == "loop") {
        // Deferred: the loop flag is consumed by gme_set_autoload_playback_limit at the next
        // startTrack(), so store it now and do not touch the live emulator.
        m_loop = value == 1 ? 1 : 0;
    }
}

int GmePlugin::getSubtrackCount() const {
    return m_trackCount;
}

int GmePlugin::getCurrentSubtrack() const {
    return m_currentTrack;
}

void GmePlugin::selectSubtrack(const int index) {
    // m_trackCount <= 0 only when no file is open (m_emu is then null too); guarding it here keeps
    // the std::clamp bound [0, m_trackCount - 1] well-formed without relying on that cross-check.
    if (!m_emu || m_trackCount <= 0) {
        return;
    }
    // Called under PlayerController::m_mutex (same contract as applySetting), so touching the live
    // emulator is safe. On failure keep the current track — the file is still valid, so unlike
    // open() we must NOT reset m_emu.
    const int clamped = std::clamp(index, 0, m_trackCount - 1);
    if (!startTrack(clamped)) {
        SDL_Log("GmePlugin: failed to select subtrack %d", clamped);
    }
}
