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
#include <cstdint>
#include <stdexcept>

#include "plugins/OpenMptPlugin.h"
#include "plugins/GmePlugin.h"
#include "plugins/SidPlugin.h"
#include "plugins/Sc68Plugin.h"


// The tap's fixed capacity is declared independently of PlayerController (to keep AudioTap.h
// dependency-free), so pin the two together here where both headers are visible: a mismatched
// constant would silently truncate the published waveform.
static_assert(AudioTap::MAX_FRAMES == static_cast<std::size_t>(PlayerController::BUFFER_FRAMES));
static_assert(AudioTap::CHANNELS == static_cast<std::size_t>(PlayerController::CHANNELS));


PlayerController::PlayerController()
    : m_device(0),
      m_activePlugin(nullptr),
      m_state(PlayerState::STOPPED),
      m_trackEnded(false),
      m_loading(false),
      m_loadPending(false),
      m_loadCancelled(false),
      m_loadPlugin(nullptr),
      m_loadSucceeded(false) {}

void PlayerController::create() {
    m_plugins.emplace_back(std::make_unique<OpenMptPlugin>());
    m_plugins.emplace_back(std::make_unique<GmePlugin>());
    m_plugins.emplace_back(std::make_unique<SidPlugin>());
    m_plugins.emplace_back(std::make_unique<Sc68Plugin>());

    for (const auto &plugin : m_plugins) {
        plugin->create(SAMPLE_RATE);
    }

    SDL_AudioSpec want = {};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
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
    // Join any in-flight parse before touching the device or plugins: no open() may run while we
    // tear plugins down. The plugin being opened is inactive, so the device need not be closed yet.
    if (m_loadWorker.joinable()) {
        m_loadWorker.join();
    }
    m_loadPending = false;

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

void PlayerController::play(const std::filesystem::path &path) {
    auto *plugin = findPluginFor(path);
    if (plugin == nullptr) {
        // No decoder for this extension: report it synchronously (callers gate on isSupported(),
        // so this is a defensive path, but the application layer still surfaces the reason).
        m_playResult = PlayResult::Unsupported;
        return;
    }

    {
        std::scoped_lock lock(m_mutex);
        if (m_activePlugin != nullptr) {
            m_activePlugin->close();
            m_activePlugin = nullptr;
        }
        m_state = PlayerState::STOPPED;
        m_currentPath.clear();
        // Cleared synchronously so an explicit click landing as the current track ends wins over
        // auto-advance: the later consumeTrackEnded() in the same frame then reads false.
        m_trackEnded.store(false);
    }

    // Join a still-running previous load and discard its result. In the common browser-click case
    // the load already finished (the overlay disabled the browser during it), so this is a no-op;
    // only a transport press made during a load makes this block briefly on the parse.
    if (m_loadWorker.joinable()) {
        m_loadWorker.join();
        // That worker may have finished a *successful* open() that update() never reaped (it
        // early-returns while m_loading is set). Close the orphaned module before we overwrite
        // m_loadPlugin, so interrupting a load with another play() never leaks a parsed module.
        // The plugin is inactive (m_activePlugin was nulled above), so this is main-thread teardown.
        if (m_loadPending && m_loadSucceeded) {
            std::scoped_lock lock(m_mutex);
            m_loadPlugin->close();
        }
    }

    m_loadPlugin = plugin;
    m_loadPath = path;
    m_loadPending = true;
    m_loadCancelled = false;
    m_loading.store(true);
    m_loadWorker = std::thread(&PlayerController::loadTrack, this, plugin, path);
}

void PlayerController::loadTrack(PlayerPlugin *plugin, const std::filesystem::path &path) {
    // Runs off m_mutex: the plugin is inactive (play() nulled m_activePlugin) so the audio thread
    // outputs silence and never touches it while it is being parsed.
    const bool ok = plugin->open(path);
    {
        std::scoped_lock lock(m_mutex);
        m_loadSucceeded = ok;
    }
    // Cleared last so update() only observes a completed load after m_loadSucceeded is published.
    m_loading.store(false);
}

void PlayerController::update() {
    if (!m_loadPending || m_loading.load()) {
        return;
    }

    // The worker has finished (m_loading cleared last); join to reap it and establish a
    // happens-before with the m_loadSucceeded write, then consume the outcome.
    if (m_loadWorker.joinable()) {
        m_loadWorker.join();
    }
    const bool succeeded = m_loadSucceeded;
    m_loadPending = false;

    if (m_loadCancelled) {
        // The parse could not be interrupted, so it finished in the background; drop its result.
        // Close the freshly-opened module so it does not linger, start no playback, and produce
        // NO play result (a cancel must not trigger auto-advance).
        m_loadCancelled = false;
        if (succeeded) {
            std::scoped_lock lock(m_mutex);
            m_loadPlugin->close();
        }
        return;
    }

    {
        std::scoped_lock lock(m_mutex);
        if (succeeded) {
            m_activePlugin = m_loadPlugin;
            m_state = PlayerState::PLAYING;
            m_currentPath = m_loadPath;
        } else {
            // A failed open() may leave a half-parsed module; close it so the plugin returns to idle.
            m_loadPlugin->close();
        }
    }
    m_playResult = succeeded ? PlayResult::Ok : PlayResult::DecodeError;
}

bool PlayerController::isLoading() const {
    return m_loadPending;
}

std::optional<PlayResult> PlayerController::consumePlayResult() {
    return std::exchange(m_playResult, std::nullopt);
}

void PlayerController::cancelLoad() {
    if (m_loadPending) {
        m_loadCancelled = true;
    }
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
    // A load can be in flight (the transport bar stays live while the browser overlay is up), so
    // drop its pending result the same way cancelLoad() does: otherwise update() would swap the
    // just-parsed plugin in and resume playing right after the user pressed Stop.
    if (m_loadPending) {
        m_loadCancelled = true;
    }

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

int PlayerController::getSubtrackCount() const {
    std::scoped_lock lock(m_mutex);
    return m_activePlugin != nullptr ? m_activePlugin->getSubtrackCount() : 1;
}

int PlayerController::getCurrentSubtrack() const {
    std::scoped_lock lock(m_mutex);
    return m_activePlugin != nullptr ? m_activePlugin->getCurrentSubtrack() : 0;
}

PlaybackStatus PlayerController::getStatus() const {
    std::scoped_lock lock(m_mutex);
    if (m_activePlugin == nullptr) {
        return {m_state, "", m_currentPath.filename().string(), 0.0, 0.0, 1, 0};
    }
    // A track that ended with no next to auto-advance to stays STOPPED with its plugin still loaded
    // (teardown is main-thread only). Honor PlaybackStatus's "0 when stopped" contract so the timer
    // resets to 0:00 rather than freezing on the finished track's final position/duration.
    const bool stopped = m_state == PlayerState::STOPPED;
    return {
        m_state,
        m_activePlugin->getTitle(),
        m_currentPath.filename().string(),
        stopped ? 0.0 : m_activePlugin->getPosition(),
        stopped ? 0.0 : m_activePlugin->getDuration(),
        m_activePlugin->getSubtrackCount(),
        m_activePlugin->getCurrentSubtrack()
    };
}

TrackMetadata PlayerController::getMetadata() const {
    std::scoped_lock lock(m_mutex);
    return m_activePlugin != nullptr ? m_activePlugin->getMetadata() : TrackMetadata{};
}

bool PlayerController::isSupported(const std::filesystem::path &path) const {
    return findPluginFor(path) != nullptr;
}

bool PlayerController::consumeTrackEnded() {
    return m_trackEnded.exchange(false);
}

std::size_t PlayerController::readLatestAudio(float *out, const std::size_t maxFrames) const {
    // Lock-free seqlock read; intentionally takes no lock (never touches m_mutex).
    return m_audioTap.read(out, maxFrames);
}

void PlayerController::applyPluginSetting(const std::string &pluginName, const std::string &key, const int value) {
    std::scoped_lock lock(m_mutex);
    for (const auto &plugin : m_plugins) {
        if (plugin->getName() == pluginName) {
            plugin->applySetting(key, value);
            return;
        }
    }
}

void PlayerController::selectSubtrack(const int index) {
    std::scoped_lock lock(m_mutex);
    if (m_activePlugin != nullptr) {
        m_activePlugin->selectSubtrack(index);
        // Selecting a subtrack is a play action: resume PLAYING so a switch made after the previous
        // subtrack ended (audio thread set STOPPED) actually starts, mirroring how play() begins.
        m_state = PlayerState::PLAYING;
        // A manual subtrack change must clear any pending end-of-track so it is not clobbered by
        // auto-advance (consistent with how play()/stop() reset m_trackEnded).
        m_trackEnded.store(false);
    }
}

std::vector<std::pair<std::string, std::vector<PluginSetting>>> PlayerController::getPluginSettings() const {
    std::scoped_lock lock(m_mutex);
    std::vector<std::pair<std::string, std::vector<PluginSetting>>> settings;
    settings.reserve(m_plugins.size());
    for (const auto &plugin : m_plugins) {
        settings.emplace_back(plugin->getName(), plugin->getSettings());
    }
    return settings;
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

    const auto frames_wanted = len / static_cast<int>(sizeof(std::int16_t) * CHANNELS);
    auto *buffer = reinterpret_cast<std::int16_t *>(stream);
    const auto frames_written = m_activePlugin->decode(buffer, frames_wanted);

    // Publish only the real decoded frames to the visualization tap, before the
    // end-of-track zero-padding below rewrites the buffer tail. Never blocks the audio thread.
    m_audioTap.publish(buffer, static_cast<std::size_t>(frames_written));

    if (frames_written < frames_wanted) {
        SDL_memset(
            buffer + frames_written * CHANNELS, 0, (frames_wanted - frames_written) * sizeof(std::int16_t) * CHANNELS
        );
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
