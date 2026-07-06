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

#include "../Paths.h"

#include <imgui.h>
#include <SDL_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>


namespace {
    // std::visit helper: builds an overload set from lambdas (C++20 aggregate CTAD, no guide needed).
    template <class... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };

    // Shared building blocks for the per-plugin metadata tables (drawModuleMetadata,
    // drawGmeMetadata, and future SID/sc68). Text rows are skipped when empty; count rows
    // are always meaningful (0 = none). Call between BeginTable/EndTable.
    void metadataTextRow(const char *label, const std::string &value) {
        if (value.empty()) {
            return;
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(value.c_str());
    }

    void metadataCountRow(const char *label, const int value) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
        ImGui::Text("%d", value);
    }

    // A labelled, scrollable, word-wrapped block for often-multiline free text (song message,
    // GME comment). Skipped when empty. TextUnformatted never treats the (user-authored) text as a
    // printf format string. Not drawn inside a table.
    void metadataTextBlock(const char *label, const std::string &text) {
        if (text.empty()) {
            return;
        }
        ImGui::Spacing();
        ImGui::TextUnformatted(label);
        if (ImGui::BeginChild("metadata_message", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(text.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndChild();
    }

    constexpr auto metadata_table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;

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
        return {buffer};
    }

    // Formats a byte count as "N B", "N.N KB", or "N.N MB".
    std::string formatSize(const std::int64_t bytes) {
        char buffer[32];
        if (bytes < 1024) {
            std::snprintf(buffer, sizeof(buffer), "%lld B", static_cast<long long>(bytes));
        } else if (bytes < 1024 * 1024) {
            std::snprintf(buffer, sizeof(buffer), "%.1f KB", static_cast<double>(bytes) / 1024.0);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        }
        return {buffer};
    }
} // namespace


Gui::Gui()
    : m_texture(0),
      m_theme(Theme::DARK),
      m_viewMode(ViewMode::WORKSPACE),
      m_aboutRequested(false) {}

void Gui::initialize() {
    const auto bin_path = assetPath("sprites/sprites.bin");
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

    const auto png_path = assetPath("sprites/sprites.png").string();
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

void Gui::drawTopBar(
    const std::vector<std::pair<std::string, std::vector<PluginSetting>>> &pluginSettings,
    const std::function<void(Theme)> &onThemeChange,
    const std::function<void(const std::string &, const std::string &, int)> &onPluginSettingChange,
    const std::function<void(const std::string &, const std::string &, int)> &onPluginSettingCommit,
    const std::vector<std::string> &visualizerNames,
    const std::size_t activeVisualizer,
    const std::function<void(std::size_t)> &onSelectVisualizer
) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextUnformatted("OSP2");
        ImGui::Separator();

        if (ImGui::BeginMenu(" Settings")) {
            if (ImGui::BeginMenu("Theme")) {
                if (ImGui::MenuItem("Dark", nullptr, m_theme == Theme::DARK)) {
                    applyTheme(Theme::DARK);
                    onThemeChange(Theme::DARK);
                }
                if (ImGui::MenuItem("Light", nullptr, m_theme == Theme::LIGHT)) {
                    applyTheme(Theme::LIGHT);
                    onThemeChange(Theme::LIGHT);
                }
                if (ImGui::MenuItem("Classic", nullptr, m_theme == Theme::CLASSIC)) {
                    applyTheme(Theme::CLASSIC);
                    onThemeChange(Theme::CLASSIC);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Plugins")) {
                // One entry per plugin that publishes settings; clicking it opens that
                // plugin's popup (latched by name, opened below in the menu-bar scope).
                auto any_shown = false;
                for (const auto &[pluginName, descriptors] : pluginSettings) {
                    if (descriptors.empty()) {
                        continue;
                    }
                    any_shown = true;
                    if (ImGui::MenuItem(pluginName.c_str())) {
                        m_requestedPluginPopup = pluginName;
                    }
                }
                if (!any_shown) {
                    ImGui::TextDisabled("No configurable plugins");
                }
                ImGui::EndMenu();
            }

            // Visualizer picker: one entry per registered visualizer, checkmark on the active one.
            // Selecting fires onSelectVisualizer(i); main.cpp calls VisualizerController::select — Gui
            // stays ignorant of the visualizer domain (same principle as onRenderVisualization).
            if (ImGui::BeginMenu("Visualizer")) {
                for (std::size_t i = 0; i < visualizerNames.size(); ++i) {
                    if (ImGui::MenuItem(visualizerNames[i].c_str(), nullptr, i == activeVisualizer)) {
                        onSelectVisualizer(i);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem(" About")) {
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

        if (!m_requestedPluginPopup.empty()) {
            ImGui::OpenPopup(m_requestedPluginPopup.c_str());
            m_requestedPluginPopup.clear();
        }
        drawPluginPopups(pluginSettings, onPluginSettingChange, onPluginSettingCommit);

        ImGui::EndMainMenuBar();
    }
}

void Gui::drawAboutPopup() const {
    const auto center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::BeginPopupModal("About", nullptr, flags)) {
        const auto &logo = m_sprites.at("k7");
        ImGui::Image(m_texture, ImVec2(logo.w, logo.h), ImVec2(logo.s, logo.t), ImVec2(logo.p, logo.q));

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

// One modal per plugin, keyed and titled by the plugin name (matches OpenPopup by name above);
// only the plugin the user picked in the submenu is ever open. Widgets are generic — one per
// descriptor, driven entirely by the descriptor's shape. On open the popup seeds a Gui-owned
// working copy (m_settingsEdit) from the descriptor cache and binds every widget to it, so the
// slider reads from stable storage and never flashes off a frame-lagging cache. Editing a widget
// applies to the decoder live (onPluginSettingChange) for immediate audio preview but does not
// persist. Save writes every value to the INI (onPluginSettingCommit per descriptor) and closes;
// Close closes without persisting, leaving the live-applied values in the decoder for the session.
void Gui::drawPluginPopups(
    const std::vector<std::pair<std::string, std::vector<PluginSetting>>> &pluginSettings,
    const std::function<void(const std::string &, const std::string &, int)> &onPluginSettingChange,
    const std::function<void(const std::string &, const std::string &, int)> &onPluginSettingCommit
) {
    const auto center = ImGui::GetMainViewport()->GetCenter();
    constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

    for (const auto &[pluginName, descriptors] : pluginSettings) {
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (!ImGui::BeginPopupModal(pluginName.c_str(), nullptr, flags)) {
            // Closed — via a button or the Escape key: forget it here (the single place that clears
            // the latch) so the next open reseeds the working copy from the live-kept cache.
            if (m_openSettingsPlugin == pluginName) {
                m_openSettingsPlugin.clear();
            }
            continue;
        }
        // Seed the working copy once when this popup becomes the open one, from the current
        // descriptor values; subsequent frames bind widgets to it (no re-read of the cache).
        if (m_openSettingsPlugin != pluginName) {
            m_openSettingsPlugin = pluginName;
            m_settingsEdit.clear();
            for (const auto &s : descriptors) {
                m_settingsEdit[s.key] = s.value;
            }
        }

        // Scope widget IDs by plugin so descriptor keys need only be unique per plugin
        // (the INI-key contract), not globally — two plugins may share a key.
        ImGui::PushID(pluginName.c_str());
        for (const auto &setting : descriptors) {
            ImGui::PushID(setting.key.c_str());
            // Reference into the working copy: a stable address across frames, so ImGui sees a
            // persistent backing value and the widget never flashes.
            int &v = m_settingsEdit[setting.key];
            std::visit(
                [&](const auto &shape) {
                    using T = std::decay_t<decltype(shape)>;
                    if constexpr (std::is_same_v<T, IntRange>) {
                        if (ImGui::SliderInt(setting.label.c_str(), &v, shape.min, shape.max)) {
                            onPluginSettingChange(pluginName, setting.key, v);
                        }
                    } else if constexpr (std::is_same_v<T, EnumOptions>) {
                        std::vector<const char *> items;
                        items.reserve(shape.labels.size());
                        for (const auto &l : shape.labels) {
                            items.push_back(l.c_str());
                        }
                        if (ImGui::Combo(setting.label.c_str(), &v, items.data(), static_cast<int>(items.size()))) {
                            onPluginSettingChange(pluginName, setting.key, v);
                        }
                    }
                },
                setting.shape
            );
            ImGui::PopID();
        }
        ImGui::PopID();

        ImGui::Separator();
        if (ImGui::Button("Save")) {
            for (const auto &setting : descriptors) {
                onPluginSettingCommit(pluginName, setting.key, m_settingsEdit[setting.key]);
            }
            ImGui::CloseCurrentPopup(); // the not-open branch clears m_openSettingsPlugin next frame
        }
        ImGui::SameLine();
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

void Gui::drawFileBrowser(
    const std::vector<FileEntry> &files,
    const std::function<void(const FileEntry &)> &onFileClick,
    const std::function<void(const FileEntry &)> &onDirectoryClick,
    const bool isWorking,
    const std::string &workingLabel,
    const std::function<void()> &onCancelWork,
    const std::string &playingFileName
) {
    // Rising edge of the loading overlay: focus is moved to the Cancel button once on this frame
    // (below) so gamepad/keyboard on the Switch can reach it; m_wasWorking is updated at function end.
    const bool overlayJustAppeared = isWorking && !m_wasWorking;

    constexpr auto folder_color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
    constexpr auto file_color = ImVec4(0.2f, 0.6f, 0.9f, 1.0f);
    constexpr auto file_browser_flags =
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg;

    const auto file_list_count = static_cast<int32_t>(files.size());

    // Capture the browser rectangle in screen space before the table, so the loading
    // overlay covers exactly this area (the left pane), not the whole window.
    const auto browser_position = ImGui::GetCursorScreenPos();
    const auto browser_size = ImGui::GetContentRegionAvail();

    // A scan blocks the whole browser: BeginDisabled stops mouse and keyboard/gamepad nav,
    // the overlay below dims it and shows the spinner.
    ImGui::BeginDisabled(isWorking);
    if (ImGui::BeginTable("file_browser", 3, file_browser_flags, browser_size)) {
        ImGui::TableSetupScrollFreeze(0, 2);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(folder_color, "");
        ImGui::SameLine();
        if (ImGui::Selectable("..", false, ImGuiSelectableFlags_SpanAllColumns)) {
            // ".." is pinned by the Gui (never a FileSystem entry); a synthetic entry carries the intent.
            onDirectoryClick(FileEntry{"..", 0, "Folder", true});
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
                        onDirectoryClick(file_entry);
                    }
                } else {
                    ImGui::TextColored(file_color, "");
                    ImGui::SameLine();
                    // Highlight the currently-playing track's row (match by filename, the same basis
                    // playAdjacentTrack uses; empty when nothing plays, so no false match).
                    const bool is_playing = !playingFileName.empty() && file_entry.name == playingFileName;
                    if (ImGui::Selectable(entry_label, is_playing, ImGuiSelectableFlags_SpanAllColumns)) {
                        onFileClick(file_entry);
                    }
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(file_entry.type.c_str());

                ImGui::TableNextColumn();
                if (!file_entry.is_directory) {
                    ImGui::TextUnformatted(formatSize(file_entry.file_size).c_str());
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();

    if (isWorking) {
        constexpr auto overlay_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;

        // ASCII spinner stepped ~8x/s: | / - \.
        constexpr char frames[] = {'|', '/', '-', '\\'};
        const char spinner[] = {frames[static_cast<int64_t>(ImGui::GetTime() * 8.0) & 3], '\0'};
        const char *text = workingLabel.c_str();

        // The frame chars differ in width in the proportional font, so give the spinner a fixed
        // slot and keep the label anchored: center [slot | gap | text] using the stable slot width,
        // then right-align the spinner within its slot so only its left edge moves (no jitter).
        const auto slot_width = ImGui::CalcTextSize("W").x;
        const auto gap_width = ImGui::CalcTextSize(" ").x;
        const auto text_size = ImGui::CalcTextSize(text);
        const auto spinner_width = ImGui::CalcTextSize(spinner).x;
        const auto start_x = (browser_size.x - (slot_width + gap_width + text_size.x)) / 2.0f;
        const auto start_y = (browser_size.y - text_size.y) / 2.0f;

        ImGui::SetNextWindowPos(browser_position);
        ImGui::SetNextWindowSize(browser_size);
        ImGui::SetNextWindowBgAlpha(0.8f);

        ImGui::Begin("##file_browser_overlay", nullptr, overlay_flags);
        // On the frame the overlay appears, pull focus onto this window so the Cancel button below is
        // reachable by gamepad/keyboard (Switch) without a mouse; done once, not every frame.
        if (overlayJustAppeared) {
            ImGui::SetWindowFocus();
        }
        ImGui::SetCursorPos(ImVec2(start_x + slot_width - spinner_width, start_y));
        ImGui::TextUnformatted(spinner);
        ImGui::SetCursorPos(ImVec2(start_x + slot_width + gap_width, start_y));
        ImGui::TextUnformatted(text);

        // Cancel button, centered below the spinner/label row; aborts the in-flight scan/download.
        const auto &overlay_style = ImGui::GetStyle();
        const auto cancel_width = ImGui::CalcTextSize("Cancel").x + overlay_style.FramePadding.x * 2.0f;
        const auto cancel_x = (browser_size.x - cancel_width) / 2.0f;
        const auto cancel_y = start_y + text_size.y + overlay_style.ItemSpacing.y;
        ImGui::SetCursorPos(ImVec2(cancel_x, cancel_y));
        // Focus the button once on the rising edge so A/Enter activates it; NOT every frame, which
        // would trap focus here and prevent the user from navigating away.
        if (overlayJustAppeared) {
            ImGui::SetKeyboardFocusHere();
        }
        if (ImGui::Button("Cancel")) {
            onCancelWork();
        }
        ImGui::End();
    }

    m_wasWorking = isWorking;
}

void Gui::drawTabsSection(const TrackMetadata &metadata) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::BeginTabBar("tabs_section")) {
        drawFileMetadata(metadata);
        drawTabPlaylist();
        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();
}

void Gui::drawFileMetadata(const TrackMetadata &metadata) {
    if (ImGui::BeginTabItem("Metadata")) {
        // Dispatch on the variant. Deliberately NO generic `auto` fallback: a new plugin's
        // metadata alternative must fail to compile here until its draw function is added.
        std::visit(
            overloaded{
                [](std::monostate) {
                    constexpr auto text = "No track loaded";
                    const auto available = ImGui::GetContentRegionAvail();
                    const auto text_size = ImGui::CalcTextSize(text);
                    ImGui::SetCursorPos(ImVec2(
                        ImGui::GetCursorPosX() + (available.x - text_size.x) / 2.0f,
                        ImGui::GetCursorPosY() + (available.y - text_size.y) / 2.0f
                    ));
                    ImGui::TextDisabled("%s", text);
                },
                [this](const ModuleMetadata &m) { drawModuleMetadata(m); },
                [this](const GmeMetadata &m) { drawGmeMetadata(m); },
                [this](const SidMetadata &m) { drawSidMetadata(m); },
                [this](const Sc68Metadata &m) { drawSc68Metadata(m); },
            },
            metadata
        );
        ImGui::EndTabItem();
    }
}

void Gui::drawModuleMetadata(const ModuleMetadata &metadata) {
    if (ImGui::BeginTable("metadata_table", 2, metadata_table_flags)) {
        metadataTextRow("Title", metadata.title);
        metadataTextRow("Artist", metadata.artist);
        metadataTextRow("Format", metadata.format);
        metadataTextRow("Tracker", metadata.tracker);
        metadataCountRow("Channels", metadata.channels);
        metadataCountRow("Patterns", metadata.patterns);
        metadataCountRow("Samples", metadata.samples);
        metadataCountRow("Instruments", metadata.instruments);
        ImGui::EndTable();
    }
    metadataTextBlock("Message", metadata.message);
}

void Gui::drawGmeMetadata(const GmeMetadata &metadata) {
    if (ImGui::BeginTable("metadata_table", 2, metadata_table_flags)) {
        metadataTextRow("Game", metadata.game);
        metadataTextRow("System", metadata.system);
        metadataTextRow("Author", metadata.author);
        metadataTextRow("Copyright", metadata.copyright);
        metadataCountRow("Tracks", metadata.trackCount);
        ImGui::EndTable();
    }
    metadataTextBlock("Comment", metadata.comment);
}

void Gui::drawSidMetadata(const SidMetadata &metadata) {
    if (ImGui::BeginTable("metadata_table", 2, metadata_table_flags)) {
        metadataTextRow("Title", metadata.title);
        metadataTextRow("Author", metadata.author);
        metadataTextRow("Released", metadata.released);
        metadataTextRow("SID model", metadata.sidModel);
        metadataTextRow("Clock", metadata.clock);
        ImGui::EndTable();
    }
}

void Gui::drawSc68Metadata(const Sc68Metadata &metadata) {
    if (ImGui::BeginTable("metadata_table", 2, metadata_table_flags)) {
        metadataTextRow("Title", metadata.title);
        metadataTextRow("Author", metadata.author);
        metadataTextRow("Composer", metadata.composer);
        metadataTextRow("Hardware", metadata.hardware);
        metadataTextRow("Ripper", metadata.ripper);
        ImGui::EndTable();
    }
}

void Gui::drawTabPlaylist() {
    if (ImGui::BeginTabItem("Playlist")) {
        ImGui::Text("Playlist");
        ImGui::EndTabItem();
    }
}

void Gui::drawPlayerBar(const PlaybackStatus &status, const std::function<void(ButtonId)> &onButtonClick) const {
    const auto item_spacing_x = ImGui::GetStyle().ItemSpacing.x;
    constexpr auto row_spacing = 4.0f;
    constexpr auto button_frame_padding = 4.0f;
    constexpr auto button_size = ImVec2(48.0f, 48.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(item_spacing_x, row_spacing));

    // Vertically center the three rows (track line, progress, transport) in the bar so the
    // transport buttons keep a clear margin above the bar's bottom edge. The progress row is as
    // tall as the timer labels; its playhead knob is smaller, so text_height sizes the row.
    constexpr auto button_height = button_size.y + button_frame_padding * 2.0f;
    const auto text_height = ImGui::GetTextLineHeight();
    const auto content_height = text_height + row_spacing + text_height + row_spacing + button_height;
    const auto slack = ImGui::GetContentRegionAvail().y - content_height;
    if (slack > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + slack / 2.0f);
    }

    // Track line.
    if (status.state == PlayerState::STOPPED || status.fileName.empty()) {
        ImGui::Text(" No track");
    } else if (status.title.empty()) {
        ImGui::Text(" %s", status.fileName.c_str());
    } else {
        ImGui::Text(" %s · %s", status.title.c_str(), status.fileName.c_str());
    }

    // Progress row: position | thin track line with a circular playhead | duration. The line and
    // knob are drawn on the window draw list (centred on the label line); the slot is reserved with a
    // plain Dummy so both timer labels keep one baseline — a framed widget would shove them around.
    const auto position = formatTime(status.positionSeconds);
    const auto duration = formatTime(status.durationSeconds);
    auto fraction = 0.0f;
    if (status.durationSeconds > 0.0) {
        fraction = std::clamp(static_cast<float>(status.positionSeconds / status.durationSeconds), 0.0f, 1.0f);
    }

    const auto duration_width = ImGui::CalcTextSize(duration.c_str()).x;

    ImGui::TextUnformatted(position.c_str());
    ImGui::SameLine();

    constexpr auto line_half_height = 1.5f; // 3 px-thick track line
    const auto track_width = ImGui::GetContentRegionAvail().x - duration_width - item_spacing_x;
    const auto track_origin = ImGui::GetCursorScreenPos();
    const auto track_y = track_origin.y + text_height * 0.5f;
    const auto track_x0 = track_origin.x;
    const auto track_x1 = track_origin.x + track_width;
    auto *draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(
        ImVec2(track_x0, track_y - line_half_height),
        ImVec2(track_x1, track_y + line_half_height),
        ImGui::GetColorU32(ImGuiCol_FrameBg),
        line_half_height
    );
    // With a track loaded, fill the line up to a circular playhead in the accent colour; when stopped
    // the row is just the empty track line (no knob). Same "track loaded" test as the title row above.
    // The knob's travel is inset by its radius so it never overflows the line ends or the timer labels.
    if (status.state != PlayerState::STOPPED && !status.fileName.empty()) {
        constexpr auto knob_radius = 6.0f;
        const auto knob_x = track_x0 + knob_radius + (track_width - knob_radius * 2.0f) * fraction;
        const auto accent = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
        draw_list->AddRectFilled(
            ImVec2(track_x0, track_y - line_half_height),
            ImVec2(knob_x, track_y + line_half_height),
            accent,
            line_half_height
        );
        draw_list->AddCircleFilled(ImVec2(knob_x, track_y), knob_radius, accent);
    }

    // Reserve the track's slot at the label line height so the line advances normally and the duration
    // label lands on the same baseline as the position label.
    ImGui::Dummy(ImVec2(track_width, text_height));
    ImGui::SameLine();
    ImGui::TextUnformatted(duration.c_str());

    // Transport: previous, play/pause, stop, next, centered as a group.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(button_frame_padding, button_frame_padding));

    constexpr auto total_buttons = button_size.x * 4.0f;
    const auto &transport_style = ImGui::GetStyle();
    const auto blank_space = transport_style.FramePadding.x * 8.0f + transport_style.ItemSpacing.x * 3.0f;
    const auto start_x = (ImGui::GetContentRegionAvail().x - (total_buttons + blank_space)) / 2.0f;
    if (start_x > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + start_x);
    }

    const auto image_button = [&](const char *id, const char *sprite_key, const ButtonId button) {
        const auto &sprite = m_sprites.at(sprite_key);
        if (ImGui::ImageButton(id, m_texture, button_size, ImVec2(sprite.s, sprite.t), ImVec2(sprite.p, sprite.q))) {
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
    drawTopBar(
        state.pluginSettings,
        actions.onThemeChange,
        actions.onPluginSettingChange,
        actions.onPluginSettingCommit,
        state.visualizerNames,
        state.activeVisualizer,
        actions.onSelectVisualizer
    );

    // VISUALIZATION mode draws only the top bar; the work area below is handed to the visualizer via
    // onRenderVisualization with the reserved rect (WorkPos/WorkSize already exclude the menu bar).
    // Gui stays presentation-only and knows nothing about the visualizer domain — main.cpp wires it.
    // Audio is unaffected.
    if (m_viewMode == ViewMode::VISUALIZATION) {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        if (actions.onRenderVisualization) {
            actions.onRenderVisualization(
                viewport->WorkPos.x, viewport->WorkPos.y, viewport->WorkSize.x, viewport->WorkSize.y
            );
        }
        return;
    }

    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

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
    const auto right_width = available.x - style.ItemSpacing.x - left_width;

    if (ImGui::BeginChild("left_pane", ImVec2(left_width, panes_height), ImGuiChildFlags_Borders)) {
        drawCurrentPath(state.path);
        // Nothing is "playing" when stopped, so pass an empty name (no row highlighted) in that state.
        const std::string playingFileName =
            state.status.state == PlayerState::STOPPED ? std::string{} : state.status.fileName;
        drawFileBrowser(
            state.files,
            actions.onFileClick,
            actions.onDirectoryClick,
            state.isWorking,
            state.workingLabel,
            actions.onCancelWork,
            playingFileName
        );
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("right_pane", ImVec2(right_width, panes_height), ImGuiChildFlags_Borders)) {
        drawTabsSection(state.metadata);
    }
    ImGui::EndChild();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 6.0f));
    if (ImGui::BeginChild(
            "player_bar", ImVec2(0.0f, player_bar_height), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar
        )) {
        drawPlayerBar(state.status, actions.onButtonClick);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::End();
}
