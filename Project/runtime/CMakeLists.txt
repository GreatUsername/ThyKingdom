# Everything that touches the machine lives here, outside core.
#
# The scheduler is pure arithmetic and reads no clock, but it turns real elapsed time
# into step counts, so it sits on this side of the boundary: core never sees real
# time, not even as a number. The time source is the only place a wall clock is read.
set(HEADERS
  include/runtime/scheduler.h
  include/runtime/time_source.h
  include/runtime/timer.h
)

add_library(runtime STATIC
  src/scheduler.cpp
  src/time_source.cpp
  src/timer.cpp
  ${HEADERS}
)

target_include_directories(runtime PUBLIC include)
target_link_libraries(runtime PUBLIC core)

if(MSVC)
  target_compile_options(runtime PRIVATE /W4)
else()
  target_compile_options(runtime PRIVATE -Wall -Wextra)
endif()
