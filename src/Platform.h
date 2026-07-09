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
#include "playlist/PlayList.h"
#include "settings/Settings.h"
#include "visualizer/VisualizerController.h"


/**
 * Platform / lifecycle layer: owns the SDL/OpenGL/ImGui handles and the process's subsystems, and runs the
 * event + render loop.
 *
 * main() just constructs one and calls create()/run()/destroy(). See docs/platform.md.
 */
class Platform final {
private:
    /**
     * Window dimensions — shared by SDL_CreateWindow and the Switch CursorEmulator, which clamps its virtual
     * cursor to them.
     */
    static constexpr int kWindowWidth = 1280;
    static constexpr int kWindowHeight = 720;

    // Platform handles.
    SDL_Window *m_window = nullptr;             ///< The application window (kWindowWidth x kWindowHeight).
    SDL_GameController *m_controller = nullptr; ///< Used by the quit-on-START handler; nullptr when none is connected.
    SDL_GLContext m_glContext = nullptr;        ///< OpenGL 4.3 core context.

    /**
     * Subsystems, owned by value. Declaration order is load-bearing: m_player, m_fileSystem, m_visualizer,
     * m_settings and m_playList precede m_app, whose constructor binds references to them.
     */
    Gui m_gui;
    PlayerController m_player;
    FileSystem m_fileSystem;
    VisualizerController m_visualizer;
    Settings m_settings;
    PlayList m_playList;
    Application m_app;

public:
    Platform(const Platform &) = delete;
    Platform &operator=(const Platform &) = delete;
    Platform();
    ~Platform() = default;

public:
    /**
     * Brings up SDL/OpenGL/ImGui and every subsystem. Main thread, once, before run().
     *
     * The internal call order is load-bearing: initNetwork() runs first (curl_global_init is not thread-safe,
     * and the Switch needs socketInitializeDefault() — both must precede the FileSystem worker spawn), and the
     * GL context must be up before VisualizerController::create() allocates shaders. Exceptions are left
     * uncaught: init failure terminates the process.
     */
    void create();
    /**
     * Runs the event + render loop until quit (ESC, controller START, or SDL_QUIT).
     *
     * Builds the UiActions bundle once and wraps onButtonClick to intercept QUIT (Platform owns the run-loop
     * flag; every other button is delegated to Application). Each frame: poll SDL events, m_app.update(), then
     * makeUiState() -> m_gui.drawUserInterface() -> ImGui render + swap.
     */
    void run();
    /**
     * Tears everything down. Main thread, once, after run().
     *
     * Teardown order is load-bearing: the FileSystem worker joins first (its isPlayable predicate calls into
     * PlayerController), network cleanup follows once no curl call can be in flight, and the visualizer frees
     * its GL objects while the context is still valid.
     */
    void destroy();

private:
    void initSdlAndGl();
    void initImGui();
    /**
     * Merges the font stack into ImGui's default atlas: Roboto (Latin base), Material Symbols (UI icon
     * glyphs), and a pre-subset Noto Sans JP (CJK) so decoder metadata renders as glyphs instead of tofu.
     */
    static void loadFonts();
    void initPlayerAndSettings();
    /**
     * Start directory for the browser: a hand-edited [user] default_folder when it names a valid directory,
     * otherwise the platform default (the sdmc root on Switch, the current working directory on desktop).
     */
    [[nodiscard]] std::filesystem::path resolveStartPath() const;
    void initNetwork();
    /**
     * Builds the browser's sources: local storage, the built-in Modland FTP source, plus one FTP source per
     * hand-edited [source.NAME] INI section. Cache subdirectories are kept unique; colliding or incomplete
     * sections are skipped with a log.
     */
    [[nodiscard]] std::vector<std::unique_ptr<DataSource>> buildDataSources() const;
};


#endif //OSP2_PLATFORM_H
