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

#include <memory>

#include "../PlayerPlugin.h"

namespace openmpt { class module; }


class OpenMptPlugin final : public PlayerPlugin {
private:
    int m_sampleRate;
    std::vector<std::string> m_extensions;
    std::unique_ptr<openmpt::module> m_module;
    // Captured once in open() so getMetadata() never touches the audio-thread-shared module.
    TrackMetadata m_metadata;

public:
    OpenMptPlugin(const OpenMptPlugin &) = delete;
    OpenMptPlugin &operator=(const OpenMptPlugin &) = delete;
    explicit OpenMptPlugin();
    ~OpenMptPlugin() override;

public:
    void create(int sampleRate) override;
    void destroy() override;
    [[nodiscard]] bool open(const std::filesystem::path &path) override;
    void close() override;
    [[nodiscard]] int decode(float *buffer, int frames) override;
    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] const std::vector<std::string> &getSupportedExtensions() const override;
    [[nodiscard]] std::string getTitle() const override;
    [[nodiscard]] double getPosition() const override;
    [[nodiscard]] double getDuration() const override;
    [[nodiscard]] TrackMetadata getMetadata() const override;
};


#endif //OSP2_OPENMPT_PLUGIN_H
