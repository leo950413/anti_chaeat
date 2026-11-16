cmake_minimum_required(VERSION 3.16)
project(ExternalTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Injected DLL Module
add_library(misc_dll SHARED misc_dll.cpp)
target_link_libraries(misc_dll PRIVATE user32)

# 2. Remote Function Caller (StackOverflow 19798912)
add_executable(RemoteCaller remote_caller.cpp)

# 3. External Memory Modifier Tool
add_executable(MemoryModifier memory_modifcication.cpp)
