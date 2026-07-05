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

#ifndef OSP2_UI_ACTIONS_H
#define OSP2_UI_ACTIONS_H

#include <cstddef>
#include <functional>
#include <string>

#include "ButtonId.h"
#include "Theme.h"
#include "../filesystem/FileEntry.h"


// Callback bundle wired once at startup; the UI reports user intent through these.
struct UiActions {
    std::function<void(ButtonId)> onButtonClick;
    std::function<void(const FileEntry &)> onFileClick;
    std::function<void(const FileEntry &)> onDirectoryClick;
    std::function<void(Theme)> onThemeChange;
    // onPluginSettingChange fires on every edit (apply to the live decoder for immediate feedback);
    // onPluginSettingCommit fires once the widget is released (persist to the INI). Split so dragging
    // a slider does not rewrite the file every frame.
    std::function<void(const std::string &pluginName, const std::string &key, int value)> onPluginSettingChange;
    std::function<void(const std::string &pluginName, const std::string &key, int value)> onPluginSettingCommit;
    // Fired by the browser-overlay Cancel button to abort an in-flight scan/download.
    std::function<void()> onCancelWork;
    // Invoked in VISUALIZATION mode with the reserved rect (screen coords) below the top bar.
    // main.cpp reads the audio tap, builds a VisualFrame, and renders the active visualizer.
    std::function<void(float x, float y, float w, float h)> onRenderVisualization;
    // Fired by the Settings→Visualizer picker with the chosen index; main.cpp calls
    // VisualizerController::select. Gui stays ignorant of the visualizer domain (same as onButtonClick).
    std::function<void(std::size_t index)> onSelectVisualizer;
};


#endif //OSP2_UI_ACTIONS_H
