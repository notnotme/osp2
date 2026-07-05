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

#include <glad/glad.h>
#include <imgui.h>
#include <SDL.h>
#include <SDL_image.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Application.h"
#include "filesystem/DataSource.h"
#include "filesystem/FileSystem.h"
#include "filesystem/FtpDataSource.h"
#include "filesystem/LocalDataSource.h"
#include "gui/Gui.h"
#include "gui/Theme.h"
#include "gui/UiState.h"
#include "input/CursorEmulator.h"
#include "player/PlayerController.h"
#include "settings/Settings.h"
#include "visualizer/VisualFrame.h"
#include "visualizer/VisualizerController.h"

#include <curl/curl.h>

#if defined(__SWITCH__)
    #include <switch.h>
    #define BASE_PATH "romfs:/"
#else
    #define BASE_PATH "romfs/"
#endif


SDL_Window *window;
SDL_GameController *controller;
SDL_GLContext opengl_context;
Gui gui;
FileSystem file_system;
PlayerController player;
VisualizerController visualizer;
Settings settings;
Application app(player, file_system, settings);

// The INI lives next to the executable on desktop (git-ignored build dir); romfs is read-only
// on the Switch, so it goes to writable sdmc storage instead.
std::filesystem::path configPath() {
#if defined(__SWITCH__)
    return "sdmc:/switch/osp2.ini";
#else
    char *base = SDL_GetBasePath();
    if (!base) {
        return "osp2.ini";
    }
    std::filesystem::path path = std::filesystem::path(base) / "osp2.ini";
    SDL_free(base);
    return path;
#endif
}

// Remote sources download to this writable cache root. Mirrors configPath()'s convention:
// sdmc storage on the Switch (romfs is read-only), next to the executable on desktop.
std::filesystem::path cachePath() {
#if defined(__SWITCH__)
    return "sdmc:/switch/OSP2/cache/";
#else
    char *base = SDL_GetBasePath();
    if (!base) {
        return "cache/";
    }
    std::filesystem::path path = std::filesystem::path(base) / "cache/";
    SDL_free(base);
    return path;
#endif
}

void initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        throw std::runtime_error(SDL_GetError());
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        throw std::runtime_error(IMG_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    window = SDL_CreateWindow("OSP 2", 0, 0, 1280, 720, SDL_WINDOW_OPENGL);
    if (!window) {
        throw std::runtime_error(SDL_GetError());
    }

    opengl_context = SDL_GL_CreateContext(window);
    if (!opengl_context) {
        throw std::runtime_error(SDL_GetError());
    }

    // A single owned handle: the quit-on-START handler uses it on every platform, and CursorEmulator
    // opens its own (refcounted) handle for the same controller index on the Switch.
    controller = SDL_GameControllerOpen(0);
    SDL_GL_MakeCurrent(window, opengl_context);
    SDL_GL_SetSwapInterval(1);
    gladLoadGL();

    ImGui::CreateContext();
    if (!ImGui_ImplSDL2_InitForOpenGL(window, opengl_context)) {
        throw std::runtime_error("ImGui_ImplSDL2_InitForOpenGL failed");
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        throw std::runtime_error("ImGui_ImplOpenGL3_Init failed");
    }

    auto &io = ImGui::GetIO();
    io.LogFilename = nullptr;
    io.IniFilename = nullptr;
#if defined(__SWITCH__)
    // The Switch OS draws no cursor, so ImGui must render the emulated one.
    io.MouseDrawCursor = true;
#endif

    ImFontConfig imFontConfig;
    imFontConfig.MergeMode = true;
    imFontConfig.GlyphOffset.y = 2.0f;

    constexpr ImWchar icon_ranges[] = { 0x0030, 0xFFCB, 0 };
    io.Fonts->AddFontFromFileTTF( BASE_PATH "font/Roboto-Regular.ttf", 22.0f);
    io.Fonts->AddFontFromFileTTF(BASE_PATH "font/MaterialSymbolsSharp_Filled-Regular.ttf", 22.0f, &imFontConfig, icon_ranges);
    io.Fonts->Build();

    gui.initialize(BASE_PATH);

    // GL context + glad are up by now: a no-op for the current ImGui-only plugin, correct for future
    // GL plugins that allocate shaders/VBOs in create().
    visualizer.create();

    settings.load(configPath());
    gui.applyTheme(themeFromString(settings.getString("user", "theme", "dark")));

    player.create();

    // Push persisted plugin settings; the INI section is "plugin.<pluginName>". Absent keys keep
    // the plugin's own default (getInt fallback = the descriptor's current value).
    for (const auto &[pluginName, descriptors] : player.getPluginSettings()) {
        const std::string section = "plugin." + pluginName;
        for (const auto &setting : descriptors) {
            player.applyPluginSetting(pluginName, setting.key,
                                      settings.getInt(section, setting.key, setting.value));
        }
    }
    // Seed the Application's cached descriptors with the post-push values (kept off the per-frame path).
    app.refreshPluginSettings();

    // Prefer a hand-edited default_folder when it names a valid directory; otherwise fall back to
    // the compile-time default (sdmc root on Switch, cwd on desktop).
#if defined(__SWITCH__)
    std::filesystem::path start_path = "sdmc:/";
#else
    std::filesystem::path start_path = std::filesystem::current_path();
#endif
    if (const auto default_folder = settings.getString("user", "default_folder", ""); !default_folder.empty()) {
        std::error_code ec;
        if (std::filesystem::is_directory(default_folder, ec)) {
            start_path = default_folder;
        }
    }

    // curl_global_init is not thread-safe: run it before file_system.create() spawns the worker.
    // On the Switch the network stack must be up first, else every transfer fails instantly.
#if defined(__SWITCH__)
    socketInitializeDefault();
#endif
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        // Non-fatal: local browsing still works; remote sources will fail their transfers.
        SDL_Log("curl_global_init failed — remote data sources will be unavailable");
    }

    std::vector<std::unique_ptr<DataSource>> sources;
    sources.push_back(std::make_unique<LocalDataSource>());
    sources.push_back(std::make_unique<FtpDataSource>(
        "Modland (FTP)", "ftp.modland.com", "/pub/modules", cachePath() / "modland"));

    // Register each user-defined [source.NAME] INI section as an extra FTP source. Hand-edit only:
    // these are not seeded and not surfaced in the UI — they simply appear at the virtual root.
    // Cache subdirs must be unique, so no user source collides with the built-in Modland "modland"
    // dir or another user source that sanitizes to the same component (cross-contaminated caches).
    constexpr std::string_view kSourcePrefix = "source.";
    std::set<std::string> takenCacheDirs{"modland"};
    for (const auto &section : settings.getSectionNames(std::string(kSourcePrefix))) {
        const std::string name = section.substr(kSourcePrefix.size());
        const std::string host = settings.getString(section, "host", "");
        if (name.empty() || host.empty()) {
            SDL_Log("main: skipping [%s]: a source needs a non-empty name and host", section.c_str());
            continue;
        }
        const std::string subdir = sanitizeCachePathComponent(name);
        if (!takenCacheDirs.insert(subdir).second) {
            SDL_Log("main: skipping [%s]: cache dir '%s' already in use", section.c_str(), subdir.c_str());
            continue;
        }
        const std::string path = settings.getString(section, "path", "/");
        sources.push_back(std::make_unique<FtpDataSource>(
            name, host, path, cachePath() / subdir));
    }

    file_system.create(std::move(sources), start_path,
        [](const std::filesystem::path &p) { return player.isSupported(p); });
}

void finalize() {
    // Join the worker before tearing down the player: its isPlayable predicate calls into PlayerController.
    file_system.destroy();
    // Worker joined — no curl call in flight. Tear the network stack down last (Switch).
    curl_global_cleanup();
#if defined(__SWITCH__)
    socketExit();
#endif
    player.destroy();
    // Free visualizer GL objects while the GL context is still valid.
    visualizer.destroy();
    gui.finalize();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (controller) {
        SDL_GameControllerClose(controller);
    }
    SDL_GL_DeleteContext(opengl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    IMG_Quit();
}

int main(int argc, char** argv) {
    initialize();

    auto actions = app.makeUiActions();

    // Wired here (not in Application) because the visualizer is a platform-layer concern: the callback
    // reads the audio tap, builds a VisualFrame, and renders the active visualizer inside the ImGui
    // frame. player and visualizer are globals, so no capture is needed.
    actions.onRenderVisualization = [](float x, float y, float w, float h) {
        // Zero-initialized so the frameCount==0 (idle) and partial-read tails are silence, never
        // indeterminate stack — a plugin that reads samples without gating on frameCount stays safe.
        float samples[PlayerController::BUFFER_FRAMES * PlayerController::CHANNELS] = {};
        // decode() publishes nothing when idle, so an ungated read returns the last stale block:
        // gate on PLAYING and pass frameCount 0 otherwise so the visual decays to rest.
        const bool playing = player.getState() == PlayerState::PLAYING;
        const std::size_t frames = playing ? player.readLatestAudio(samples, PlayerController::BUFFER_FRAMES) : 0;
        const VisualFrame frame{x, y, w, h, samples, frames, PlayerController::CHANNELS, PlayerController::SAMPLE_RATE};
        visualizer.render(frame);
    };

    // Settings→Visualizer picker: switch the active visualizer at runtime. visualizer is a global,
    // so no capture is needed. main.cpp is the sole bridge — Gui/Application stay ignorant of the domain.
    actions.onSelectVisualizer = [](std::size_t i) { visualizer.select(i); };

    // Scoped so any CursorEmulator (and the controller it owns) is destroyed here, before finalize()
    // calls SDL_Quit() — SDL_Quit force-frees open controllers, so a later close would be a use-after-free.
    {
#if defined(__SWITCH__)
    // The Switch has no mouse: drive the ImGui cursor from the gamepad. Constructed after the
    // GameController subsystem and ImGui context are up (initialize()); window is 1280x720.
    CursorEmulator cursorEmulator(1280, 720);
#endif

    bool is_running = true;
    while (is_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
                case SDL_KEYDOWN:
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        is_running = false;
                    }
                break;
                case SDL_CONTROLLERBUTTONDOWN:
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                        is_running = false;
                    }
                break;
                case SDL_QUIT:
                    is_running = false;
                default:
                break;
            }
        }

        app.update();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
#if defined(__SWITCH__)
        // Inject the emulated cursor between the SDL backend's NewFrame (which seeds IO) and
        // ImGui::NewFrame (which consumes it) — the ImGui-idiomatic virtual-cursor injection point.
        cursorEmulator.update(ImGui::GetIO());
#endif
        ImGui::NewFrame();
        // Application does not know the visualizer domain, so main.cpp (the bridge) supplies the picker
        // state onto the per-frame view model before handing it to the Gui.
        UiState state = app.makeUiState();
        state.visualizerNames = visualizer.getNames();
        state.activeVisualizer = visualizer.getActiveIndex();
        gui.drawUserInterface(state, actions);
        ImGui::Render();

        auto *draw_data = ImGui::GetDrawData();
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);

        SDL_GL_SwapWindow(window);
    }
    }

    finalize();
    return EXIT_SUCCESS;
}
