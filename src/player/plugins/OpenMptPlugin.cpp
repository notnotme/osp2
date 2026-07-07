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

#include "OpenMptPlugin.h"

#include <libopenmpt/libopenmpt.hpp>
#include <SDL_log.h>

#include <algorithm>
#include <cstdint>
#include <fstream>


namespace {
    // Maps the "interpolation" setting index to a libopenmpt filter length. Out-of-range → 0.
    std::int32_t interpolationLength(const int index) {
        switch (index) {
        case 1:
            return 1; // None
        case 2:
            return 2; // Linear
        case 3:
            return 4; // Cubic
        case 4:
            return 8; // Sinc
        default:
            return 0; // Default
        }
    }
} // namespace

OpenMptPlugin::OpenMptPlugin()
    : m_sampleRate(0),
      m_stereoSeparation(100),
      m_interpolation(0),
      m_loop(0) {}

OpenMptPlugin::~OpenMptPlugin() = default;

void OpenMptPlugin::create(const int sampleRate) {
    m_sampleRate = sampleRate;
    m_extensions = openmpt::get_supported_extensions();
}

void OpenMptPlugin::destroy() {
    m_module.reset();
}

bool OpenMptPlugin::open(const std::filesystem::path &path) {
    try {
        auto file = std::ifstream(path, std::ios::binary);
        if (!file.is_open()) {
            SDL_Log("OpenMptPlugin: cannot open %s", path.c_str());
            return false;
        }

        m_module = std::make_unique<openmpt::module>(file);
        m_module->set_repeat_count(m_loop ? -1 : 0);
        m_module->set_render_param(openmpt::module::RENDER_STEREOSEPARATION_PERCENT, m_stereoSeparation);
        m_module->set_render_param(
            openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, interpolationLength(m_interpolation)
        );
        m_metadata = ModuleMetadata{
            m_module->get_metadata("title"),
            m_module->get_metadata("artist"),
            m_module->get_metadata("type_long"),
            m_module->get_metadata("tracker"),
            m_module->get_num_channels(),
            m_module->get_num_patterns(),
            m_module->get_num_samples(),
            m_module->get_num_instruments(),
            m_module->get_metadata("message")
        };
        return true;
    } catch (const openmpt::exception &e) {
        SDL_Log("OpenMptPlugin: failed to parse %s: %s", path.c_str(), e.what());
        m_module.reset();
        return false;
    }
}

void OpenMptPlugin::close() {
    m_module.reset();
    m_metadata = std::monostate{};
}

int OpenMptPlugin::decode(std::int16_t *buffer, const int frames) {
    return static_cast<int>(m_module->read_interleaved_stereo(m_sampleRate, frames, buffer));
}

std::string OpenMptPlugin::getName() const {
    return "libopenmpt";
}

const std::vector<std::string> &OpenMptPlugin::getSupportedExtensions() const {
    return m_extensions;
}

std::string OpenMptPlugin::getTitle() const {
    return m_module ? m_module->get_metadata("title") : "";
}

double OpenMptPlugin::getPosition() const {
    return m_module ? m_module->get_position_seconds() : 0.0;
}

double OpenMptPlugin::getDuration() const {
    return m_module ? m_module->get_duration_seconds() : 0.0;
}

TrackMetadata OpenMptPlugin::getMetadata() const {
    return m_metadata;
}

std::vector<PluginSetting> OpenMptPlugin::getSettings() const {
    return {
        {"stereo_separation", "Stereo separation", IntRange{0, 200},                                            m_stereoSeparation},
        {"interpolation",
         "Interpolation",                          EnumOptions{{"Default", "None", "Linear", "Cubic", "Sinc"}},
         m_interpolation                                                                                                          },
        {"loop",              "Loop",              EnumOptions{{"Off", "On"}},                                  m_loop            }
    };
}

void OpenMptPlugin::applySetting(const std::string &key, const int value) {
    // Clamp on store so a hand-edited INI can never feed set_render_param an out-of-range value
    // (which throws and would fail every subsequent open()), nor leave m_interpolation pointing
    // past the EnumOptions labels. Stored values are therefore always valid for open()/getSettings().
    if (key == "stereo_separation") {
        m_stereoSeparation = std::clamp(value, 0, 200);
        if (m_module) {
            m_module->set_render_param(openmpt::module::RENDER_STEREOSEPARATION_PERCENT, m_stereoSeparation);
        }
    } else if (key == "interpolation") {
        m_interpolation = value >= 0 && value <= 4 ? value : 0;
        if (m_module) {
            m_module->set_render_param(
                openmpt::module::RENDER_INTERPOLATIONFILTER_LENGTH, interpolationLength(m_interpolation)
            );
        }
    } else if (key == "loop") {
        m_loop = value == 1 ? 1 : 0;
        if (m_module) {
            m_module->set_repeat_count(m_loop ? -1 : 0);
        }
    }
}
