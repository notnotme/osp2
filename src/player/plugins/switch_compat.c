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

/*
 * devkitA64's newlib declares basename() but ships no libc symbol for it,
 * while libsc68 (api68.c) references a plain `basename'. Provide it here for
 * the Switch build only. No <string.h>/<libgen.h> include: their asm aliases
 * would rename this symbol and defeat the purpose.
 */
char *basename(char *path) {
    char *last = path;
    if (path) {
        for (char *s = path; *s; ++s) {
            if (*s == '/') {
                last = s + 1;
            }
        }
    }
    return last;
}
