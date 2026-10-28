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

# Profiling is opt-in and off in the shipped build. The simulation core takes no
# third-party dependencies (Codex note 08); with THYKINGDOM_PROFILE=ON, easy_profiler
# is built alongside and every system step becomes a named block. Turn it on when you
# need to know where time goes, not by default.
option(THYKINGDOM_PROFILE "Build with easy_profiler instrumentation" OFF)
set(THYKINGDOM_EASY_PROFILER_DIR "" CACHE PATH "Path to the easy_profiler source tree")

if(THYKINGDOM_PROFILE)
  if(NOT EXISTS "${THYKINGDOM_EASY_PROFILER_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
      "THYKINGDOM_PROFILE=ON requires THYKINGDOM_EASY_PROFILER_DIR to point at a "
      "checkout of https://github.com/yse/easy_profiler")
  endif()
  # The GUI needs Qt, and the core library is all we link against.
  set(EASY_PROFILER_NO_GUI ON CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${THYKINGDOM_EASY_PROFILER_DIR}" "${CMAKE_BINARY_DIR}/easy_profiler"
                   EXCLUDE_FROM_ALL)
endif()

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
