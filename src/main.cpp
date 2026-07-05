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
#include <stdexcept>
#include <vector>

#include "Application.h"
#include "filesystem/DataSource.h"
#include "filesystem/FileSystem.h"
#include "filesystem/LocalDataSource.h"
#include "gui/Gui.h"
#include "player/PlayerController.h"

#if defined(__SWITCH__)
    #define BASE_PATH "romfs:/"
#else
    #define BASE_PATH "romfs/"
#endif


SDL_Window *window;
SDL_Joystick *joystick;
SDL_GLContext opengl_context;
Gui gui;
FileSystem file_system;
PlayerController player;
Application app(player, file_system);

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

    joystick = SDL_JoystickOpen(0);
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

    ImFontConfig imFontConfig;
    imFontConfig.MergeMode = true;
    imFontConfig.GlyphOffset.y = 2.0f;

    constexpr ImWchar icon_ranges[] = { 0x0030, 0xFFCB, 0 };
    io.Fonts->AddFontFromFileTTF( BASE_PATH "font/Roboto-Regular.ttf", 22.0f);
    io.Fonts->AddFontFromFileTTF(BASE_PATH "font/MaterialSymbolsSharp_Filled-Regular.ttf", 22.0f, &imFontConfig, icon_ranges);
    io.Fonts->Build();

    gui.initialize(BASE_PATH);
    player.create();

    // Start path lives in one spot: TODO_6 will override it with default_folder when set.
#if defined(__SWITCH__)
    const std::filesystem::path start_path = "sdmc:/";
#else
    const std::filesystem::path start_path = std::filesystem::current_path();
#endif

    std::vector<std::unique_ptr<DataSource>> sources;
    sources.push_back(std::make_unique<LocalDataSource>());
    file_system.create(std::move(sources), start_path,
        [](const std::filesystem::path &p) { return player.isSupported(p); });
}

void finalize() {
    // Join the worker before tearing down the player: its isPlayable predicate calls into PlayerController.
    file_system.destroy();
    player.destroy();
    gui.finalize();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_JoystickClose(joystick);
    SDL_GL_DeleteContext(opengl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    IMG_Quit();
}

int main(int argc, char** argv) {
    initialize();

    const auto actions = app.makeUiActions();

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
                    if (event.jbutton.which == 0 && event.jbutton.button == SDL_CONTROLLER_BUTTON_START) {
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
        ImGui::NewFrame();
        gui.drawUserInterface(app.makeUiState(), actions);
        ImGui::Render();

        auto *draw_data = ImGui::GetDrawData();
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);

        SDL_GL_SwapWindow(window);
    }

    finalize();
    return EXIT_SUCCESS;
}
