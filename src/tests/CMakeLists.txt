# Unit tests. Built only when WITH_TESTS is on, and never linked into a shipping
# binary: doctest is an INTERFACE target carrying a single header, so nothing
# here reaches mangosd or realmd.
#
# The layout mirrors the subsystem it exercises -- wire/, crypto/, terrain/,
# combat/ -- so a failure names the layer before it names the file.

set(TEST_SOURCES
    test_main.cpp
    test_globals.cpp

    wire/test_authcrypt.cpp
    wire/test_packetcodec.cpp

    terrain/test_column.cpp
    terrain/test_fusedterrain.cpp
    terrain/test_store.cpp
    terrain/test_mappedfile.cpp
    terrain/test_tileserializer.cpp
    terrain/test_preload.cpp

    pose/test_spatial_boundary.cpp
    pose/test_numeric_edges.cpp
    pose/test_arc.cpp

    wire/test_updatedata.cpp
    wire/test_fieldtable.cpp

    unit/test_diminishing.cpp
    unit/test_sides.cpp
    unit/test_itemsavequeue.cpp
    unit/test_capturepoint.cpp
    unit/test_usertally.cpp
    unit/test_liftpath.cpp
    unit/test_vesselroute.cpp
    unit/test_mineralvein.cpp
    unit/test_spawnclock.cpp
    unit/test_questbond.cpp
    unit/test_trapsight.cpp
    unit/test_creaturenumbers.cpp
    unit/test_petnumbers.cpp
    unit/test_playernumbers.cpp
    unit/test_chances.cpp

    combat/test_school.cpp
    combat/test_roll.cpp
    combat/test_resolve.cpp
    combat/test_hitqueue.cpp
    combat/test_flags.cpp

    metrics/test_metrics.cpp

    synthetic/test_syntheticlink.cpp

    util/test_idlist.cpp
    combat/test_manashield.cpp

    spells/test_auraindex.cpp
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

# The game library links the MySQL client, so the test binary does too and needs
# the DLL beside it on Windows. Copied rather than left to PATH so that ctest
# works in a fresh build tree.
if(WIN32)
    get_filename_component(MYSQL_LIB_DIR ${MySQL_LIBRARIES} DIRECTORY)
    get_filename_component(MYSQL_LIB_DLL ${MySQL_LIBRARIES} NAME)
    string(REPLACE ".lib" ".dll" MYSQL_LIB_DLL ${MYSQL_LIB_DLL})

    add_custom_command(TARGET mangos_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MYSQL_LIB_DIR}/${MYSQL_LIB_DLL}"
                "$<TARGET_FILE_DIR:mangos_tests>"
    )
endif()
