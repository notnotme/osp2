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

#include "FileSystem.h"

FileSystem::FileSystem()
    : m_path("/sample/path/placeholder"),
      m_content({
       { "placeholder_folder_01", 0, true },
       { "placeholder_folder_02", 0, true },
       { "placeholder_folder_03", 0, true },
       { "placeholder_folder_04", 0, true },
       { "placeholder_1.snd", 33, false },
       { "placeholder_2.xm", 2, false },
       { "placeholder_3.sndh", 18, false },
       { "placeholder_4.sc68", 16, false },
       { "placeholder_5.sid", 24, false },
       { "placeholder_6.mod", 100, false },
       { "placeholder_7.it", 89, false },
       { "placeholder_8.xm", 3, false },
       { "placeholder_9.mod", 8, false },
       { "placeholder_10.sid", 7, false },
       { "placeholder_11.sid", 4, false },
       { "placeholder_12.sid", 12, false },
       { "placeholder_13.s3m", 10, false },
       { "placeholder_14.xm", 10, false },
       { "placeholder_15.snd", 11, false },
       { "placeholder_16.snd", 21, false },
       { "placeholder_17.sndh", 17, false },
       { "placeholder_18.sc68", 60, false },
       { "placeholder_19.sc68", 41, false },
       { "placeholder_20.sc68", 55, false },
       { "placeholder_21.mod", 8, false },
       { "placeholder_22.s3m", 8, false },
       { "placeholder_23.mod", 7, false },
       { "placeholder_24.s3m", 8, false },
       { "placeholder_25.mod", 2, false }
      }) {}

const std::filesystem::path &FileSystem::getPath() const {
    return m_path;
}

const std::vector<FileEntry> &FileSystem::getContent() const {
    return m_content;
}

bool FileSystem::isWorking() const {
    return false;
}
