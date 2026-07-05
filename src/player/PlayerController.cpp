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

#include "PlayerController.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "plugins/OpenMptPlugin.h"


PlayerController::PlayerController()
    : m_device(0),
      m_activePlugin(nullptr),
      m_state(PlayerState::STOPPED),
      m_trackEnded(false) {}

void PlayerController::create() {
    m_plugins.emplace_back(std::make_unique<OpenMptPlugin>());

    for (const auto &plugin : m_plugins) {
        plugin->create(SAMPLE_RATE);
    }

    SDL_AudioSpec want = {};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_F32SYS;
    want.channels = CHANNELS;
    want.samples = BUFFER_FRAMES;
    want.callback = &PlayerController::audioCallback;
    want.userdata = this;

    SDL_AudioSpec have = {};
    m_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (m_device == 0) {
        throw std::runtime_error(SDL_GetError());
    }

    // The device runs for the whole app lifetime; the callback outputs silence when idle.
    SDL_PauseAudioDevice(m_device, 0);
}

void PlayerController::destroy() {
    // Close the device first so the callback can never fire on destroyed plugins.
    if (m_device != 0) {
        SDL_CloseAudioDevice(m_device);
        m_device = 0;
    }

    for (const auto &plugin : m_plugins) {
        plugin->close();
        plugin->destroy();
    }
    m_plugins.clear();
    m_activePlugin = nullptr;
    m_state = PlayerState::STOPPED;
}

bool PlayerController::play(const std::filesystem::path &path) {
    auto *plugin = findPluginFor(path);
    if (plugin == nullptr) {
        return false;
    }

    std::scoped_lock lock(m_mutex);
    if (m_activePlugin != nullptr) {
        m_activePlugin->close();
        m_activePlugin = nullptr;
    }
    m_state = PlayerState::STOPPED;
    m_currentPath.clear();
    m_trackEnded.store(false);

    if (!plugin->open(path)) {
        return false;
    }

    m_activePlugin = plugin;
    m_state = PlayerState::PLAYING;
    m_currentPath = path;
    return true;
}

void PlayerController::play() {
    std::scoped_lock lock(m_mutex);
    if (m_state == PlayerState::PAUSED) {
        m_state = PlayerState::PLAYING;
    }
}

void PlayerController::pause() {
    std::scoped_lock lock(m_mutex);
    if (m_state == PlayerState::PLAYING) {
        m_state = PlayerState::PAUSED;
    }
}

void PlayerController::stop() {
    std::scoped_lock lock(m_mutex);
    if (m_activePlugin != nullptr) {
        m_activePlugin->close();
        m_activePlugin = nullptr;
    }
    m_state = PlayerState::STOPPED;
    m_currentPath.clear();
    m_trackEnded.store(false);
}

PlayerState PlayerController::getState() const {
    std::scoped_lock lock(m_mutex);
    return m_state;
}

std::string PlayerController::getCurrentFileName() const {
    std::scoped_lock lock(m_mutex);
    return m_currentPath.filename().string();
}

std::filesystem::path PlayerController::getCurrentPath() const {
    std::scoped_lock lock(m_mutex);
    return m_currentPath;
}

std::string PlayerController::getCurrentTitle() const {
    std::scoped_lock lock(m_mutex);
    return m_activePlugin != nullptr ? m_activePlugin->getTitle() : "";
}

PlaybackStatus PlayerController::getStatus() const {
    std::scoped_lock lock(m_mutex);
    if (m_activePlugin == nullptr) {
        return {m_state, "", m_currentPath.filename().string(), 0.0, 0.0};
    }
    return {
        m_state,
        m_activePlugin->getTitle(),
        m_currentPath.filename().string(),
        m_activePlugin->getPosition(),
        m_activePlugin->getDuration()
    };
}

bool PlayerController::isSupported(const std::filesystem::path &path) const {
    return findPluginFor(path) != nullptr;
}

bool PlayerController::consumeTrackEnded() {
    return m_trackEnded.exchange(false);
}

void PlayerController::audioCallback(void *userdata, Uint8 *stream, const int len) {
    static_cast<PlayerController *>(userdata)->decode(stream, len);
}

void PlayerController::decode(Uint8 *stream, const int len) {
    std::scoped_lock lock(m_mutex);
    if (m_state != PlayerState::PLAYING || m_activePlugin == nullptr) {
        SDL_memset(stream, 0, len);
        return;
    }

    const auto frames_wanted = len / static_cast<int>(sizeof(float) * CHANNELS);
    auto *buffer = reinterpret_cast<float *>(stream);
    const auto frames_written = m_activePlugin->decode(buffer, frames_wanted);
    if (frames_written < frames_wanted) {
        SDL_memset(buffer + frames_written * CHANNELS, 0,
                   (frames_wanted - frames_written) * sizeof(float) * CHANNELS);
        // Track teardown stays on the main thread; only flip the state here.
        m_state = PlayerState::STOPPED;
        m_trackEnded.store(true);
    }
}

PlayerPlugin *PlayerController::findPluginFor(const std::filesystem::path &path) const {
    auto extension = path.extension().string();
    if (extension.empty()) {
        return nullptr;
    }
    extension.erase(0, 1);
    std::ranges::transform(extension, extension.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    for (const auto &plugin : m_plugins) {
        const auto &extensions = plugin->getSupportedExtensions();
        if (std::ranges::find(extensions, extension) != extensions.end()) {
            return plugin.get();
        }
    }
    return nullptr;
}
