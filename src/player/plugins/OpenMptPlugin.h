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

#ifndef OSP2_OPENMPT_PLUGIN_H
#define OSP2_OPENMPT_PLUGIN_H

#include <cstdint>
#include <memory>

#include "../PlayerPlugin.h"

namespace openmpt {
    class module;
}


class OpenMptPlugin final : public PlayerPlugin {
private:
    int m_sampleRate;
    std::vector<std::string> m_extensions;
    std::unique_ptr<openmpt::module> m_module;
    // Captured once in open() so getMetadata()/getTitle() never touch the audio-thread-shared
    // module. m_title stays raw metadata (may be empty — the Gui falls back to the filename).
    TrackMetadata m_metadata;
    std::string m_title;
    // Cached render settings, re-applied to each module on open(); m_interpolation is an index
    // into the Interpolation enum labels (see getSettings()). m_loop is 0/1 and maps to
    // set_repeat_count (1 -> -1, loop forever; 0 -> play once).
    int m_stereoSeparation;
    int m_interpolation;
    int m_loop;

public:
    OpenMptPlugin(const OpenMptPlugin &) = delete;
    OpenMptPlugin &operator=(const OpenMptPlugin &) = delete;
    explicit OpenMptPlugin(int sampleRate);
    ~OpenMptPlugin() override;

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


#endif //OSP2_OPENMPT_PLUGIN_H
