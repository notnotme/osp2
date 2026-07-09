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
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "AudioTap.h"
#include "Metadata.h"
#include "PlaybackStatus.h"
#include "PlayerPlugin.h"
#include "PlayResult.h"
#include "PluginSetting.h"
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

    // Reload continuity (guarded by m_mutex): while play(path) replaces a live track, m_reloadActive
    // stays true and getStatus() serves m_reloadStatus — a snapshot of the outgoing track — so the
    // player bar keeps its title/highlight instead of flashing to STOPPED during the async load.
    // Cleared on swap-in success; a failure/cancel/stop reverts to a genuine STOPPED "No track".
    PlaybackStatus m_reloadStatus{};
    bool m_reloadActive;

    // Set by the audio thread at end of track, consumed once by the main loop.
    std::atomic_bool m_trackEnded;

    // Async decode. plugin->open() (a whole-module parse) runs on this worker instead of the UI
    // thread, so a large module never freezes the UI. The plugin being opened is guaranteed
    // inactive (play() nulls m_activePlugin first), so the audio thread outputs silence and the
    // worker touches the plugin off m_mutex; update() swaps it in under m_mutex once done.
    std::thread m_loadWorker;
    std::atomic_bool m_loading;       // worker-running flag; the worker clears it last.
    bool m_loadPending;               // main-thread only: a load is in flight or awaiting swap-in.
    bool m_loadCancelled;             // main-thread only: drop the in-flight load's result.
    PlayerPlugin *m_loadPlugin;       // in-flight target plugin.
    std::filesystem::path m_loadPath; // in-flight target path.
    bool m_loadSucceeded;             // worker outcome; written under m_mutex, read after join.
    // Main-thread only: last play() outcome, consumed once. Unsupported is set synchronously in
    // play(); Ok/DecodeError are published by update() when it reaps the load worker.
    std::optional<PlayResult> m_playResult;

    // Lock-free seqlock publishing the just-decoded block to the visualization reader.
    // Deliberately NOT guarded by m_mutex: the audio thread never blocks to publish and
    // the reader never contends the decode lock.
    AudioTap m_audioTap;

public:
    PlayerController(const PlayerController &) = delete;
    PlayerController &operator=(const PlayerController &) = delete;
    explicit PlayerController();
    ~PlayerController() = default;

public:
    void create();
    void destroy();
    // Starts an asynchronous load of path (findPluginFor + plugin->open() on m_loadWorker).
    // Returns immediately; poll isLoading()/consumePlayResult() and pump update() each frame.
    // A null plugin match is a defensive no-op (callers gate on isSupported()).
    void play(const std::filesystem::path &path);
    void play();
    void pause();
    void stop();
    // Main thread, once per frame: reaps a finished load worker and, on success, swaps the
    // freshly-opened plugin in under m_mutex (or closes it on failure/cancel).
    void update();

public:
    [[nodiscard]] PlayerState getState() const;
    [[nodiscard]] std::string getCurrentFileName() const;
    [[nodiscard]] std::filesystem::path getCurrentPath() const;
    [[nodiscard]] std::string getCurrentTitle() const;
    // Lightweight subtrack navigation queries (getStatus() stays the full per-frame UI snapshot).
    // A locked read of the active plugin; the safe defaults (1 / 0) apply when nothing is loaded.
    [[nodiscard]] int getSubtrackCount() const;
    [[nodiscard]] int getCurrentSubtrack() const;
    [[nodiscard]] PlaybackStatus getStatus() const;
    [[nodiscard]] TrackMetadata getMetadata() const;
    [[nodiscard]] bool isSupported(const std::filesystem::path &path) const;
    [[nodiscard]] bool consumeTrackEnded();

    // True from play() until update() swaps the loaded plugin in (main-thread only). Deliberately
    // tracks m_loadPending, not the m_loading atomic, so the overlay stays up for the swap-in frame
    // and never flickers off for one frame between the worker finishing and playback starting.
    [[nodiscard]] bool isLoading() const;
    // True while play(path) is replacing a live track (the outgoing snapshot is being served from
    // getStatus()); lets the app hold the outgoing metadata/subtrack until the swap-in. Under lock.
    [[nodiscard]] bool isReloading() const;
    // Returns and clears the last play() outcome (Ok = playing, Unsupported = no plugin matched the
    // extension, DecodeError = the module failed to parse); nullopt while a load is still in flight
    // or nothing is pending. Main-thread only.
    [[nodiscard]] std::optional<PlayResult> consumePlayResult();
    // Marks the in-flight load to be discarded on completion. plugin->open() cannot be interrupted,
    // so the parse still finishes in the background; its result is dropped, the plugin is closed,
    // no playback starts and no play result is produced (a cancel must not trigger auto-advance).
    void cancelLoad();

    // Copies up to maxFrames of the most recently decoded interleaved-stereo block into out
    // (which must hold maxFrames * CHANNELS floats); returns frames copied (0 if nothing has
    // played yet). The tap stores int16 and converts to normalized float on read.
    // Reader-thread safe and lock-free: it never touches m_mutex.
    [[nodiscard]] std::size_t readLatestAudio(float *out, std::size_t maxFrames) const;

    // Applies a setting to a named plugin under m_mutex (same contract as decode/open).
    // pluginName is matched against PlayerPlugin::getName(). No-op if no plugin matches.
    void applyPluginSetting(const std::string &pluginName, const std::string &key, int value);
    // Selects a subtrack on the active plugin under m_mutex (same contract as applyPluginSetting:
    // it touches the audio-thread-shared decoder). Clears the pending end-of-track flag so a manual
    // subtrack change is not clobbered by auto-advance. No-op when no plugin is active.
    void selectSubtrack(int index);
    // Plugin name (getName()) + its setting descriptors, for the settings UI. Under lock.
    [[nodiscard]] std::vector<std::pair<std::string, std::vector<PluginSetting>>> getPluginSettings() const;

private:
    static void audioCallback(void *userdata, Uint8 *stream, int len);
    void decode(Uint8 *stream, int len);
    [[nodiscard]] PlayerPlugin *findPluginFor(const std::filesystem::path &path) const;
    // Builds the PlaybackStatus snapshot of the loaded track, stamped with stateOverride; STOPPED
    // zeroes position/duration per PlaybackStatus's "0 when stopped" contract. Precondition: the
    // caller holds m_mutex (m_mutex is non-recursive, hence no lock here) and m_activePlugin is
    // non-null.
    [[nodiscard]] PlaybackStatus statusLocked(PlayerState stateOverride) const;
    // Worker body: parses the module off m_mutex (the plugin is inactive), stores the outcome
    // under m_mutex, then clears m_loading last so update() observes a fully-published result.
    void loadTrack(PlayerPlugin *plugin, const std::filesystem::path &path);
};


#endif //OSP2_PLAYER_CONTROLLER_H
