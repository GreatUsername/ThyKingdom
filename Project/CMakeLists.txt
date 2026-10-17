cmake_minimum_required(VERSION 3.20)

project(ThyKingdom LANGUAGES CXX)

# The simulation core takes no third-party dependencies and knows nothing about
# rendering: see Codex note 08 (Tests and engineering). The engine choice (OD-2)
# is deliberately absent here, because M1 and M2 need no engine at all.
#
# C++17 rather than 20: the skeleton uses nothing newer, and 17 builds on every
# compiler that matters, including older MinGW. Raising it later is one line, with
# nothing in the current code to change.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

enable_testing()

add_subdirectory(core)
add_subdirectory(runner)
add_subdirectory(tests)
