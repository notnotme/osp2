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

#include <gme/gme.h>
#include <SDL_log.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <iterator>


// decode() renders straight into the int16 output buffer via gme_play, which takes short*.
static_assert(
    sizeof(short) == sizeof(std::int16_t),
    "GmePlugin decodes gme_play's short output directly into the int16 audio buffer"
);

namespace {
    // Maps a possibly-null C string (libgme leaves absent fields as nullptr) to a std::string.
    std::string toString(const char *value) {
        return value != nullptr ? std::string(value) : std::string();
    }
} // namespace

GmePlugin::GmePlugin()
    : m_sampleRate(0),
      m_emu(nullptr, &gme_delete),
      m_duration(0.0),
      m_stereoDepth(0),
      m_accuracy(0) {}

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

        // Accuracy is best set before the track starts; stereo depth is applied after.
        gme_enable_accuracy(m_emu.get(), m_accuracy);

        if (const gme_err_t error = gme_start_track(m_emu.get(), 0)) {
            SDL_Log("GmePlugin: failed to start track in %s: %s", path.c_str(), error);
            m_emu.reset();
            return false;
        }

        gme_set_stereo_depth(m_emu.get(), m_stereoDepth / 100.0);

        gme_info_t *info = nullptr;
        if (const gme_err_t error = gme_track_info(m_emu.get(), &info, 0)) {
            SDL_Log("GmePlugin: failed to read track info in %s: %s", path.c_str(), error);
            m_emu.reset();
            return false;
        }

        m_metadata = GmeMetadata{
            toString(info->game),
            toString(info->system),
            toString(info->author),
            toString(info->copyright),
            toString(info->comment),
            gme_track_count(m_emu.get())
        };

        m_title = toString(info->song);
        if (m_title.empty()) {
            m_title = toString(info->game);
        }
        m_duration = info->play_length > 0 ? info->play_length / 1000.0 : 0.0;

        gme_free_info(info);
        return true;
    } catch (const std::exception &e) {
        SDL_Log("GmePlugin: failed to open %s: %s", path.c_str(), e.what());
        m_emu.reset();
        return false;
    }
}

void GmePlugin::close() {
    m_emu.reset();
    m_metadata = std::monostate{};
    m_title.clear();
    m_duration = 0.0;
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
        {"stereo_depth", "Stereo depth",       IntRange{0, 100},                  m_stereoDepth},
        {"accuracy",     "Emulation accuracy", EnumOptions{{"Fast", "Accurate"}}, m_accuracy   }
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
    }
}
