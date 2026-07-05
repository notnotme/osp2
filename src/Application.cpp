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

#include "player/PlayerState.h"

#if defined(__SWITCH__)
    #define BASE_PATH "romfs:/"
#else
    #define BASE_PATH "romfs/"
#endif


Application::Application(PlayerController &player, FileSystem &fileSystem)
    : m_player(player), m_fileSystem(fileSystem), m_advanceDirection(0) {}

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
                    m_player.play(BASE_PATH "music/test.s3m");
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
    }
}

void Application::handleFileClick(const FileEntry &entry) {
    if (m_player.isSupported(entry.name)) {
        m_advanceDirection = 0;
        m_fileSystem.requestFile(entry);
    }
}

// direction: +1 for NEXT, -1 for PREVIOUS. Requests the first playable sibling of the current track;
// success is decided later at the consume site, which retries via this cursor if the fetch fails.
void Application::playAdjacentTrack(const int direction) {
    const auto &entries = m_fileSystem.getContent();
    const auto count = static_cast<int>(entries.size());
    const auto current = m_lastRequestedName.empty()
        ? m_player.getCurrentPath().filename().string()
        : m_lastRequestedName;

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

void Application::update() {
    m_fileSystem.update();

    // Resolve a pending request first: a successful play() clears the track-ended flag,
    // so an explicit click made just as the current track ends wins over auto-advance
    // (rather than being clobbered by it when both land in the same frame).
    if (auto r = m_fileSystem.consumeFetchResult()) {
        if (r->succeeded && m_player.play(r->localPath)) {
            m_lastRequestedName.clear();
        } else if (m_advanceDirection != 0) {
            playAdjacentTrack(m_advanceDirection);   // skip a broken sibling
        }
        // Direct-click failure (direction 0): SDL_Log inside the player is enough.
    }

    if (m_player.consumeTrackEnded()) {
        playAdjacentTrack(+1);
    }
}

UiState Application::makeUiState() const {
    const auto &path = m_fileSystem.getPath();
    return {
        m_player.getStatus(),
        path.empty() ? "Sources" : path.string(),
        m_fileSystem.getContent(),
        m_fileSystem.isWorking()
    };
}

UiActions Application::makeUiActions() {
    return {
        [this](const ButtonId buttonId) { handleButtonClick(buttonId); },
        [this](const FileEntry &entry) { handleFileClick(entry); }
    };
}
