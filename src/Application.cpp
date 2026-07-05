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
    : m_player(player), m_fileSystem(fileSystem) {}

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
        m_player.play(m_fileSystem.getPath() / entry.name);
    }
}

// direction: +1 for NEXT, -1 for PREVIOUS. Plays the nearest playable sibling of the current track.
void Application::playAdjacentTrack(const int direction) {
    const auto &entries = m_fileSystem.getContent();
    const auto count = static_cast<int>(entries.size());
    const auto current = m_player.getCurrentPath().filename().string();

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
        if (m_player.play(m_fileSystem.getPath() / entry.name)) {
            return;
        }
    }
}

void Application::update() {
    if (m_player.consumeTrackEnded()) {
        playAdjacentTrack(+1);
    }
}

UiState Application::makeUiState() const {
    return {
        m_player.getCurrentFileName(),
        m_fileSystem.getPath().string(),
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
