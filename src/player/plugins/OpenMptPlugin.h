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


/**
 * Decoder plugin wrapping libopenmpt for tracker formats (MOD, XM, S3M, IT, ...).
 *
 * Extensions come from openmpt::get_supported_extensions(). Captured strings need no transcoding — libopenmpt
 * already returns UTF-8. All three settings (stereo_separation, interpolation, loop) are cached members applied
 * to each module on open() and applied live in applySetting(); loop On maps to set_repeat_count(-1), so the
 * module loops seamlessly inside the decoder and never signals end-of-track.
 */
class OpenMptPlugin final : public PlayerPlugin {
private:
    int m_sampleRate;                          ///< Output sample rate handed to every module render call.
    std::vector<std::string> m_extensions;     ///< Supported extensions, from openmpt::get_supported_extensions().
    std::unique_ptr<openmpt::module> m_module; ///< The open module; shared with the audio thread while active.
    /**
     * Captured once in open() so getMetadata()/getTitle() never touch the audio-thread-shared module.
     */
    TrackMetadata m_metadata;
    /** Raw title captured in open(); may be empty — the Gui falls back to the filename. */
    std::string m_title;
    int m_stereoSeparation; ///< Cached stereo separation percent, re-applied to each module on open().
    int m_interpolation;    ///< Index into the Interpolation enum labels (see getSettings()); re-applied on open().
    int m_loop;             ///< 0/1, maps to set_repeat_count (1 -> -1, loop forever; 0 -> play once).

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
