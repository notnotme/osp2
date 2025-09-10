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

#include <fstream>


OpenMptPlugin::OpenMptPlugin()
    : m_sampleRate(0) {}

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
        m_module->set_repeat_count(0);
        return true;
    } catch (const openmpt::exception &e) {
        SDL_Log("OpenMptPlugin: failed to parse %s: %s", path.c_str(), e.what());
        m_module.reset();
        return false;
    }
}

void OpenMptPlugin::close() {
    m_module.reset();
}

int OpenMptPlugin::decode(float *buffer, const int frames) {
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
