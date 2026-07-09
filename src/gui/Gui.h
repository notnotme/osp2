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

#ifndef OSP2_GUI_H
#define OSP2_GUI_H

#include <glad/glad.h>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "ButtonId.h"
#include "Theme.h"
#include "ViewMode.h"
#include "UiActions.h"
#include "UiState.h"
#include "../player/Metadata.h"
#include "../player/PlaybackStatus.h"


/**
 * Presentation layer: renders the whole UI each frame from an immutable UiState snapshot and reports user intent
 * only through the UiActions callbacks.
 *
 * Holds presentation state exclusively — the sprite-atlas texture, current theme, view mode, and popup/settings-edit
 * latches — never application or domain state; it never touches the player or filesystem directly.
 */
class Gui final {
private:
    /** Sprite-atlas entry parsed from sprites.bin: a UV rectangle into the atlas texture plus the pixel size. */
    struct Sprite {
        float s;   ///< Top-left UV, horizontal.
        float t;   ///< Top-left UV, vertical.
        float p;   ///< Bottom-right UV, horizontal.
        float q;   ///< Bottom-right UV, vertical.
        int16_t w; ///< Sprite width in pixels.
        int16_t h; ///< Sprite height in pixels.
    };

    std::unordered_map<std::string, Sprite> m_sprites; ///< Atlas entries loaded from sprites.bin, keyed by name.
    GLuint m_texture;    ///< Atlas GL texture; created in initialize(), deleted in finalize().
    Theme m_theme;       ///< Current color theme; drives the Settings-menu checkmark.
    ViewMode m_viewMode; ///< Current view mode, flipped by the top-bar toggle.
    /**
     * One-frame latch: the top-bar About entry sets it, the top bar then opens the popup so OpenPopup and
     * BeginPopupModal share the same window ID scope (works in both modes).
     */
    bool m_aboutRequested;
    /**
     * One-frame latch: the top-bar Quit entry sets it, the top bar then opens the confirm popup so OpenPopup and
     * BeginPopupModal share the same menu-bar window ID scope (works in both modes).
     */
    bool m_quitRequested;
    /**
     * Text of the currently-shown error popup (empty = none).
     *
     * Latched from the one-frame UiState::error on its rising edge in the menu-bar scope, so the modal persists
     * until Close even after UiState::error goes empty again; cleared by drawErrorPopup's Close button.
     */
    std::string m_errorMessage;
    /**
     * Name of the plugin whose settings popup was requested from the Plugins submenu this frame (empty = none);
     * consumed by the top bar to open that plugin's popup by name.
     */
    std::string m_requestedPluginPopup;
    /**
     * Working copy backing the open settings popup so sliders bind to stable storage (no one-frame flash from the
     * frame-lagging descriptor cache).
     *
     * m_openSettingsPlugin is the plugin whose popup is currently open (empty = none); it seeds m_settingsEdit
     * (key -> value) once on open.
     */
    std::string m_openSettingsPlugin;
    std::map<std::string, int> m_settingsEdit; ///< Key -> value working copy for the open settings popup.
    /**
     * Previous frame's file-browser working state; used to detect the rising edge when the loading overlay first
     * appears, so focus is moved to the Cancel button exactly once (see drawFileBrowser).
     */
    bool m_wasWorking = false;
    /**
     * Saved GetScrollY() per nav depth: pushed on descend (before zeroing), popped and restored on ascend, so
     * returning to a parent directory lands at the offset it was left at.
     */
    std::vector<float> m_browserScrollStack;
    /**
     * Latched navigation signal: drawUserInterface stores the one-frame UiState::navKind here every frame (before
     * the VISUALIZATION early-return), and drawFileBrowser applies + clears it only once the file_browser table is
     * actually laid out — so a scan that lands while the browser is hidden (VISUALIZATION mode, or a culled pane)
     * still moves the scroll stack, keeping it balanced.
     */
    NavKind m_pendingNav = NavKind::None;

public:
    Gui(const Gui &) = delete;
    Gui &operator=(const Gui &) = delete;
    explicit Gui();

private:
    void drawTopBar(const UiState &state, const UiActions &actions);
    void drawAboutPopup() const;
    void drawQuitConfirmPopup(const std::function<void(ButtonId)> &onButtonClick) const;
    /**
     * Draws the playback-error modal.
     *
     * Not const: Close clears m_errorMessage (unlike drawAboutPopup, which reads no members).
     */
    void drawErrorPopup();
    void drawPluginPopups(const UiState &state, const UiActions &actions);
    void drawCurrentPath(const std::string &path);
    /**
     * Draws the file-browser table.
     *
     * @param playingFileName derived once per frame in drawUserInterface (empty when stopped); drives only the
     * browser highlight — the playlist tab keys on UiState::playingPlaylistIndex instead
     */
    void drawFileBrowser(const UiState &state, const UiActions &actions, const std::string &playingFileName);
    void drawTabsSection(const UiState &state, const UiActions &actions);
    void drawFileMetadata(const TrackMetadata &metadata);
    void drawModuleMetadata(const ModuleMetadata &metadata);
    void drawGmeMetadata(const GmeMetadata &metadata);
    void drawSidMetadata(const SidMetadata &metadata);
    void drawSc68Metadata(const Sc68Metadata &metadata);
    void drawTabPlaylist(const UiState &state, const UiActions &actions);
    void drawPlayerBar(const PlaybackStatus &status, const std::function<void(ButtonId)> &onButtonClick) const;

public:
    /**
     * Loads the sprite atlas (romfs/sprites/sprites.bin + sprites.png) into one GL texture, sets the
     * theme-independent style metrics once, and applies the dark default palette.
     */
    void initialize();
    /** Releases the sprite-atlas GL texture. */
    void finalize();
    /**
     * Swaps the ImGui palette to the given theme and records it (drives the Settings-menu checkmark).
     *
     * Colors only — style metrics are untouched, so it is safe to call live from the menu.
     */
    void applyTheme(Theme theme);
    /**
     * Renders the whole UI for the current frame from the immutable snapshot, reporting user intent through the
     * callbacks.
     *
     * In WORKSPACE mode draws the top bar, the browser and tabs panes, and the player bar; in VISUALIZATION mode
     * draws the top bar only and hands the remaining work area to actions.onRenderVisualization.
     */
    void drawUserInterface(const UiState &state, const UiActions &actions);
};


#endif //OSP2_GUI_H
