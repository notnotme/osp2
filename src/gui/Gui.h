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
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ButtonId.h"
#include "Theme.h"
#include "ViewMode.h"
#include "UiActions.h"
#include "UiState.h"
#include "../filesystem/FileEntry.h"
#include "../player/Metadata.h"
#include "../player/PlaybackStatus.h"
#include "../player/PluginSetting.h"
#include "../playlist/PlaylistEntry.h"


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
    // One-frame latch: the top-bar Quit entry sets it, the top bar then opens the confirm popup
    // so OpenPopup and BeginPopupModal share the same menu-bar window ID scope (works in both modes).
    bool m_quitRequested;
    // Text of the currently-shown error popup (empty = none). Latched from the one-frame
    // UiState::error on its rising edge in the menu-bar scope, so the modal persists until Close
    // even after UiState::error goes empty again; cleared by drawErrorPopup's Close button.
    std::string m_errorMessage;
    // Name of the plugin whose settings popup was requested from the Plugins submenu this
    // frame (empty = none); consumed by the top bar to open that plugin's popup by name.
    std::string m_requestedPluginPopup;
    // Working copy backing the open settings popup so sliders bind to stable storage (no one-frame
    // flash from the frame-lagging descriptor cache). m_openSettingsPlugin is the plugin whose popup
    // is currently open (empty = none); it seeds m_settingsEdit (key -> value) once on open.
    std::string m_openSettingsPlugin;
    std::map<std::string, int> m_settingsEdit;
    // Previous frame's file-browser working state; used to detect the rising edge when the loading
    // overlay first appears, so focus is moved to the Cancel button exactly once (see drawFileBrowser).
    bool m_wasWorking = false;
    // Saved GetScrollY() per nav depth: pushed on descend (before zeroing), popped and restored on
    // ascend, so returning to a parent directory lands at the offset it was left at.
    std::vector<float> m_browserScrollStack;
    // Latched navigation signal: drawUserInterface stores the one-frame UiState::navKind here every
    // frame (before the VISUALIZATION early-return), and drawFileBrowser applies + clears it only once
    // the file_browser table is actually laid out — so a scan that lands while the browser is hidden
    // (VISUALIZATION mode, or a culled pane) still moves the scroll stack, keeping it balanced.
    NavKind m_pendingNav = NavKind::None;

public:
    Gui(const Gui &) = delete;
    Gui &operator=(const Gui &) = delete;
    explicit Gui();
    virtual ~Gui() = default;

private:
    void drawTopBar(
        const std::vector<std::pair<std::string, std::vector<PluginSetting>>> &pluginSettings,
        const std::function<void(Theme)> &onThemeChange,
        const std::function<void(const std::string &, const std::string &, int)> &onPluginSettingChange,
        const std::function<void(const std::string &, const std::string &, int)> &onPluginSettingCommit,
        const std::vector<std::string> &visualizerNames,
        std::size_t activeVisualizer,
        const std::function<void(std::size_t)> &onSelectVisualizer,
        const std::function<void(ButtonId)> &onButtonClick,
        const std::string &error
    );
    void drawAboutPopup() const;
    void drawQuitConfirmPopup(const std::function<void(ButtonId)> &onButtonClick) const;
    // Not const: Close clears m_errorMessage (unlike drawAboutPopup, which reads no members).
    void drawErrorPopup();
    void drawPluginPopups(
        const std::vector<std::pair<std::string, std::vector<PluginSetting>>> &pluginSettings,
        const std::function<void(const std::string &, const std::string &, int)> &onPluginSettingChange,
        const std::function<void(const std::string &, const std::string &, int)> &onPluginSettingCommit
    );
    void drawCurrentPath(const std::string &path);
    void drawFileBrowser(
        const std::vector<FileEntry> &files,
        const std::function<void(const FileEntry &)> &onFileClick,
        const std::function<void(const FileEntry &)> &onDirectoryClick,
        bool isWorking,
        const std::string &workingLabel,
        const std::function<void()> &onCancelWork,
        const std::string &playingFileName,
        bool isAtRoot,
        const std::function<void(const FileEntry &)> &onAddToPlaylist
    );
    void drawTabsSection(
        const TrackMetadata &metadata,
        const std::vector<PlaylistEntry> &playlist,
        const std::string &playingFileName,
        bool shuffle,
        bool repeat,
        const std::function<void(std::size_t)> &onRemoveFromPlaylist,
        const std::function<void(std::size_t)> &onPlayPlaylistEntry,
        const std::function<void()> &onToggleShuffle,
        const std::function<void()> &onToggleRepeat
    );
    void drawFileMetadata(const TrackMetadata &metadata);
    void drawModuleMetadata(const ModuleMetadata &metadata);
    void drawGmeMetadata(const GmeMetadata &metadata);
    void drawSidMetadata(const SidMetadata &metadata);
    void drawSc68Metadata(const Sc68Metadata &metadata);
    void drawTabPlaylist(
        const std::vector<PlaylistEntry> &playlist,
        const std::string &playingFileName,
        bool shuffle,
        bool repeat,
        const std::function<void(std::size_t)> &onRemoveFromPlaylist,
        const std::function<void(std::size_t)> &onPlayPlaylistEntry,
        const std::function<void()> &onToggleShuffle,
        const std::function<void()> &onToggleRepeat
    );
    void drawPlayerBar(const PlaybackStatus &status, const std::function<void(ButtonId)> &onButtonClick) const;

public:
    void initialize();
    void finalize();
    void applyTheme(Theme theme);
    void drawUserInterface(const UiState &state, const UiActions &actions);
};


#endif //OSP2_WINDOW_SYSTEM_H
