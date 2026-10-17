# Application preferences: what the player configured in the settings menu.
#
# This library sits above core and depends on it, not the other way round. The
# direction of that arrow is the guarantee: no file in core can even name
# Settings, so a presentation option cannot reach the simulation and cannot break
# replay (OD-34).
set(HEADERS
  include/config/settings.h
)

add_library(config STATIC
  src/settings.cpp
  ${HEADERS}
)

target_include_directories(config PUBLIC include)
target_link_libraries(config PRIVATE core)

if(MSVC)
  target_compile_options(config PRIVATE /W4)
else()
  target_compile_options(config PRIVATE -Wall -Wextra)
endif()
