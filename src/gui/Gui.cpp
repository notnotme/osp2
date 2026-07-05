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

#include "Gui.h"

#include <imgui.h>
#include <SDL_image.h>

#include <fstream>


Gui::Gui()
    : m_texture(0), m_theme(Theme::DARK) {}

void Gui::initialize(const std::string &basePath) {
    const auto bin_path = basePath + "sprites/sprites.bin";
    auto file = std::ifstream(bin_path, std::ifstream::in | std::ifstream::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open sprites.bin");
    }

    char check[5] = {};
    file.read(check, 4);
    if (std::string(check) != "SPSH") {
        throw std::runtime_error("sprites.bin wrong signature");
    }

    auto count = 0;
    file.read(reinterpret_cast<char *>(&count), sizeof(count));

    for (int i = 0; i < count; ++i) {
        char name[32] = {};
        file.read(name, 32);

        Sprite sprite = {};
        file.read(reinterpret_cast<char *>(&sprite), sizeof(Sprite));
        m_sprites.emplace(name, sprite);
    }

    const auto png_path = basePath + "sprites/sprites.png";
    const auto image = IMG_Load(png_path.c_str());
    if (!image) {
        throw std::runtime_error("Failed to open sprites.png");
    }

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image->w, image->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
    SDL_FreeSurface(image);

    // Shared style metrics are theme-independent: set them once here. applyTheme only
    // swaps colors afterwards, so these survive runtime theme switches.
    auto &style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    applyTheme(m_theme);
}

void Gui::finalize() {
    glDeleteTextures(1, &m_texture);
}

void Gui::applyTheme(const Theme theme) {
    m_theme = theme;
    switch (theme) {
        case Theme::DARK:
            ImGui::StyleColorsDark();
            break;
        case Theme::LIGHT:
            ImGui::StyleColorsLight();
            break;
        case Theme::CLASSIC:
            ImGui::StyleColorsClassic();
            break;
    }
}

void Gui::drawMainMenuBar(const std::string &file) {
    ImGui::BeginMainMenuBar();
    ImGui::Text("\ue405  %s", file.c_str());
    ImGui::EndMainMenuBar();
}

void Gui::drawCurrentPath(const std::string &path) {
    constexpr auto color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
    ImGui::TextColored(color,"\ue2c7");
    ImGui::SameLine();
    ImGui::Text("%s", path.c_str());
}

void Gui::drawFileBrowser(const std::vector<FileEntry> &files, const std::function<void(const FileEntry &)> &onFileClick, const bool isWorking) {
    constexpr auto folder_color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
    constexpr auto file_color = ImVec4(0.2f, 0.6f, 0.9f, 1.0f);
    constexpr auto file_browser_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg;

    const auto file_list_count = static_cast<int32_t>(files.size());

    ImGui::BeginTable("file_browser", 2, file_browser_flags);
    const auto table_position = ImGui::GetWindowPos();
    const auto table_size = ImGui::GetWindowSize();

    ImGui::TableSetupScrollFreeze(0, 2);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(folder_color,"\ue2c7");
    ImGui::SameLine();
    if (ImGui::Selectable("..", false, ImGuiSelectableFlags_SpanAllColumns)) {
        // Click
    }

    ImGuiListClipper clipper;
    clipper.Begin(file_list_count);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            const auto &file_entry = files[row];
            const auto entry_label = file_entry.name.c_str();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (file_entry.is_directory) {
                ImGui::TextColored(folder_color,"\ue2c7");
                ImGui::SameLine();
                if (ImGui::Selectable(entry_label, false, ImGuiSelectableFlags_SpanAllColumns)) {
                    // Click
                }
            } else {
                ImGui::TextColored(file_color,"\ueb82");
                ImGui::SameLine();
                if (ImGui::Selectable(entry_label, false, ImGuiSelectableFlags_SpanAllColumns)) {
                    onFileClick(file_entry);
                }
                ImGui::TableNextColumn();
                ImGui::Text("%i Kb", file_entry.file_size);
            }
        }
    }
    ImGui::EndTable();

    if (isWorking) {
        constexpr auto overlay_flags = ImGuiWindowFlags_NoDecoration;

        const auto animation_time = -1.0f * static_cast<float>(ImGui::GetTime());
        const auto bar_size = ImVec2(table_size.x / 2.0f, 32.0f);
        const auto bar_position_x = (table_size.x - bar_size.x) / 2.0f;
        const auto bar_position_y = (table_size.y - bar_size.y) / 2.0f;

        ImGui::SetNextWindowPos(table_position);
        ImGui::SetNextWindowSize(table_size);
        ImGui::SetNextWindowBgAlpha(0.8);

        ImGui::Begin("##file_browser_overlay", nullptr, overlay_flags);
        ImGui::SetCursorPosX(bar_position_x);
        ImGui::SetCursorPosY(bar_position_y);
        ImGui::ProgressBar(animation_time, bar_size, "Loading...");
        ImGui::End();
    }
}

void Gui::drawPlayerControls(const std::function<void(ButtonId)> &onButtonClick) {
    constexpr auto button_size = ImVec2(64.0f, 64.0f);
    constexpr auto total_button_size = button_size.x * 4;

    const auto &style = ImGui::GetStyle();
    const auto cursor_position_x = ImGui::GetCursorPosX();
    const auto available_size = ImGui::GetContentRegionAvail();
    const auto blank_space = style.FramePadding.x * 8 + style.ItemSpacing.x * 3;
    const auto start_x = (available_size.x - (total_button_size + blank_space)) / 2.0f;
    ImGui::SetCursorPosX(cursor_position_x + start_x);

    const auto &play_pause = m_sprites.at("play");
    if (ImGui::ImageButton("play_pause_button", m_texture, button_size,
        ImVec2(play_pause.s, play_pause.t),
        ImVec2(play_pause.p, play_pause.q))) {
        onButtonClick(PLAY_PAUSE);
    };

    const auto &stop = m_sprites.at("stop");
    ImGui::SameLine();
    if (ImGui::ImageButton("stop_button", m_texture, button_size,
        ImVec2(stop.s, stop.t),
        ImVec2(stop.p, stop.q))) {
        onButtonClick(STOP);
    };

    const auto &previous = m_sprites.at("previous");
    ImGui::SameLine();
    if (ImGui::ImageButton("previous_button", m_texture, button_size,
        ImVec2(previous.s, previous.t),
        ImVec2(previous.p, previous.q))) {
        onButtonClick(PREVIOUS);
    };

    const auto &next = m_sprites.at("next");
    ImGui::SameLine();
    if (ImGui::ImageButton("next_button", m_texture, button_size,
        ImVec2(next.s, next.t),
        ImVec2(next.p, next.q))) {
        onButtonClick(NEXT);
    };
}

void Gui::drawTrackInformation() {
    constexpr auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp;
    ImGui::BeginTable("track_info", 2, flags);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Title");
    ImGui::TableNextColumn();
    ImGui::Text("Placeholders never dies");

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Position");
    ImGui::TableNextColumn();
    ImGui::Text("%s / %s", "0:01", "3:37");

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Track");
    ImGui::TableNextColumn();
    ImGui::Text("%s / %s", "1", "1");

    ImGui::EndTable();
}

void Gui::drawTabsSection() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginTabBar("tabs_section");
    drawFileMetadata();
    drawTabPlaylist();
    drawTabSettings();
    drawTabAbout();
    ImGui::EndTabBar();
    ImGui::PopStyleVar();
}

void Gui::drawFileMetadata() {
    if (ImGui::BeginTabItem("File metadata")) {
        ImGui::Text("Metadata");
        ImGui::EndTabItem();
    }
}

void Gui::drawTabPlaylist() {
    if (ImGui::BeginTabItem("Playlist")) {
        ImGui::Text("Playlist");
        ImGui::EndTabItem();
    }
}

void Gui::drawTabSettings() {
    if (ImGui::BeginTabItem("Settings")) {
        ImGui::Text("Settings");
        ImGui::EndTabItem();
    }
}

void Gui::drawTabAbout() {
    if (ImGui::BeginTabItem("About")) {
        const auto &style = ImGui::GetStyle();
        const auto &logo = m_sprites.at("k7");
        const auto cursor_position_x = ImGui::GetCursorPosX();
        const auto available_size = ImGui::GetContentRegionAvail();
        const auto start_x = (available_size.x - static_cast<float>(logo.w)) / 2.0f;
        ImGui::SetCursorPosX(cursor_position_x + start_x);

        ImGui::Image(m_texture, ImVec2(logo.w, logo.h),
            ImVec2(logo.s, logo.t),
            ImVec2(logo.p, logo.q));

        ImGui::EndTabItem();
    }
}

void Gui::drawUserInterface(const UiState &state, const UiActions &actions) {
    drawMainMenuBar(state.status.fileName);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::Begin("fullscreen_window", nullptr, flags);
    ImGui::BeginTable("table_area", 2);

    ImGui::TableNextColumn();
    drawCurrentPath(state.path);
    drawFileBrowser(state.files, actions.onFileClick, state.isWorking);

    ImGui::TableNextColumn();
    drawPlayerControls(actions.onButtonClick);
    drawTrackInformation();
    drawTabsSection();

    ImGui::EndTable();
    ImGui::End();
}
