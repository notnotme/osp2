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


/**
 * Playback core: owns the SDL audio device and the decoder plugins, dispatching by file extension.
 *
 * The SDL audio thread pulls samples through audioCallback() -> decode() under m_mutex; track loads
 * (plugin->open()) run on a dedicated worker thread; the rest of the public API is called on the main thread.
 * See docs/audio.md for the full threading and format contracts.
 */
class PlayerController final {
public:
    static constexpr int SAMPLE_RATE = 48000;  ///< Output sample rate in Hz (AUDIO_S16SYS, pull-model device).
    static constexpr int CHANNELS = 2;         ///< Interleaved stereo.
    static constexpr int BUFFER_FRAMES = 1024; ///< Frames per audio-callback pull.

private:
    /** Immutable while the audio device is open; safe to read from both threads without locking. */
    std::vector<std::unique_ptr<PlayerPlugin>> m_plugins;
    /** Immutable while the audio device is open; safe to read from both threads without locking. */
    SDL_AudioDeviceID m_device;

    /**
     * Guards the state shared with the SDL audio thread: m_activePlugin, m_state and m_currentPath, plus the
     * reload-continuity fields and m_loadSucceeded below.
     */
    mutable std::mutex m_mutex;
    PlayerPlugin *m_activePlugin;        ///< Shared with the SDL audio thread; guarded by m_mutex.
    PlayerState m_state;                 ///< Shared with the SDL audio thread; guarded by m_mutex.
    std::filesystem::path m_currentPath; ///< Shared with the SDL audio thread; guarded by m_mutex.

    /**
     * Reload continuity (guarded by m_mutex): while play(path) replaces a live track, m_reloadActive stays true
     * and getStatus() serves m_reloadStatus — a snapshot of the outgoing track — so the player bar keeps its
     * title/highlight instead of flashing to STOPPED during the async load. Cleared on swap-in success; a
     * failure/cancel/stop reverts to a genuine STOPPED "No track".
     */
    PlaybackStatus m_reloadStatus{};
    bool m_reloadActive; ///< See m_reloadStatus; guarded by m_mutex.

    std::atomic_bool m_trackEnded; ///< Set by the audio thread at end of track, consumed once by the main loop.

    /**
     * Async decode worker. plugin->open() (a whole-module parse) runs on this worker instead of the UI thread, so
     * a large module never freezes the UI. The plugin being opened is guaranteed inactive (play() nulls
     * m_activePlugin first), so the audio thread outputs silence and the worker touches the plugin off m_mutex;
     * update() swaps it in under m_mutex once done.
     */
    std::thread m_loadWorker;
    std::atomic_bool m_loading;       ///< Worker-running flag; the worker clears it last.
    bool m_loadPending;               ///< Main-thread only: a load is in flight or awaiting swap-in.
    bool m_loadCancelled;             ///< Main-thread only: drop the in-flight load's result.
    PlayerPlugin *m_loadPlugin;       ///< In-flight target plugin.
    std::filesystem::path m_loadPath; ///< In-flight target path.
    bool m_loadSucceeded;             ///< Worker outcome; written under m_mutex, read after join.
    /**
     * Main-thread only: last play() outcome, consumed once. Unsupported is set synchronously in play();
     * Ok/DecodeError are published by update() when it reaps the load worker.
     */
    std::optional<PlayResult> m_playResult;

    /**
     * Lock-free seqlock publishing the just-decoded block to the visualization reader. Deliberately NOT guarded
     * by m_mutex: the audio thread never blocks to publish and the reader never contends the decode lock.
     */
    AudioTap m_audioTap;

public:
    PlayerController(const PlayerController &) = delete;
    PlayerController &operator=(const PlayerController &) = delete;
    explicit PlayerController();
    ~PlayerController() = default;

public:
    /**
     * Constructs the decoder plugins (registration order = dispatch priority on extension overlap) and opens the
     * SDL audio device, which then runs for the whole app lifetime.
     */
    void create();
    /**
     * Tears playback down: joins the load worker, closes the audio device, closes each plugin, then destroys them.
     *
     * The device is closed before any plugin is destroyed, so a plugin destructor can never race decode().
     */
    void destroy();
    /**
     * Starts an asynchronous load of path (findPluginFor + plugin->open() on m_loadWorker).
     *
     * Returns immediately; poll isLoading()/consumePlayResult() and pump update() each frame.
     * A null plugin match is a defensive no-op (callers gate on isSupported()).
     */
    void play(const std::filesystem::path &path);
    /** Resumes a paused track; no-op unless PAUSED. */
    void play();
    /** Pauses playback. Controller state only: the device keeps running and the callback emits silence. */
    void pause();
    /**
     * Stops playback and closes the active track.
     *
     * Also drops an in-flight load's result the same way cancelLoad() does, so a Stop pressed mid-load is
     * honoured instead of being overridden by the load swapping in.
     */
    void stop();
    /**
     * Main thread, once per frame: reaps a finished load worker and, on success, swaps the freshly-opened plugin
     * in under m_mutex (or closes it on failure/cancel).
     */
    void update();

public:
    /** Current playback state; read under m_mutex. */
    [[nodiscard]] PlayerState getState() const;
    /** Basename of the open file; "" when none. Under lock. */
    [[nodiscard]] std::string getCurrentFileName() const;
    /** Full path of the open file; empty when none. Under lock. */
    [[nodiscard]] std::filesystem::path getCurrentPath() const;
    /** Active plugin's track title; "" when no plugin is active. Under lock. */
    [[nodiscard]] std::string getCurrentTitle() const;
    /**
     * Lightweight subtrack navigation query (getStatus() stays the full per-frame UI snapshot).
     *
     * A locked read of the active plugin; the safe default (1) applies when nothing is loaded. Deliberately not
     * served from the reload snapshot: during a reload it reports the no-track default, not the outgoing snapshot.
     */
    [[nodiscard]] int getSubtrackCount() const;
    /**
     * Lightweight subtrack navigation query (getStatus() stays the full per-frame UI snapshot).
     *
     * A locked read of the active plugin; the safe default (0) applies when nothing is loaded. Deliberately not
     * served from the reload snapshot: during a reload it reports the no-track default, not the outgoing snapshot.
     */
    [[nodiscard]] int getCurrentSubtrack() const;
    /**
     * Full per-frame UI snapshot of the loaded track, built under a single lock.
     *
     * While a reload is active it serves the outgoing track's snapshot (m_reloadStatus) so the player bar keeps
     * its title/highlight until update() swaps the replacement in.
     */
    [[nodiscard]] PlaybackStatus getStatus() const;
    /**
     * Cached metadata of the active plugin; monostate when no track is loaded.
     *
     * Locks m_mutex only to read m_activePlugin safely — the value itself is cached during open(), so callers
     * refetch on track change rather than per frame.
     */
    [[nodiscard]] TrackMetadata getMetadata() const;
    /** True when a plugin matches the extension of path. */
    [[nodiscard]] bool isSupported(const std::filesystem::path &path) const;
    /**
     * Returns and clears the end-of-track flag set by the audio thread.
     *
     * Polled once per frame by the main loop to drive auto-advance.
     */
    [[nodiscard]] bool consumeTrackEnded();

    /**
     * True from play() until update() swaps the loaded plugin in (main-thread only).
     *
     * Deliberately tracks m_loadPending, not the m_loading atomic, so the overlay stays up for the swap-in frame
     * and never flickers off for one frame between the worker finishing and playback starting.
     */
    [[nodiscard]] bool isLoading() const;
    /**
     * True while play(path) is replacing a live track (the outgoing snapshot is being served from getStatus()).
     *
     * Lets the app hold the outgoing metadata/subtrack until the swap-in. Under lock.
     */
    [[nodiscard]] bool isReloading() const;
    /**
     * Returns and clears the last play() outcome; nullopt while a load is still in flight or nothing is pending.
     *
     * Ok = playing, Unsupported = no plugin matched the extension, DecodeError = the module failed to parse.
     * Main-thread only.
     */
    [[nodiscard]] std::optional<PlayResult> consumePlayResult();
    /**
     * Marks the in-flight load to be discarded on completion.
     *
     * plugin->open() cannot be interrupted, so the parse still finishes in the background; its result is dropped,
     * the plugin is closed, no playback starts and no play result is produced (a cancel must not trigger
     * auto-advance).
     */
    void cancelLoad();

    /**
     * Copies up to maxFrames of the most recently decoded interleaved-stereo block into out.
     *
     * The tap stores int16 and converts to normalized float on read. Reader-thread safe and lock-free: it never
     * touches m_mutex.
     * @param out destination; must hold maxFrames * CHANNELS floats
     * @return frames copied (0 if nothing has played yet)
     */
    [[nodiscard]] std::size_t readLatestAudio(float *out, std::size_t maxFrames) const;

    /**
     * Applies a setting to a named plugin under m_mutex (same contract as decode/open).
     *
     * pluginName is matched against PlayerPlugin::getName(). No-op if no plugin matches.
     */
    void applyPluginSetting(const std::string &pluginName, const std::string &key, int value);
    /**
     * Selects a subtrack on the active plugin under m_mutex (same contract as applyPluginSetting: it touches the
     * audio-thread-shared decoder).
     *
     * Clears the pending end-of-track flag so a manual subtrack change is not clobbered by auto-advance.
     * No-op when no plugin is active.
     */
    void selectSubtrack(int index);
    /** Plugin name (getName()) + its setting descriptors, for the settings UI. Under lock. */
    [[nodiscard]] std::vector<std::pair<std::string, std::vector<PluginSetting>>> getPluginSettings() const;

private:
    /** SDL audio callback trampoline: forwards to decode() on the SDL audio thread. */
    static void audioCallback(void *userdata, Uint8 *stream, int len);
    /**
     * Audio-thread body: renders one buffer via the active plugin under m_mutex and publishes it to the tap.
     *
     * Emits silence when not PLAYING or when no plugin is active. A short decode zero-pads the tail, flips
     * m_state to STOPPED and sets m_trackEnded — track teardown stays on the main thread.
     */
    void decode(Uint8 *stream, int len);
    /**
     * First registered plugin supporting path's extension (registration order = dispatch priority); nullptr when
     * none matches.
     */
    [[nodiscard]] PlayerPlugin *findPluginFor(const std::filesystem::path &path) const;
    /**
     * Builds the PlaybackStatus snapshot of the loaded track, stamped with stateOverride.
     *
     * STOPPED zeroes position/duration per PlaybackStatus's "0 when stopped" contract. Precondition: the caller
     * holds m_mutex (m_mutex is non-recursive, hence no lock here) and m_activePlugin is non-null.
     */
    [[nodiscard]] PlaybackStatus statusLocked(PlayerState stateOverride) const;
    /**
     * Worker body: parses the module off m_mutex (the plugin is inactive), stores the outcome under m_mutex, then
     * clears m_loading last so update() observes a fully-published result.
     */
    void loadTrack(PlayerPlugin *plugin, const std::filesystem::path &path);
};


#endif //OSP2_PLAYER_CONTROLLER_H
