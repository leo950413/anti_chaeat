cmake_minimum_required(VERSION 3.16)

# Enforce CMake 3.5+ policy minimum for modern CMake 3.31+ compatibility
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

project(AntiCheatLab LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 1. Automatically fetch and build Raylib 5.5 (compatible with modern CMake)
message(STATUS "Configuring Raylib...")
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
)
FetchContent_MakeAvailable(raylib)

# 2. Target Game Executable
add_executable(TargetGame
    src/main.cpp
    src/sha256.hpp
    src/anticheat.hpp
)

target_include_directories(TargetGame PRIVATE src)
target_link_libraries(TargetGame PRIVATE raylib)

# Copy the assets directory to the output directory after building
add_custom_command(TARGET TargetGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:TargetGame>/assets
)

# 3. External Tools Subdirectory
if (WIN32)
    add_subdirectory(tools)
endif()
