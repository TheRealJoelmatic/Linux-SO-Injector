cmake_minimum_required(VERSION 3.20)
project(SO_Injector)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

find_package(OpenGL REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)

add_executable(SO_Injector
        entry.cpp
        render/gui.cpp

        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp

        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        render/menu/menu.cpp
        render/menu/menu.h
        render/menu/theme.h
        render/menu/theme_manager.cpp
        render/menu/tabs/misc.cpp
        render/menu/tabs/tabs.h
        render/menu/tabs/homepage.cpp
        render/menu/tabs/injector.cpp
        render/injector/inject.cpp
        render/injector/inject.h
        render/injector/config.cpp
        render/injector/config.h
        render/menu/helpers/messageBox.cpp
        render/menu/helpers/messageBox.h
        render/injector/memory.cpp
        render/injector/memory.h
        render/injector/utils.cpp
        render/injector/utils.h
        render/menu/tabs/theme.cpp
)

target_include_directories(SO_Injector PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        render
        ${GLFW_INCLUDE_DIRS}
)

target_link_libraries(SO_Injector
        ${GLFW_LIBRARIES}
        OpenGL::GL
        dl
        pthread
        X11
)