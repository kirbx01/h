cmake_minimum_required(VERSION 3.16)

# AFTER THE FALL
#
# One build description for all three targets: Linux (and macOS) natively, Windows by
# cross compiling with cmake/mingw-w64-x86_64.cmake, and the web through emscripten.
# The raylib and raygui sources are expected under external/, which is not tracked; see
# scripts/fetch_deps.sh.

project(after_the_fall LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Everything lands next to the build directory rather than in /usr/local, so a build from
# a fresh clone cannot touch the system.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(ATF_BUILD_TESTS    "Build the headless smoke test"   ON)
option(ATF_BUILD_CAPTURE  "Build the screenshot tool"       OFF)
option(ATF_WERROR         "Treat warnings as errors"        OFF)

# Credits metadata. ATF_ITCH_URL is empty on purpose: the game will not invent a link, and
# assets/itch_url.txt points the credits at the real page instead.
set(ATF_AUTHOR   "kirbx01" CACHE STRING "Name shown in the credits")
set(ATF_ITCH_URL ""       CACHE STRING "itch.io page, if there is one")

set(ATF_SOURCE_DIRS
    src
    src/game
    src/ui
    src/sound
    src/input
    src/platform)

file(GLOB_RECURSE ATF_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER ATF_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")

# The engine and its UI kit are compiled from source here rather than linked from the
# system, so all three targets come out of the same tree.
set(RAYLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/raylib" CACHE PATH "raylib checkout")
set(RAYGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/raygui" CACHE PATH "raygui checkout")

foreach(required "${RAYLIB_DIR}/CMakeLists.txt" "${RAYGUI_DIR}/src/raygui.h")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR
            "${required} is missing. Run scripts/fetch_deps.sh to pull raylib and raygui "
            "into external/, or point -DRAYLIB_DIR/-DRAYGUI_DIR at existing checkouts.")
    endif()
endforeach()

# raylib picks its own backend from the generator, so nothing has to be forced here.
add_subdirectory("${RAYLIB_DIR}" raylib EXCLUDE_FROM_ALL)

# raygui is a single header and ships no build of its own.
add_library(raygui INTERFACE)
target_include_directories(raygui INTERFACE "${RAYGUI_DIR}/src")
target_compile_definitions(raygui INTERFACE SUPPORT_MODULE_RTEXTURES)

add_library(atf_core STATIC ${ATF_SOURCES})
target_include_directories(atf_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sound"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform"
    "${RAYGUI_DIR}/src")
target_link_libraries(atf_core PUBLIC raygui)
target_compile_definitions(atf_core PUBLIC
    ATF_AUTHOR="${ATF_AUTHOR}"
    ATF_ITCH_URL="${ATF_ITCH_URL}")
target_link_libraries(atf_core PUBLIC raylib)
target_compile_options(atf_core PRIVATE -Wall -Wextra)
if(ATF_WERROR)
    target_compile_options(atf_core PRIVATE -Werror)
endif()
if(EMSCRIPTEN)
    target_compile_definitions(atf_core PUBLIC PLATFORM_WEB)
endif()

add_executable(after_the_fall src/main.cpp)
target_link_libraries(after_the_fall PRIVATE atf_core)
set_target_properties(after_the_fall PROPERTIES
    OUTPUT_NAME "after_the_fall"
    WIN32_EXECUTABLE OFF)

if(EMSCRIPTEN)
    # A frame-synced game and a music stream both need the loop to yield; ASYNCIFY is what
    # lets the browser breathe between frames without changing the game loop.
    target_link_options(after_the_fall PRIVATE
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
        "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/assets@assets")
    set_target_properties(after_the_fall PROPERTIES SUFFIX ".html")
endif()

if(WIN32)
    # The music loader looks for assets next to the executable, and Windows players expect
    # the file they double click to be in a single folder.
    target_compile_definitions(atf_core PUBLIC
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN)
endif()

if(ATF_BUILD_TESTS AND NOT EMSCRIPTEN)
    enable_testing()

    # The test drives the simulation directly and supplies its own input, audio and drawing
    # stubs, so it links the simulation only. Nothing here opens a window or a sound card.
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
        ATF_AUTHOR="${ATF_AUTHOR}"
        ATF_ITCH_URL="${ATF_ITCH_URL}")
    add_test(NAME smoke COMMAND smoke_test)
    set_tests_properties(smoke PROPERTIES
        ENVIRONMENT "SAVE_DIR=${CMAKE_BINARY_DIR}/test-save")
endif()

if(ATF_BUILD_CAPTURE AND NOT EMSCRIPTEN)
    add_executable(capture tools/capture.cpp)
    target_link_libraries(capture PRIVATE atf_core)
    target_compile_definitions(capture PRIVATE
        OUTPUT_DIR="${CMAKE_BINARY_DIR}/shots")
    set_target_properties(capture PROPERTIES OUTPUT_NAME "capture")
endif()