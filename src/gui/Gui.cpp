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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>


namespace {
    // Formats a duration as "m:ss" (zero-padded seconds). Negative or NaN clamps to "0:00".
    std::string formatTime(const double seconds) {
        if (std::isnan(seconds) || seconds < 0.0) {
            return "0:00";
        }
        const auto total_seconds = static_cast<int>(seconds);
        const auto minutes = total_seconds / 60;
        const auto remaining_seconds = total_seconds % 60;

        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%d:%02d", minutes, remaining_seconds);
        return std::string(buffer);
    }
}


Gui::Gui()
    : m_texture(0), m_theme(Theme::DARK), m_viewMode(ViewMode::WORKSPACE), m_aboutRequested(false) {}

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

void Gui::drawTopBar() {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextUnformatted("OSP2");
        ImGui::Separator();

        if (ImGui::BeginMenu("  Settings")) {
            if (ImGui::BeginMenu("Theme")) {
                if (ImGui::MenuItem("Dark", nullptr, m_theme == Theme::DARK)) {
                    applyTheme(Theme::DARK);
                }
                if (ImGui::MenuItem("Light", nullptr, m_theme == Theme::LIGHT)) {
                    applyTheme(Theme::LIGHT);
                }
                if (ImGui::MenuItem("Classic", nullptr, m_theme == Theme::CLASSIC)) {
                    applyTheme(Theme::CLASSIC);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("About")) {
            m_aboutRequested = true;
        }

        // View-mode toggle, right-aligned: fullscreen glyph in WORKSPACE (go collapse),
        // fullscreen_exit in VISUALIZATION (come back).
        const auto &style = ImGui::GetStyle();
        const auto *toggle_label = m_viewMode == ViewMode::WORKSPACE ? "" : "";
        const auto toggle_width = ImGui::CalcTextSize(toggle_label).x + style.FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - toggle_width - style.ItemSpacing.x);
        if (ImGui::MenuItem(toggle_label)) {
            m_viewMode = m_viewMode == ViewMode::WORKSPACE ? ViewMode::VISUALIZATION : ViewMode::WORKSPACE;
        }

        // Opened from the always-drawn menu bar so About works in both view modes;
        // OpenPopup and BeginPopupModal share this window's ID scope.
        if (m_aboutRequested) {
            ImGui::OpenPopup("About");
            m_aboutRequested = false;
        }
        drawAboutPopup();

        ImGui::EndMainMenuBar();
    }
}

void Gui::drawAboutPopup() {
    const auto center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::BeginPopupModal("About", nullptr, flags)) {
        const auto &logo = m_sprites.at("k7");
        ImGui::Image(m_texture, ImVec2(logo.w, logo.h),
            ImVec2(logo.s, logo.t),
            ImVec2(logo.p, logo.q));

        ImGui::Spacing();
        ImGui::TextUnformatted("OSP2 — chiptune player");
        ImGui::TextUnformatted("GPL-3.0-or-later");
        ImGui::Spacing();

        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Gui::drawCurrentPath(const std::string &path) {
    constexpr auto color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
    ImGui::TextColored(color, "");
    ImGui::SameLine();
    ImGui::Text("%s", path.c_str());
}

void Gui::drawFileBrowser(const std::vector<FileEntry> &files, const std::function<void(const FileEntry &)> &onFileClick, const bool isWorking) {
    constexpr auto folder_color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
    constexpr auto file_color = ImVec4(0.2f, 0.6f, 0.9f, 1.0f);
    constexpr auto file_browser_flags = ImGuiTableFlags_SizingFixedFit
        | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg;

    const auto file_list_count = static_cast<int32_t>(files.size());

    // Capture the browser rectangle in screen space before the table, so the loading
    // overlay covers exactly this area (the left pane), not the whole window.
    const auto browser_position = ImGui::GetCursorScreenPos();
    const auto browser_size = ImGui::GetContentRegionAvail();

    if (ImGui::BeginTable("file_browser", 2, file_browser_flags, browser_size)) {
        ImGui::TableSetupScrollFreeze(0, 2);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(folder_color, "");
        ImGui::SameLine();
        if (ImGui::Selectable("..", false, ImGuiSelectableFlags_SpanAllColumns)) {
            // Directory navigation is wired in TODO_4.
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
                    ImGui::TextColored(folder_color, "");
                    ImGui::SameLine();
                    if (ImGui::Selectable(entry_label, false, ImGuiSelectableFlags_SpanAllColumns)) {
                        // Directory navigation is wired in TODO_4.
                    }
                } else {
                    ImGui::TextColored(file_color, "");
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
    }

    if (isWorking) {
        constexpr auto overlay_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;

        const auto animation_time = -1.0f * static_cast<float>(ImGui::GetTime());
        const auto bar_size = ImVec2(browser_size.x / 2.0f, 32.0f);
        const auto bar_position_x = (browser_size.x - bar_size.x) / 2.0f;
        const auto bar_position_y = (browser_size.y - bar_size.y) / 2.0f;

        ImGui::SetNextWindowPos(browser_position);
        ImGui::SetNextWindowSize(browser_size);
        ImGui::SetNextWindowBgAlpha(0.8f);

        ImGui::Begin("##file_browser_overlay", nullptr, overlay_flags);
        ImGui::SetCursorPosX(bar_position_x);
        ImGui::SetCursorPosY(bar_position_y);
        ImGui::ProgressBar(animation_time, bar_size, "Loading...");
        ImGui::End();
    }
}

void Gui::drawTabsSection() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::BeginTabBar("tabs_section")) {
        drawFileMetadata();
        drawTabPlaylist();
        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();
}

void Gui::drawFileMetadata() {
    if (ImGui::BeginTabItem("Metadata")) {
        // Placeholder key/value view; TODO_5 supplies typed per-plugin metadata.
        constexpr auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("metadata_table", 2, flags)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Title");
            ImGui::TableNextColumn();
            ImGui::Text("Cool Song");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Format");
            ImGui::TableNextColumn();
            ImGui::Text("S3M");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Channels");
            ImGui::TableNextColumn();
            ImGui::Text("16");

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
}

void Gui::drawTabPlaylist() {
    if (ImGui::BeginTabItem("Playlist")) {
        ImGui::Text("Playlist");
        ImGui::EndTabItem();
    }
}

void Gui::drawPlayerBar(const PlaybackStatus &status, const std::function<void(ButtonId)> &onButtonClick) {
    const auto item_spacing_x = ImGui::GetStyle().ItemSpacing.x;
    constexpr auto row_spacing = 4.0f;
    constexpr auto progress_bar_height = 18.0f;
    constexpr auto button_frame_padding = 4.0f;
    constexpr auto button_size = ImVec2(48.0f, 48.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(item_spacing_x, row_spacing));

    // Vertically center the three rows (track line, progress, transport) in the bar so the
    // transport buttons keep a clear margin above the bar's bottom edge.
    const auto text_height = ImGui::GetTextLineHeight();
    const auto button_height = button_size.y + button_frame_padding * 2.0f;
    const auto content_height = text_height + row_spacing
        + std::max(text_height, progress_bar_height) + row_spacing + button_height;
    const auto slack = ImGui::GetContentRegionAvail().y - content_height;
    if (slack > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + slack / 2.0f);
    }

    // Track line.
    if (status.state == PlayerState::STOPPED || status.fileName.empty()) {
        ImGui::Text("  No track");
    } else if (status.title.empty()) {
        ImGui::Text("  %s", status.fileName.c_str());
    } else {
        ImGui::Text("  %s · %s", status.title.c_str(), status.fileName.c_str());
    }

    // Progress row: position | display-only bar | duration.
    const auto position = formatTime(status.positionSeconds);
    const auto duration = formatTime(status.durationSeconds);
    auto fraction = 0.0f;
    if (status.durationSeconds > 0.0) {
        fraction = std::clamp(static_cast<float>(status.positionSeconds / status.durationSeconds), 0.0f, 1.0f);
    }

    ImGui::TextUnformatted(position.c_str());
    ImGui::SameLine();
    const auto duration_width = ImGui::CalcTextSize(duration.c_str()).x;
    const auto bar_width = ImGui::GetContentRegionAvail().x - duration_width - item_spacing_x;
    ImGui::ProgressBar(fraction, ImVec2(bar_width, progress_bar_height), "");
    ImGui::SameLine();
    ImGui::TextUnformatted(duration.c_str());

    // Transport: previous, play/pause, stop, next, centered as a group.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(button_frame_padding, button_frame_padding));

    const auto &transport_style = ImGui::GetStyle();
    const auto total_buttons = button_size.x * 4.0f;
    const auto blank_space = transport_style.FramePadding.x * 8.0f + transport_style.ItemSpacing.x * 3.0f;
    const auto start_x = (ImGui::GetContentRegionAvail().x - (total_buttons + blank_space)) / 2.0f;
    if (start_x > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start_x);
    }

    const auto image_button = [&](const char *id, const char *sprite_key, const ButtonId button) {
        const auto &sprite = m_sprites.at(sprite_key);
        if (ImGui::ImageButton(id, m_texture, button_size,
            ImVec2(sprite.s, sprite.t),
            ImVec2(sprite.p, sprite.q))) {
            onButtonClick(button);
        }
    };

    image_button("previous_button", "previous", PREVIOUS);
    ImGui::SameLine();
    image_button("play_pause_button", status.state == PlayerState::PLAYING ? "pause" : "play", PLAY_PAUSE);
    ImGui::SameLine();
    image_button("stop_button", "stop", STOP);
    ImGui::SameLine();
    image_button("next_button", "next", NEXT);

    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
}

void Gui::drawUserInterface(const UiState &state, const UiActions &actions) {
    drawTopBar();

    // VISUALIZATION mode draws only the top bar; the area below is left empty (GL clear
    // color shows through) for the future visualization system. Audio is unaffected.
    if (m_viewMode == ViewMode::VISUALIZATION) {
        return;
    }

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGui::Begin("fullscreen_window", nullptr, flags);

    // Left/right panes fill the height above a full-width player bar pinned to the bottom.
    // Geometry is derived from the live content region and ItemSpacing so nothing overflows.
    const auto &style = ImGui::GetStyle();
    const auto available = ImGui::GetContentRegionAvail();
    constexpr auto player_bar_height = 140.0f;
    const auto panes_height = available.y - style.ItemSpacing.y - player_bar_height;
    const auto left_width = (available.x - style.ItemSpacing.x) * 0.45f;
    const auto right_width = (available.x - style.ItemSpacing.x) - left_width;

    if (ImGui::BeginChild("left_pane", ImVec2(left_width, panes_height), ImGuiChildFlags_Borders)) {
        drawCurrentPath(state.path);
        drawFileBrowser(state.files, actions.onFileClick, state.isWorking);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("right_pane", ImVec2(right_width, panes_height), ImGuiChildFlags_Borders)) {
        drawTabsSection();
    }
    ImGui::EndChild();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 6.0f));
    if (ImGui::BeginChild("player_bar", ImVec2(0.0f, player_bar_height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar)) {
        drawPlayerBar(state.status, actions.onButtonClick);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::End();
}
