# doctest 2.4.11 -- MIT, see LICENSE.txt. Vendored as the single header it ships
# as: there is nothing to compile and nothing to link, so this stays an INTERFACE
# target and adds no library to any binary that ships.

add_library(doctest INTERFACE)

target_include_directories(doctest
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# The test runner defines DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN in exactly one TU.
# Everything else including the header gets declarations only.
