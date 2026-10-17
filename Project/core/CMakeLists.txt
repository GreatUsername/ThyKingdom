# Headers are listed on purpose. They are not compiled on their own, but a target
# that mentions only its .cpp files shows no headers in an IDE, which makes the
# project look empty.
set(HEADERS
  include/core/time.h
  include/core/hash.h
  include/core/rng.h
  include/core/world.h
  include/core/scenario.h
)

add_library(core STATIC
  src/scenario.cpp
  ${HEADERS}
)

target_include_directories(core PUBLIC include)

if(MSVC)
  target_compile_options(core PRIVATE /W4)
else()
  target_compile_options(core PRIVATE -Wall -Wextra)
endif()
