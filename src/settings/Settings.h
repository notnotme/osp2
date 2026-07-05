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

#ifndef OSP2_SETTINGS_H
#define OSP2_SETTINGS_H

#include <filesystem>
#include <map>
#include <string>


// Hand-editable INI store: sections of key = value pairs. Main-thread only.
// Grammar (hand-rolled parser): a line is trimmed, then an inline comment starting at the
// first '#' or ';' is stripped and the remainder re-trimmed; blank lines are skipped; a
// "[section]" line switches the current section; any other line splits on the FIRST '=' into
// a trimmed key/value under the current section. Because comments are stripped inline, '#'
// and ';' cannot appear inside a value. The writer does NOT preserve comments (documented
// limitation); unknown sections/keys survive a round-trip.
class Settings final {
private:
    // Ordered maps → deterministic section/key output on save.
    std::map<std::string, std::map<std::string, std::string>> m_data;
    std::filesystem::path m_path;

public:
    Settings(const Settings &) = delete;
    Settings &operator=(const Settings &) = delete;
    explicit Settings() = default;
    ~Settings() = default;

public:
    // Remembers path for later save(). If the file opens, parses it; if it is missing, seeds
    // the default user settings and immediately writes them so first run materializes a
    // discoverable, hand-editable file.
    void load(const std::filesystem::path &path);
    void save() const;

    [[nodiscard]] std::string getString(const std::string &section, const std::string &key, const std::string &fallback) const;
    [[nodiscard]] int getInt(const std::string &section, const std::string &key, int fallback) const;
    // Setters mutate m_data only; callers call save() explicitly after a batch of changes.
    void setString(const std::string &section, const std::string &key, const std::string &value);
    void setInt(const std::string &section, const std::string &key, int value);

private:
    void parse(std::istream &input);
    void applyDefaults();
};


#endif //OSP2_SETTINGS_H
