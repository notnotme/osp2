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

// Wrapper around the submodule's OpenGL3 backend (external/imgui) so the
// submodule stays pristine. On the Switch the embedded imgl3w loader cannot
// work (no dlopen); IMGUI_IMPL_OPENGL_LOADER_CUSTOM (set in CMakeLists.txt)
// skips it and glad, included here first, provides the GL symbols instead.
#if defined(__SWITCH__)
#include <glad/glad.h>
#endif
#include <backends/imgui_impl_opengl3.cpp>
