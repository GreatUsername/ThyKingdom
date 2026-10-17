add_executable(tk_tests main.cpp)
target_link_libraries(tk_tests PRIVATE tk_core)
target_compile_definitions(tk_tests PRIVATE
  TK_SCENARIO_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../scenarios"
)

add_test(NAME tk_tests COMMAND tk_tests)
