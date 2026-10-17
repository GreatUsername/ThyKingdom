add_library(tk_core STATIC
  src/scenario.cpp
)

target_include_directories(tk_core PUBLIC include)

if(MSVC)
  target_compile_options(tk_core PRIVATE /W4)
else()
  target_compile_options(tk_core PRIVATE -Wall -Wextra)
endif()
