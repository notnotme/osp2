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

#ifndef OSP2_BUTTON_ID_H
#define OSP2_BUTTON_ID_H


/** User-intent button identifier reported through UiActions::onButtonClick (player-bar transport and top-bar Quit). */
enum ButtonId {
    PLAY_PAUSE, ///< Toggles between playing and paused; a no-op while stopped.
    STOP,       ///< Stops playback (also cancels an in-flight load).
    NEXT,       ///< Advances: next subtrack first, then the next file/playlist entry.
    PREVIOUS,   ///< Steps back: previous subtrack first, then the previous file/playlist entry.
    QUIT        ///< Requests application exit; handled by Platform's run loop.
};


#endif //OSP2_BUTTON_ID_H
