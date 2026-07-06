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

#include "Settings.h"

#include <SDL.h>

#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <system_error>


namespace {

    std::string trim(const std::string &value) {
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return {};
        }
        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

} // namespace

void Settings::load(const std::filesystem::path &path) {
    m_path = path;
    m_data.clear();

    std::ifstream file(m_path);
    if (file) {
        parse(file);
        return;
    }

    // Missing file: seed and materialize defaults so the user can find and hand-edit the file.
    SDL_Log("Settings: '%s' not found, writing defaults", m_path.string().c_str());
    applyDefaults();
    save();
}

void Settings::save() const {
    // Ensure the target directory exists before writing: the config path may live in a nested,
    // not-yet-created directory (e.g. the Switch's "/switch/OSP2/", which is otherwise only made
    // lazily when a remote source first downloads to its cache). Best-effort — a failure here just
    // surfaces as the open failing below.
    if (m_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(m_path.parent_path(), ec);
    }

    std::ofstream file(m_path, std::ios::trunc);
    if (!file) {
        SDL_Log("Settings: cannot open '%s' for writing", m_path.string().c_str());
        return;
    }

    bool first = true;
    for (const auto &[section, entries] : m_data) {
        if (!first) {
            file << '\n';
        }
        first = false;
        file << '[' << section << "]\n";
        for (const auto &[key, value] : entries) {
            file << key << " = " << value << '\n';
        }
    }
}

void Settings::parse(std::istream &input) {
    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);

        // Strip an inline comment (from the first '#' or ';') then re-trim the remainder.
        if (const auto comment = line.find_first_of("#;"); comment != std::string::npos) {
            line = trim(line.substr(0, comment));
        }

        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos || section.empty()) {
            SDL_Log("Settings: skipping malformed line: %s", line.c_str());
            continue;
        }

        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        m_data[section][key] = value;
    }
}

void Settings::applyDefaults() {
    m_data["user"]["theme"] = "dark";
    m_data["user"]["default_folder"] = "";
}

std::string Settings::getString(const std::string &section, const std::string &key, const std::string &fallback) const {
    const auto sectionIt = m_data.find(section);
    if (sectionIt == m_data.end()) {
        return fallback;
    }
    const auto keyIt = sectionIt->second.find(key);
    if (keyIt == sectionIt->second.end()) {
        return fallback;
    }
    return keyIt->second;
}

int Settings::getInt(const std::string &section, const std::string &key, const int fallback) const {
    const auto value = getString(section, key, "");
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

std::vector<std::string> Settings::getSectionNames(const std::string &prefix) const {
    std::vector<std::string> names;
    // m_data is a std::map, so iteration is already in sorted section order.
    for (const auto &section : m_data | std::views::keys) {
        if (section.rfind(prefix, 0) == 0) {
            names.push_back(section);
        }
    }
    return names;
}

void Settings::setString(const std::string &section, const std::string &key, const std::string &value) {
    m_data[section][key] = value;
}

void Settings::setInt(const std::string &section, const std::string &key, const int value) {
    m_data[section][key] = std::to_string(value);
}
