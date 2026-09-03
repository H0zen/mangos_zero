# Unit tests. Built only when WITH_TESTS is on, and never linked into a shipping
# binary: doctest is an INTERFACE target carrying a single header, so nothing
# here reaches mangosd or realmd.
#
# The layout mirrors the subsystem it exercises -- wire/, crypto/, terrain/,
# combat/ -- so a failure names the layer before it names the file.

set(TEST_SOURCES
    test_main.cpp

    wire/test_authcrypt.cpp
    wire/test_packetcodec.cpp

    terrain/test_column.cpp
    terrain/test_fusedterrain.cpp

    pose/test_spatial_boundary.cpp
    pose/test_numeric_edges.cpp
    pose/test_arc.cpp
)

add_executable(mangos_tests ${TEST_SOURCES})

target_link_libraries(mangos_tests
    PRIVATE
        doctest
        game
)

# `game` already carries `shared`, `terrain` and `geometry` as PUBLIC
# dependencies, so the tests reach AuthCrypt and the terrain queries through it
# without naming them again.

set_target_properties(mangos_tests
    PROPERTIES
        FOLDER "tests"
)

add_test(NAME mangos_tests COMMAND mangos_tests)
