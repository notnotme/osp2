cmake_minimum_required(VERSION 3.28.3)

project(OSP2 VERSION 1.0.0 DESCRIPTION "A simple chiptune player.")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
# ========================================
if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp)
    message(FATAL_ERROR "Dear ImGui submodule missing — run: git submodule update --init")
endif ()

include_directories(
    external/imgui
)

add_executable(OSP2
    # Dear ImGui (git submodule; the OpenGL3 backend is compiled through
    # src/gui/imgui_impl_opengl3_glad.cpp to inject glad on the Switch)
    external/imgui/imgui.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/backends/imgui_impl_sdl2.cpp
    src/gui/imgui_impl_opengl3_glad.cpp
    # OSP2 sources
    src/Application.cpp
    src/Platform.cpp
    src/filesystem/FileSystem.cpp
    src/filesystem/FtpDataSource.cpp
    src/filesystem/LocalDataSource.cpp
    src/gui/Gui.cpp
    src/input/CursorEmulator.cpp
    src/player/PlayerController.cpp
    src/player/Charset.cpp
    src/player/SongLengthDb.cpp
    src/player/plugins/OpenMptPlugin.cpp
    src/player/plugins/GmePlugin.cpp
    src/player/plugins/SidPlugin.cpp
    src/player/plugins/Sc68Plugin.cpp
    src/playlist/PlayList.cpp
    src/settings/Settings.cpp
    src/visualizer/VisualizerController.cpp
    src/visualizer/visualizers/BarsVisualizer.cpp
    src/visualizer/visualizers/ShaderQuadVisualizer.cpp
    src/main.cpp
)

# Define ImGui specific build options to use on the Switch
if (NINTENDO_SWITCH)
    add_compile_definitions(
        -DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
        -DIMGUI_IMPL_OPENGL_LOADER_CUSTOM
    )
endif ()

# libsc68 references a plain `basename' symbol that devkitA64's newlib declares but does not
# provide; supply it on the Switch only (see switch_compat.c). Desktop libc already has it.
if (NINTENDO_SWITCH)
    target_sources(OSP2 PRIVATE src/player/plugins/switch_compat.c)
endif ()

# App version surfaced to the About box. OSP_VERSION comes from project(VERSION);
# OSP_GIT_REV is a best-effort short commit hash ("" when git or the repo is unavailable,
# so the build string degrades cleanly).
set(OSP_GIT_REV "")
find_package(Git QUIET)
if (Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse --short HEAD
        OUTPUT_VARIABLE OSP_GIT_REV
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif ()
target_compile_definitions(OSP2 PRIVATE
    OSP_VERSION="${PROJECT_VERSION}"
    OSP_GIT_REV="${OSP_GIT_REV}"
)

# Formatting / linting (developer tooling; decoupled from the normal build)
# ========================================
# Operate on a glob of src/ — never external/ or cmake-build-* — rather than the
# executable's source list, because src/player/plugins/switch_compat.c is added
# conditionally (Switch only) yet should still be formatted/linted everywhere.
file(GLOB_RECURSE OSP2_SOURCE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

find_program(CLANG_FORMAT_EXE NAMES clang-format-18 clang-format)
if (CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i --style=file ${OSP2_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "clang-format: rewriting src/ in place"
        VERBATIM
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror --style=file ${OSP2_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "clang-format: checking src/ (no changes written)"
        VERBATIM
    )
endif ()

# `tidy` needs a compile database; export one so the target works from any build dir.
# Lint the .cpp translation units only (they are all in the desktop compile
# database); headers are covered transitively via .clang-tidy's HeaderFilterRegex,
# and the Switch-only switch_compat.c is absent from the desktop build by design.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
file(GLOB_RECURSE OSP2_TIDY_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
find_program(CLANG_TIDY_EXE NAMES clang-tidy-18 clang-tidy)
if (CLANG_TIDY_EXE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${OSP2_TIDY_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "clang-tidy: linting src/ (uses .clang-tidy)"
        VERBATIM
    )
endif ()

# Dependencies
# ========================================
find_package(PkgConfig REQUIRED)

# SDL2
find_package(SDL2 REQUIRED)
target_link_libraries(OSP2 PRIVATE SDL2::SDL2main SDL2::SDL2)

# SDL2_image
if (NINTENDO_SWITCH)
    pkg_check_modules(SDL2_image REQUIRED IMPORTED_TARGET SDL2_image)
    target_link_libraries(OSP2 PRIVATE PkgConfig::SDL2_image)
else ()
    find_package(SDL2_image REQUIRED)
    target_link_libraries(OSP2 PRIVATE SDL2_image::SDL2_image)
endif ()

# OpenGL
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
target_link_libraries(OSP2 PRIVATE OpenGL::GL)

# glad
if (NINTENDO_SWITCH)
    pkg_check_modules(GLAD REQUIRED libglad)
    target_include_directories(OSP2 PRIVATE ${GLAD_INCLUDE_DIRS})
    target_link_libraries(OSP2 PRIVATE ${GLAD_LIBRARIES})
else ()
    find_package(glad CONFIG REQUIRED)
    target_link_libraries(OSP2 PRIVATE glad::glad)
endif ()


# libgme
pkg_check_modules(libgme REQUIRED IMPORTED_TARGET libgme)
target_link_libraries(OSP2 PRIVATE PkgConfig::libgme)

# libsidplayfp
pkg_check_modules(libsidplayfp REQUIRED IMPORTED_TARGET libsidplayfp)
target_link_libraries(OSP2 PRIVATE PkgConfig::libsidplayfp)

# libsc68 (+ file68 + unice68)
pkg_check_modules(sc68 REQUIRED IMPORTED_TARGET sc68)
target_link_libraries(OSP2 PRIVATE PkgConfig::sc68)

# libopenmpt
pkg_check_modules(libopenmpt REQUIRED IMPORTED_TARGET libopenmpt)
target_link_libraries(OSP2 PRIVATE PkgConfig::libopenmpt)

# libcurl (FtpDataSource)
pkg_check_modules(libcurl REQUIRED IMPORTED_TARGET libcurl)
target_link_libraries(OSP2 PRIVATE PkgConfig::libcurl)

# NRO config
# ========================================
if (NINTENDO_SWITCH)
    nx_generate_nacp(OSP2.nacp
            NAME ${CMAKE_PROJECT_NAME}
            VERSION ${CMAKE_PROJECT_VERSION}
            AUTHOR "romain337"
    )
    nx_create_nro(OSP2
            NACP OSP2.nacp
            ROMFS romfs
            ICON icon.jpg
    )
endif ()
