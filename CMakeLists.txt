cmake_minimum_required(VERSION 3.16)

project(i_forgor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(IFG_BUILD_TESTS    "Build the headless smoke test"   ON)
option(IFG_WERROR         "Treat warnings as errors"        OFF)

set(IFG_AUTHOR   "kirbx01" CACHE STRING "Name shown in the credits")
set(IFG_ITCH_URL ""       CACHE STRING "itch.io page, if there is one")

set(IFG_SOURCE_DIRS
    src
    src/game
    src/ui
    src/sound
    src/input
    src/platform)

file(GLOB_RECURSE IFG_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER IFG_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")

set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/raylib" CACHE PATH "raylib checkout")
set(RAYGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/raygui" CACHE PATH "raygui checkout")

foreach(required "${RAYLIB_DIR}/CMakeLists.txt" "${RAYGUI_DIR}/src/raygui.h")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR
            "${required} is missing. Run scripts/fetch_deps.sh to pull raylib and raygui "
            "into external/, or point -DRAYLIB_DIR/-DRAYGUI_DIR at existing checkouts.")
    endif()
endforeach()

if(EMSCRIPTEN AND NOT PLATFORM)
    set(PLATFORM "Web" CACHE STRING "" FORCE)
endif()

add_subdirectory("${RAYLIB_DIR}" raylib EXCLUDE_FROM_ALL)

add_library(raygui INTERFACE)
target_include_directories(raygui INTERFACE "${RAYGUI_DIR}/src")
target_compile_definitions(raygui INTERFACE SUPPORT_MODULE_RTEXTURES)
add_compile_definitions(SUPPORT_MODULE_RAAC)

add_library(ifg_core STATIC ${IFG_SOURCES})
set_property(TARGET raylib raygui ifg_core PROPERTY INTERFACE_COMPILE_DEFINITIONS SUPPORT_MODULE_RAAC)
target_include_directories(ifg_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sound"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform"
    "${RAYGUI_DIR}/src")
target_link_libraries(ifg_core PUBLIC raygui)
target_compile_definitions(ifg_core PUBLIC
    IFG_AUTHOR="${IFG_AUTHOR}"
    IFG_ITCH_URL="${IFG_ITCH_URL}")
target_link_libraries(ifg_core PUBLIC raylib)
target_compile_options(ifg_core PRIVATE -Wall -Wextra)
if(IFG_WERROR)
    target_compile_options(ifg_core PRIVATE -Werror)
endif()
if(EMSCRIPTEN)
    target_compile_definitions(ifg_core PUBLIC PLATFORM_WEB)
endif()

add_executable(i_forgor src/main.cpp)
target_link_libraries(i_forgor PRIVATE ifg_core)
set_target_properties(i_forgor PROPERTIES
    OUTPUT_NAME "i_forgor"
    WIN32_EXECUTABLE OFF)

if(EMSCRIPTEN)
    set(IFG_WEB_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH
        "Folder preloaded into the web build")

    target_link_options(i_forgor PRIVATE
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
        "--preload-file=${IFG_WEB_ASSET_DIR}@assets")
    set_target_properties(i_forgor PROPERTIES SUFFIX ".html")
endif()

if(WIN32)
    target_compile_definitions(ifg_core PUBLIC
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN)
    target_link_libraries(ifg_core PUBLIC shell32)
endif()

if(IFG_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()

    add_executable(smoke_test
        tests/smoke.cpp
        src/game/board.cpp
        src/game/trail.cpp
        src/game/story.cpp
        src/game/game.cpp
        src/game/persist.cpp
        src/platform/platform.cpp)
    target_include_directories(smoke_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/sound"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/input"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform"
        "${RAYGUI_DIR}/src"
        "${RAYLIB_DIR}/src")
    target_compile_definitions(smoke_test PRIVATE
        IFG_AUTHOR="${IFG_AUTHOR}"
        IFG_ITCH_URL="${IFG_ITCH_URL}")
    add_test(NAME smoke COMMAND smoke_test)
    set_tests_properties(smoke PROPERTIES
        ENVIRONMENT "SAVE_DIR=${CMAKE_BINARY_DIR}/test-save")
endif()
