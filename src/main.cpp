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

#include <cstdlib>

// SDL redefines main to SDL_main here (SDL2main is linked and provides the real entry point that
// calls it); keep this include so the entry-point wrapping stays intact.
#include <SDL.h>

#include "Platform.h"


int main(int, char **) {
    Platform platform;
    platform.create();
    platform.run();
    platform.destroy();
    return EXIT_SUCCESS;
}
