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

#ifndef OSP2_UI_ACTIONS_H
#define OSP2_UI_ACTIONS_H

#include <functional>

#include "ButtonId.h"
#include "../filesystem/FileEntry.h"


// Callback bundle wired once at startup; the UI reports user intent through these.
struct UiActions {
    std::function<void(ButtonId)> onButtonClick;
    std::function<void(const FileEntry &)> onFileClick;
    std::function<void(const FileEntry &)> onDirectoryClick;
};


#endif //OSP2_UI_ACTIONS_H
