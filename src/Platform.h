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

#ifndef OSP2_PLATFORM_H
#define OSP2_PLATFORM_H

#include <filesystem>
#include <memory>
#include <vector>

#include <SDL_video.h>
#include <SDL_gamecontroller.h>

#include "Application.h"
#include "filesystem/DataSource.h"
#include "filesystem/FileSystem.h"
#include "gui/Gui.h"
#include "player/PlayerController.h"
#include "settings/Settings.h"
#include "visualizer/VisualizerController.h"


// Platform / lifecycle layer: owns the SDL/OpenGL/ImGui handles and the process's subsystems, and
// runs the event + render loop. main() just constructs one and calls create()/run()/destroy()
// (formerly the free functions plus file-scope globals in main.cpp). The visualizer bridge lives
// here, not in Application — it is a platform-layer concern. See docs/platform.md.
class Platform final {
private:
    // Window dimensions — shared by SDL_CreateWindow and the Switch CursorEmulator, which clamps
    // its virtual cursor to them.
    static constexpr int kWindowWidth = 1280;
    static constexpr int kWindowHeight = 720;

    // Platform handles.
    SDL_Window *m_window = nullptr;
    SDL_GameController *m_controller = nullptr;
    SDL_GLContext m_glContext = nullptr;

    // Subsystems, owned by value. Declaration order is load-bearing: m_player, m_fileSystem and
    // m_settings precede m_app, whose constructor binds references to them.
    Gui m_gui;
    PlayerController m_player;
    FileSystem m_fileSystem;
    VisualizerController m_visualizer;
    Settings m_settings;
    Application m_app;

public:
    Platform(const Platform &) = delete;
    Platform &operator=(const Platform &) = delete;
    Platform();
    ~Platform() = default;

public:
    void create();
    void run();
    void destroy();

private:
    void initSdlAndGl();
    void initImGui();
    static void loadFonts();
    void initPlayerAndSettings();
    [[nodiscard]] std::filesystem::path resolveStartPath() const;
    void initNetwork();
    [[nodiscard]] std::vector<std::unique_ptr<DataSource>> buildDataSources() const;
};


#endif //OSP2_PLATFORM_H
