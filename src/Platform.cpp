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

#include "Platform.h"

#include <glad/glad.h>
#include <imgui.h>
#include <SDL.h>
#include <SDL_image.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

#include <curl/curl.h>

#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(__SWITCH__)
#include <switch.h>
#endif

#include "Paths.h"
#include "filesystem/FtpDataSource.h"
#include "filesystem/LocalDataSource.h"
#include "gui/Theme.h"
#include "gui/UiState.h"
#include "input/CursorEmulator.h"
#include "player/PlayerState.h"
#include "visualizer/VisualFrame.h"


Platform::Platform()
    : m_app(m_player, m_fileSystem, m_settings) {}

void Platform::create() {
    initNetwork(); // before m_fileSystem.create() spawns the worker
    initSdlAndGl();
    initImGui();

    // GL context + glad are up by now: a no-op for the current ImGui-only plugin, correct for
    // future GL plugins that allocate shaders/VBOs in create().
    m_visualizer.create();
    initPlayerAndSettings();

    const std::filesystem::path start_path = resolveStartPath();
    m_fileSystem.create(buildDataSources(), start_path, [this](const std::filesystem::path &p) {
        return m_player.isSupported(p);
    });
}

void Platform::run() {
    auto actions = m_app.makeUiActions();

    // Wired here (not in Application) because the visualizer is a platform-layer concern: the callback
    // reads the audio tap, builds a VisualFrame, and renders the active visualizer inside the ImGui
    // frame. Captures this — Platform outlives the loop.
    actions.onRenderVisualization = [this](const float x, const float y, const float w, const float h) {
        // Zero-initialized so the frameCount==0 (idle) and partial-read tails are silence, never
        // indeterminate stack — a plugin that reads samples without gating on frameCount stays safe.
        float samples[PlayerController::BUFFER_FRAMES * PlayerController::CHANNELS] = {};
        // decode() publishes nothing when idle, so an ungated read returns the last stale block:
        // gate on PLAYING and pass frameCount 0 otherwise so the visual decays to rest.
        const bool playing = m_player.getState() == PlayerState::PLAYING;
        const std::size_t frames = playing ? m_player.readLatestAudio(samples, PlayerController::BUFFER_FRAMES) : 0;
        const VisualFrame frame{x, y, w, h, samples, frames, PlayerController::CHANNELS, PlayerController::SAMPLE_RATE};
        m_visualizer.render(frame);
    };

    // Settings→Visualizer picker: switch the active visualizer at runtime, then persist the choice as
    // its stable plugin name (mirrors theme). Platform is the sole bridge — Gui/Application stay
    // ignorant of the domain.
    actions.onSelectVisualizer = [this](const std::size_t i) {
        const std::vector<std::string> names = m_visualizer.getNames();
        if (i < names.size()) {
            m_visualizer.select(i);
            m_settings.setString("user", "visualizer", names[i]);
            m_settings.save();
        }
    };

#if defined(__SWITCH__)
    // The Switch has no mouse: drive the ImGui cursor from the gamepad. A local so it (and the
    // controller it owns) is destroyed when run() returns — before destroy() calls SDL_Quit, which
    // force-frees open controllers, so a later close would be a use-after-free. Window is 1280x720.
    CursorEmulator cursorEmulator(kWindowWidth, kWindowHeight);
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
                break;
            default:
                break;
            }
        }

        m_app.update();

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
        // Application does not know the visualizer domain, so Platform (the bridge) supplies the
        // picker state onto the per-frame view model before handing it to the Gui.
        UiState state = m_app.makeUiState();
        state.visualizerNames = m_visualizer.getNames();
        state.activeVisualizer = m_visualizer.getActiveIndex();
        m_gui.drawUserInterface(state, actions);
        ImGui::Render();

        auto *draw_data = ImGui::GetDrawData();
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);

        SDL_GL_SwapWindow(m_window);
    }
}

void Platform::destroy() {
    // Join the worker before tearing down the player: its isPlayable predicate calls into PlayerController.
    m_fileSystem.destroy();
    // Worker joined — no curl call in flight. Tear the network stack down last (Switch).
    curl_global_cleanup();
#if defined(__SWITCH__)
    socketExit();
#endif
    m_player.destroy();
    // Free visualizer GL objects while the GL context is still valid.
    m_visualizer.destroy();
    m_gui.finalize();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (m_controller) {
        SDL_GameControllerClose(m_controller);
    }
    SDL_GL_DeleteContext(m_glContext);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
    IMG_Quit();
}

void Platform::initSdlAndGl() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        throw std::runtime_error(SDL_GetError());
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        throw std::runtime_error(IMG_GetError());
    }

    // OpenGL 4.3 core context. (Distinct from the ImGui backend's GLSL "#version 330 core" in
    // initImGui() — that is a GLSL version kept compatible with this context, not this number.)
    constexpr int kGlContextMajor = 4;
    constexpr int kGlContextMinor = 3;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, kGlContextMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, kGlContextMinor);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    m_window = SDL_CreateWindow("OSP 2", 0, 0, kWindowWidth, kWindowHeight, SDL_WINDOW_OPENGL);
    if (!m_window) {
        throw std::runtime_error(SDL_GetError());
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        throw std::runtime_error(SDL_GetError());
    }

    // A single owned handle: the quit-on-START handler uses it on every platform, and CursorEmulator
    // opens its own (refcounted) handle for the same controller index on the Switch.
    m_controller = SDL_GameControllerOpen(0);
    SDL_GL_MakeCurrent(m_window, m_glContext);
    SDL_GL_SetSwapInterval(1);
    gladLoadGL();
}

void Platform::initImGui() {
    ImGui::CreateContext();
    if (!ImGui_ImplSDL2_InitForOpenGL(m_window, m_glContext)) {
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

    loadFonts();

    m_gui.initialize();
}

void Platform::loadFonts() {
    constexpr float kFontSize = 22.0f;

    const auto &io = ImGui::GetIO();

    ImFontConfig imFontConfig;
    imFontConfig.MergeMode = true;
    imFontConfig.GlyphOffset.y = 2.0f;

    constexpr ImWchar icon_ranges[] = {0x0030, 0xFFCB, 0};
    io.Fonts->AddFontFromFileTTF(assetPath("font/Roboto-Regular.ttf").string().c_str(), kFontSize);
    io.Fonts->AddFontFromFileTTF(
        assetPath("font/MaterialSymbolsSharp_Filled-Regular.ttf").string().c_str(),
        kFontSize,
        &imFontConfig,
        icon_ranges
    );
    io.Fonts->Build();
}

void Platform::initPlayerAndSettings() {
    m_settings.load(configPath());
    m_gui.applyTheme(themeFromString(m_settings.getString("user", "theme", "dark")));

    m_player.create();

    // Push persisted plugin settings; the INI section is "plugin.<pluginName>". Absent keys keep
    // the plugin's own default (getInt fallback = the descriptor's current value).
    for (const auto &[pluginName, descriptors] : m_player.getPluginSettings()) {
        const std::string section = "plugin." + pluginName;
        for (const auto &setting : descriptors) {
            m_player.applyPluginSetting(
                pluginName, setting.key, m_settings.getInt(section, setting.key, setting.value)
            );
        }
    }
    // Seed the Application's cached descriptors with the post-push values (kept off the per-frame path).
    m_app.refreshPluginSettings();

    // Restore the persisted visualizer by stable plugin name (mirrors the theme restore above). An
    // empty or unknown name leaves the controller's default (index 0) untouched.
    if (const std::string visualizerName = m_settings.getString("user", "visualizer", ""); !visualizerName.empty()) {
        if (const auto index = m_visualizer.indexOf(visualizerName)) {
            m_visualizer.select(*index);
        }
    }
}

std::filesystem::path Platform::resolveStartPath() const {
    // Prefer a hand-edited default_folder when it names a valid directory; otherwise fall back to
    // the compile-time default (sdmc root on Switch, cwd on desktop).
#if defined(__SWITCH__)
    std::filesystem::path start_path = "/";
#else
    std::filesystem::path start_path = std::filesystem::current_path();
#endif
    if (const auto default_folder = m_settings.getString("user", "default_folder", ""); !default_folder.empty()) {
        std::error_code ec;
        if (std::filesystem::is_directory(default_folder, ec)) {
            start_path = default_folder;
        }
    }
    return start_path;
}

void Platform::initNetwork() {
    // curl_global_init is not thread-safe: run it before m_fileSystem.create() spawns the worker.
    // On the Switch the network stack must be up first, else every transfer fails instantly.
#if defined(__SWITCH__)
    socketInitializeDefault();
#endif
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        // Non-fatal: local browsing still works; remote sources will fail their transfers.
        SDL_Log("curl_global_init failed — remote data sources will be unavailable");
    }
}

std::vector<std::unique_ptr<DataSource>> Platform::buildDataSources() const {
    std::vector<std::unique_ptr<DataSource>> sources;
    sources.push_back(std::make_unique<LocalDataSource>());
    sources.push_back(
        std::make_unique<FtpDataSource>("Modland (FTP)", "ftp.modland.com", "/pub/modules", cachePath() / "modland")
    );

    // Cache subdirs must be unique, so no user source collides with a built-in source's cache dir
    // or with another user source that sanitizes to the same component (cross-contaminated caches).
    // Seed the taken set from the built-in sources' own cache ids rather than hardcoding them.
    std::set<std::string> takenCacheDirs;
    for (const auto &source : sources) {
        if (auto id = source->getCacheId(); !id.empty()) {
            takenCacheDirs.insert(std::move(id));
        }
    }

    // Register each user-defined [source.NAME] INI section as an extra FTP source. Hand-edit only:
    // these are not seeded and not surfaced in the UI — they simply appear at the virtual root.
    constexpr std::string_view kSourcePrefix = "source.";
    for (const auto &section : m_settings.getSectionNames(std::string(kSourcePrefix))) {
        const std::string name = section.substr(kSourcePrefix.size());
        const std::string host = m_settings.getString(section, "host", "");
        if (name.empty() || host.empty()) {
            SDL_Log("Platform: skipping [%s]: a source needs a non-empty name and host", section.c_str());
            continue;
        }
        const std::string subdir = sanitizeCachePathComponent(name);
        if (!takenCacheDirs.insert(subdir).second) {
            SDL_Log("Platform: skipping [%s]: cache dir '%s' already in use", section.c_str(), subdir.c_str());
            continue;
        }
        const std::string path = m_settings.getString(section, "path", "/");
        sources.push_back(std::make_unique<FtpDataSource>(name, host, path, cachePath() / subdir));
    }
    return sources;
}
