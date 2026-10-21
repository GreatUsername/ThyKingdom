# Headers are listed on purpose. They are not compiled on their own, but a target
# that mentions only its .cpp files shows no headers in an IDE, which makes the
# project look empty.
set(HEADERS
  include/core/clock.h
  include/core/season.h
  include/core/month_profile.h
  include/core/accumulator.h
  include/core/ini.h
  include/core/control_signal.h
  include/core/system.h
  include/core/plants.h
  include/core/weather.h
  include/core/world.h
  include/core/evaluator.h
  include/core/hash.h
  include/core/rng.h
  include/core/scenario.h
)

add_library(core STATIC
  src/ini.cpp
  src/scenario.cpp
  src/season.cpp
  src/month_profile.cpp
  src/world.cpp
  src/evaluator.cpp
  src/plants.cpp
  src/weather.cpp
  ${HEADERS}
)

target_include_directories(core PUBLIC include)

if(MSVC)
  target_compile_options(core PRIVATE /W4)
else()
  target_compile_options(core PRIVATE -Wall -Wextra)
endif()
