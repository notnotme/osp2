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

#ifndef OSP2_PLAY_RESULT_H
#define OSP2_PLAY_RESULT_H


/**
 * Outcome of a play() request, consumed once by the application layer.
 *
 * - Ok — the module opened and playback started.
 * - Unsupported — no plugin matched the extension (decided synchronously in play()).
 * - DecodeError — a matching plugin failed to parse the module (published by the load worker).
 *
 * A cancelled load produces NO result (see PlayerController::cancelLoad).
 */
enum class PlayResult { Ok, Unsupported, DecodeError };


#endif //OSP2_PLAY_RESULT_H
