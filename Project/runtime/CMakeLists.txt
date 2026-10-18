# Everything that touches the machine lives here, outside core.
#
# The scheduler below is pure arithmetic and reads no clock, but it exists to turn
# real elapsed time into step counts, so it sits on this side of the boundary: core
# never sees real time, not even as a number. The timer itself will join this
# library.
set(HEADERS
  include/runtime/scheduler.h
)

add_library(runtime STATIC
  src/scheduler.cpp
  ${HEADERS}
)

target_include_directories(runtime PUBLIC include)
target_link_libraries(runtime PUBLIC core)

if(MSVC)
  target_compile_options(runtime PRIVATE /W4)
else()
  target_compile_options(runtime PRIVATE -Wall -Wextra)
endif()
