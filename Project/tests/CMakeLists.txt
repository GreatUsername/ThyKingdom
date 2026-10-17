add_executable(tests main.cpp)
target_link_libraries(tests PRIVATE core)

# The scenario directory is passed as an argument rather than compiled in with a
# macro: one less define, and the binary can be pointed at another set of
# scenarios without a rebuild.
add_test(NAME tests COMMAND tests "${CMAKE_CURRENT_SOURCE_DIR}/../scenarios")
