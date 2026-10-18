cmake_minimum_required(VERSION 3.20)

project(ThyKingdom LANGUAGES CXX)

# The simulation core takes no third-party dependencies and knows nothing about
# rendering: see Codex note 08 (Tests and engineering). The engine choice (OD-2)
# is deliberately absent, because M1 and M2 need no engine at all.
#
# C++17 rather than 20: nothing here uses anything newer, and 17 builds on every
# compiler that matters, including older MinGW. Raising it later is one line.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

enable_testing()

# core is the simulation and depends on nothing but the standard library.
add_subdirectory(core)

# config is application preferences. It depends on core only for the tiny INI
# reader; core never depends on config, which is what makes it impossible for a
# player setting to reach the simulation.
add_subdirectory(config)

# runtime is everything that touches the machine: pacing now, threads later. It sits
# above core, never below it.
add_subdirectory(runtime)

add_subdirectory(runner)
add_subdirectory(tests)
