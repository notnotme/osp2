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
#include <vector>


/**
 * Hand-editable INI store: sections of key = value pairs. Main-thread only.
 *
 * Grammar (hand-rolled parser): a line is trimmed, then an inline comment starting at the first '#' or ';' is
 * stripped and the remainder re-trimmed; blank lines are skipped; a "[section]" line switches the current section;
 * any other line splits on the FIRST '=' into a trimmed key/value under the current section. Because comments are
 * stripped inline, '#' and ';' cannot appear inside a value. The writer does NOT preserve comments (documented
 * limitation); unknown sections/keys survive a round-trip.
 */
class Settings final {
private:
    /** Ordered maps → deterministic section/key output on save. */
    std::map<std::string, std::map<std::string, std::string>> m_data;
    std::filesystem::path m_path; ///< INI file location, remembered by load() for save().

public:
    Settings(const Settings &) = delete;
    Settings &operator=(const Settings &) = delete;
    explicit Settings() = default;
    ~Settings() = default;

public:
    /**
     * Loads the INI file at path, remembering the path for later save().
     *
     * If the file opens, parses it; if it is missing, seeds the default user settings and immediately writes them
     * so first run materializes a discoverable, hand-editable file.
     */
    void load(const std::filesystem::path &path);
    /**
     * Truncates and rewrites the whole file at the load() path.
     *
     * Creates the parent directory first (best-effort); the ordered maps make the output deterministic.
     */
    void save() const;

    /** Value at [section] key. @return `fallback` when the section or key is absent */
    [[nodiscard]] std::string
    getString(const std::string &section, const std::string &key, const std::string &fallback) const;
    /** Integer value at [section] key. @return `fallback` when absent or not parseable as an int */
    [[nodiscard]] int getInt(const std::string &section, const std::string &key, int fallback) const;
    /**
     * Section names beginning with `prefix` (the prefix is included in each returned name), in the stored sorted
     * order.
     *
     * Lets Platform discover user-defined [source.NAME] sections without Settings knowing anything about their
     * schema.
     */
    [[nodiscard]] std::vector<std::string> getSectionNames(const std::string &prefix) const;
    /** Sets [section] key in memory only; callers call save() explicitly after a batch of changes. */
    void setString(const std::string &section, const std::string &key, const std::string &value);
    /** Sets [section] key in memory only; callers call save() explicitly after a batch of changes. */
    void setInt(const std::string &section, const std::string &key, int value);

private:
    /** Parses the INI grammar from input into m_data; malformed lines are logged and skipped, never thrown on. */
    void parse(std::istream &input);
    /** Seeds the default user settings for a first run. */
    void applyDefaults();
};


#endif //OSP2_SETTINGS_H
