set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

find_program(IFG_MINGW_CXX NAMES x86_64-w64-mingw32-g++ mingw32-g++)
find_program(IFG_MINGW_C   NAMES x86_64-w64-mingw32-gcc mingw32-gcc)
find_program(IFG_MINGW_AR  NAMES x86_64-w64-mingw32-ar  mingw32-ar)
find_program(IFG_MINGW_RC  NAMES x86_64-w64-mingw32-windres mingw32-windres)

if(NOT IFG_MINGW_CXX OR NOT IFG_MINGW_C OR NOT IFG_MINGW_RC)
    message(FATAL_ERROR
    "MinGW-w64 C/C++ or resource compiler is missing from PATH. Install mingw-w64, "
    "unpack winlibs, or point PATH at the directory holding the prefixed tools.")
endif()

get_filename_component(IFG_MINGW_BIN_DIR "${IFG_MINGW_CXX}" DIRECTORY)

set(CMAKE_C_COMPILER   "${IFG_MINGW_C}"               CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${IFG_MINGW_CXX}"              CACHE FILEPATH "")
set(CMAKE_RC_COMPILER  "${IFG_MINGW_RC}"              CACHE FILEPATH "")
set(CMAKE_AR           "${IFG_MINGW_AR}"               CACHE FILEPATH "")

set(CMAKE_FIND_ROOT_PATH "${IFG_MINGW_BIN_DIR}/..")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

set(CMAKE_C_FLAGS_INIT   "-D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN")
set(CMAKE_CXX_FLAGS_INIT "-D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN")