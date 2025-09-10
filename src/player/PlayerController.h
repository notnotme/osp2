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

#ifndef OSP2_PLAYER_CONTROLLER_H
#define OSP2_PLAYER_CONTROLLER_H

#include <SDL.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "PlayerPlugin.h"
#include "PlayerState.h"


class PlayerController final {
public:
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr int CHANNELS = 2;
    static constexpr int BUFFER_FRAMES = 1024;

private:
    // Immutable while the audio device is open; safe to read from both threads without locking.
    std::vector<std::unique_ptr<PlayerPlugin>> m_plugins;
    SDL_AudioDeviceID m_device;

    // Shared with the SDL audio thread; guarded by m_mutex.
    mutable std::mutex m_mutex;
    PlayerPlugin *m_activePlugin;
    PlayerState m_state;
    std::filesystem::path m_currentPath;

    // Set by the audio thread at end of track, consumed once by the main loop.
    std::atomic_bool m_trackEnded;

public:
    PlayerController(const PlayerController &) = delete;
    PlayerController &operator=(const PlayerController &) = delete;
    explicit PlayerController();
    ~PlayerController() = default;

public:
    void create();
    void destroy();
    bool play(const std::filesystem::path &path);
    void play();
    void pause();
    void stop();

public:
    [[nodiscard]] PlayerState getState() const;
    [[nodiscard]] std::string getCurrentFileName() const;
    [[nodiscard]] std::filesystem::path getCurrentPath() const;
    [[nodiscard]] std::string getCurrentTitle() const;
    [[nodiscard]] bool isSupported(const std::filesystem::path &path) const;
    [[nodiscard]] bool consumeTrackEnded();

private:
    static void audioCallback(void *userdata, Uint8 *stream, int len);
    void decode(Uint8 *stream, int len);
    [[nodiscard]] PlayerPlugin *findPluginFor(const std::filesystem::path &path) const;
};


#endif //OSP2_PLAYER_CONTROLLER_H
