# Cross compile i forgor for 64-bit Windows with MinGW-w64.
#
#   cmake -S . -B build-mingw -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
#   cmake --build build-mingw -j
#
# The compiler is picked up from the environment, so both a normal install and the
# self-extracting archive from winlibs.com work:
#
#   PATH=/mingw64/bin:$PATH cmake -S . -B build-mingw \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

find_program(IFG_MINGW_CXX NAMES x86_64-w64-mingw32-g++ mingw32-g++)
find_program(IFG_MINGW_AR  NAMES x86_64-w64-mingw32-ar  mingw32-ar)

if(NOT IFG_MINGW_CXX)
    message(FATAL_ERROR
        "No MinGW-w64 g++ on PATH. Install mingw-w64, unpack winlibs, or point PATH at "
        "the bin directory that holds x86_64-w64-mingw32-g++.")
endif()

get_filename_component(IFG_MINGW_BIN_DIR "${IFG_MINGW_CXX}" DIRECTORY)

set(CMAKE_C_COMPILER   "${IFG_MINGW_BIN_DIR}/gcc"     CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${IFG_MINGW_CXX}"              CACHE FILEPATH "")
set(CMAKE_RC_COMPILER  "${IFG_MINGW_BIN_DIR}/windres"  CACHE FILEPATH "")
set(CMAKE_AR           "${IFG_MINGW_AR}"               CACHE FILEPATH "")

set(CMAKE_FIND_ROOT_PATH "${IFG_MINGW_BIN_DIR}/..")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# The Windows build is a single folder that someone can double click, so the DLLs have to
# land next to the executable.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

# raylib and the game agree on one CRT; mismatched heaps crash at shutdown rather than at
# build time, so the policy is pinned for every target in the tree.
set(CMAKE_C_FLAGS_INIT   "-D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN -municode")
set(CMAKE_CXX_FLAGS_INIT "-D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN")