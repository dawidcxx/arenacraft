CollectSourceFiles(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        PRIVATE_SOURCES
)

add_executable(
        unit_tests
        ${PRIVATE_SOURCES}
)

target_link_libraries(
        unit_tests
        game
        game-interface
)

enable_testing()

add_test(NAME srcTests COMMAND unit_tests)