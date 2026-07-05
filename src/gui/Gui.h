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

#ifndef OSP2_WINDOW_SYSTEM_H
#define OSP2_WINDOW_SYSTEM_H

#include <glad/glad.h>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ButtonId.h"
#include "Theme.h"
#include "ViewMode.h"
#include "UiActions.h"
#include "UiState.h"
#include "../filesystem/FileEntry.h"
#include "../player/PlaybackStatus.h"


class Gui {
private:
    struct Sprite {
        float s;
        float t;
        float p;
        float q;
        int16_t w;
        int16_t h;
    };

    std::unordered_map<std::string, Sprite> m_sprites;
    GLuint m_texture;
    Theme m_theme;
    ViewMode m_viewMode;
    // One-frame latch: the top-bar About entry sets it, the top bar then opens the popup
    // so OpenPopup and BeginPopupModal share the same window ID scope (works in both modes).
    bool m_aboutRequested;

public:
    Gui(const Gui &) = delete;
    Gui &operator=(const Gui &) = delete;
    explicit Gui();
    virtual ~Gui() = default;

private:
    void drawTopBar();
    void drawAboutPopup();
    void drawCurrentPath(const std::string &path);
    void drawFileBrowser(const std::vector<FileEntry> &files, const std::function<void(const FileEntry &)> &onFileClick, bool isWorking);
    void drawTabsSection();
    void drawFileMetadata();
    void drawTabPlaylist();
    void drawPlayerBar(const PlaybackStatus &status, const std::function<void(ButtonId)> &onButtonClick);

public:
    void initialize(const std::string &basePath);
    void finalize();
    void applyTheme(Theme theme);
    void drawUserInterface(const UiState &state, const UiActions &actions);
};


#endif //OSP2_WINDOW_SYSTEM_H