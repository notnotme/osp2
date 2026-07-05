cmake_minimum_required(VERSION 3.28.3)

project(OSP2 VERSION 0.0.0 DESCRIPTION "A simple chiptune player.")
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
    src/filesystem/FileSystem.cpp
    src/filesystem/FtpDataSource.cpp
    src/filesystem/LocalDataSource.cpp
    src/gui/Gui.cpp
    src/player/PlayerController.cpp
    src/player/plugins/OpenMptPlugin.cpp
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


# libgme (re-enable with GmePlugin)
#pkg_check_modules(libgme REQUIRED IMPORTED_TARGET libgme)
#target_link_libraries(OSP2 PRIVATE PkgConfig::libgme)

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
