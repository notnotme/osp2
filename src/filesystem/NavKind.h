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

#ifndef OSP2_NAV_KIND_H
#define OSP2_NAV_KIND_H

/**
 * One-frame browser navigation signal: emitted when a directory listing is swapped in, so the Gui can reset scroll
 * to the top on descend and restore the saved offset on ascend.
 *
 * See docs/ui.md.
 */
enum class NavKind { None, Descend, Ascend };

#endif //OSP2_NAV_KIND_H
