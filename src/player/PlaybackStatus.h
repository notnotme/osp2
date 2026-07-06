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

#ifndef OSP2_PLAYBACK_STATUS_H
#define OSP2_PLAYBACK_STATUS_H

#include <string>

#include "PlayerState.h"


struct PlaybackStatus {
    PlayerState state;
    std::string title;      // from decoder metadata, may be empty
    std::string fileName;   // basename of the open file
    double positionSeconds; // 0 when stopped
    double durationSeconds; // 0 when stopped/unknown
};


#endif //OSP2_PLAYBACK_STATUS_H
