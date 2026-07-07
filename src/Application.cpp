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

#include "Application.h"

#include "Paths.h"
#include "player/PlayerState.h"


Application::Application(PlayerController &player, FileSystem &fileSystem, Settings &settings)
    : m_player(player),
      m_fileSystem(fileSystem),
      m_settings(settings),
      m_advanceDirection(0) {}

void Application::handleButtonClick(const ButtonId buttonId) {
    switch (buttonId) {
    case PLAY_PAUSE:
        switch (m_player.getState()) {
        case PlayerState::PLAYING:
            m_player.pause();
            break;
        case PlayerState::PAUSED:
            m_player.play();
            break;
        case PlayerState::STOPPED:
            // TODO(temporary): hardcoded test track until FileSystem returns real directories.
            m_player.play(assetPath("music/test.s3m"));
            break;
        }
        break;
    case STOP:
        m_player.stop();
        break;
    case NEXT:
        playAdjacentTrack(+1);
        break;
    case PREVIOUS:
        playAdjacentTrack(-1);
        break;
    case QUIT:
        // Intercepted by Platform (it owns the run-loop flag); never reaches the app layer.
        break;
    }
}

void Application::handleFileClick(const FileEntry &entry) {
    if (m_player.isSupported(entry.name)) {
        m_advanceDirection = 0;
        m_fileSystem.requestFile(entry);
    }
}

void Application::handleDirectoryClick(const FileEntry &entry) {
    // No path joining here: at the virtual root, entry.name is a source display name, not a
    // path component — FileSystem interprets it against the active source (or lack of one).
    if (entry.name == "..") {
        m_fileSystem.navigateToParent();
    } else {
        m_fileSystem.navigateToEntry(entry);
    }
}

// direction: +1 for NEXT, -1 for PREVIOUS. Requests the first playable sibling of the current track;
// success is decided later at the consume site, which retries via this cursor if the fetch fails.
void Application::playAdjacentTrack(const int direction) {
    const auto &entries = m_fileSystem.getContent();
    const auto count = static_cast<int>(entries.size());
    const auto current =
        m_lastRequestedName.empty() ? m_player.getCurrentPath().filename().string() : m_lastRequestedName;

    auto index = -1;
    for (int i = 0; i < count; ++i) {
        if (!entries[i].is_directory && entries[i].name == current) {
            index = i;
            break;
        }
    }

    for (int i = index + direction; i >= 0 && i < count; i += direction) {
        const auto &entry = entries[i];
        if (entry.is_directory || !m_player.isSupported(entry.name)) {
            continue;
        }
        m_advanceDirection = direction;
        m_lastRequestedName = entry.name;
        m_fileSystem.requestFile(entry);
        return;
    }

    // No candidate in this direction: drop the retry cursor so a later NEXT/PREVIOUS
    // resolves against the actually-playing track, not the last failed name.
    m_lastRequestedName.clear();
}

// The Gui already applied the theme visually (it owns the ImGui style); here we only persist it.
void Application::handleThemeChange(const Theme theme) {
    m_settings.setString("user", "theme", themeToString(theme));
    m_settings.save();
}

// Live edit (fires every frame while a slider is dragged, or once per combo change): apply to the
// decoder for immediate audio preview, but do NOT persist. Also patch the cached descriptor's value
// in place so m_pluginSettings tracks the live decoder value — this is what a reopened popup seeds
// from. The in-place int write reallocates nothing and is safe even though makeUiState hands
// m_pluginSettings to the Gui: during the popup the Gui reads its own working copy and only reads
// the cache when seeding on open, before any change fires that frame.
void Application::handlePluginSettingChange(const std::string &pluginName, const std::string &key, const int value) {
    m_player.applyPluginSetting(pluginName, key, value);
    for (auto &[name, descriptors] : m_pluginSettings) {
        if (name == pluginName) {
            for (auto &d : descriptors) {
                if (d.key == key) {
                    d.value = value;
                    break;
                }
            }
        }
    }
}

// Commit (fired per setting by the popup's Save button): the decoder already holds the value from
// the live edits, so only persist it to the INI.
void Application::handlePluginSettingCommit(const std::string &pluginName, const std::string &key, const int value) {
    m_settings.setInt("plugin." + pluginName, key, value);
    m_settings.save();
}

void Application::handleCancelWork() {
    m_fileSystem.cancel();
    // Cancel covers both stages the overlay can be showing: a download (FileSystem) and a decode
    // (PlayerController). The player parse cannot be interrupted, so its result is merely dropped.
    m_player.cancelLoad();
    // A cancelled download must NOT auto-advance to the next sibling: drop the advance intent so the
    // resulting failed FetchResult (direction 0) just logs instead of kicking off another download.
    m_advanceDirection = 0;
    m_lastRequestedName.clear();
}

// Builds the cached plugin-setting descriptors from the player (locks the audio mutex, so it is
// called only at settled points — once from main.cpp after the startup push — never per frame and
// never during a draw). Live edits keep the cache current in place, so no rebuild is needed after.
void Application::refreshPluginSettings() {
    m_pluginSettings = m_player.getPluginSettings();
}

void Application::update() {
    m_fileSystem.update();
    m_pendingNav = m_fileSystem.consumeNavigation();
    // Reap a finished async load: on success it swaps the decoded plugin in and publishes a
    // play result (consumed below); the "Loading..." overlay stays up until this swap-in.
    m_player.update();

    // Resolve a fetched file: play() is now asynchronous (it starts the decode on a player worker
    // and returns), so success/failure is decided later at the consumePlayResult() poll below.
    // play() still clears the track-ended flag synchronously, so an explicit click made just as the
    // current track ends wins over auto-advance rather than being clobbered by it in the same frame.
    if (const auto r = m_fileSystem.consumeFetchResult()) {
        if (r->succeeded) {
            m_player.play(r->localPath); // async; the retry cursor is dropped once it succeeds
        } else if (m_advanceDirection != 0) {
            playAdjacentTrack(m_advanceDirection); // skip a broken sibling (fetch failure)
        }
        // Direct-click fetch failure (direction 0): SDL_Log inside the filesystem is enough.
    }

    // Poll the async decode outcome: on success drop the retry cursor, on a decode failure skip a
    // broken sibling exactly as a failed fetch does (a cancel produces no result, so it is silent).
    if (const auto ok = m_player.consumePlayResult()) {
        if (*ok) {
            m_lastRequestedName.clear();
        } else if (m_advanceDirection != 0) {
            playAdjacentTrack(m_advanceDirection); // skip a broken sibling (decode failure)
        }
    }

    if (m_player.consumeTrackEnded()) {
        playAdjacentTrack(+1);
    }

    // Refetch metadata only when the playing path changes (manual play, auto-advance, stop),
    // not per frame — getMetadata() locks the audio mutex. A cleared path resets to monostate.
    if (const auto path = m_player.getCurrentPath(); path != m_metadataPath) {
        m_metadataPath = path;
        m_trackMetadata = m_player.getMetadata();
    }
}

UiState Application::makeUiState() const {
    const auto &path = m_fileSystem.getPath();

    // Read each working flag once so a flag and its label can't disagree if a worker finishes
    // mid-build. The player's decode load reuses the same browser overlay as the filesystem, with a
    // distinct label taking priority (a decode always follows the fetch that fed it).
    const bool loading = m_player.isLoading();
    const bool fsWorking = m_fileSystem.isWorking();
    const bool working = loading || fsWorking;
    std::string workingLabel;
    if (loading) {
        workingLabel = "Loading...";
    } else if (fsWorking) {
        workingLabel = m_fileSystem.isFetching() ? "Downloading..." : "Scanning...";
    }

    return {
        m_player.getStatus(),
        path.empty() ? "Sources" : path.string(),
        m_fileSystem.getContent(),
        working,
        std::move(workingLabel),
        m_trackMetadata,
        m_pluginSettings,
        m_pendingNav
    };
}

UiActions Application::makeUiActions() {
    return {
        [this](const ButtonId buttonId) { handleButtonClick(buttonId); },
        [this](const FileEntry &entry) { handleFileClick(entry); },
        [this](const FileEntry &entry) { handleDirectoryClick(entry); },
        [this](const Theme theme) { handleThemeChange(theme); },
        [this](const std::string &pluginName, const std::string &key, const int value) {
            handlePluginSettingChange(pluginName, key, value);
        },
        [this](const std::string &pluginName, const std::string &key, const int value) {
            handlePluginSettingCommit(pluginName, key, value);
        },
        [this]() { handleCancelWork(); }
    };
}
