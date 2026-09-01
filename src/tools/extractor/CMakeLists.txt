set(MANGOS_CLIENT_NAME "World of Warcraft 1.12.x")

set(SRC_GRP_CLIENT
    client/AdtParser.cpp
    client/AdtParser.hpp
    client/ChunkReaders.hpp
    client/IMpqArchive.hpp
    client/M2Parser.cpp
    client/M2Parser.hpp
    client/WdtParser.cpp
    client/WdtParser.hpp
    client/WmoParser.cpp
    client/WmoParser.hpp
)
source_group("client" FILES ${SRC_GRP_CLIENT})

add_library(extractor_client STATIC ${SRC_GRP_CLIENT})

target_include_directories(extractor_client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/client
)

target_link_libraries(extractor_client
    PUBLIC
        terrain
)

set_target_properties(extractor_client PROPERTIES FOLDER "tools")

set(SRC_GRP_NAV
    nav/NavMeshBuilder.cpp
    nav/NavMeshBuilder.hpp
)
source_group("nav" FILES ${SRC_GRP_NAV})

set(SRC_GRP_DATA
    client/ModelLoaders.cpp
    client/ModelLoaders.hpp
    client/MpqTileSource.cpp
    client/MpqTileSource.hpp
    stores/LiquidTypeStore.hpp
    stores/MapDbcStore.hpp
    stores/MpqDbcLoader.hpp
    ${CMAKE_SOURCE_DIR}/src/shared/DataStores/DBCFileLoader.cpp
)
source_group("data" FILES ${SRC_GRP_DATA})

add_library(extractor_data STATIC ${SRC_GRP_DATA} ${SRC_GRP_NAV})

target_include_directories(extractor_data
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/shared
        ${CMAKE_SOURCE_DIR}/src/game

        ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers
)

target_link_libraries(extractor_data
    PUBLIC
        extractor_client
        RecastNavigation::Recast
        RecastNavigation::Detour
)

set_target_properties(extractor_data PROPERTIES FOLDER "tools")

add_library(extractor_mpq STATIC
        client/StormLibArchive.cpp
        client/StormLibArchive.hpp
    )

target_link_libraries(extractor_mpq
    PUBLIC
        extractor_data
    PRIVATE
        stormlib
)

set_target_properties(extractor_mpq PROPERTIES FOLDER "tools")

add_executable(mangos-extractor
    Extractor.cpp
    ExtractorConsole.cpp
    ExtractorConsole.hpp
    ${CMAKE_SOURCE_DIR}/src/shared/Console/ConsoleUI.cpp
    ${CMAKE_SOURCE_DIR}/src/shared/Console/Terminal.cpp
    $<$<BOOL:${WIN32}>:${CMAKE_CURRENT_SOURCE_DIR}/mangos-extractor.rc>
)

target_include_directories(mangos-extractor
    PRIVATE ${CMAKE_SOURCE_DIR}/src/shared/Console)

target_compile_definitions(mangos-extractor
    PRIVATE MANGOS_CLIENT_NAME="${MANGOS_CLIENT_NAME}")

target_link_libraries(mangos-extractor
    PRIVATE
        extractor_mpq
        mangos_version

        Threads::Threads

        $<$<BOOL:${WIN32}>:shell32>
        $<$<BOOL:${WIN32}>:ole32>
)

if(WIN32)
    target_include_directories(mangos-extractor PRIVATE ${CMAKE_SOURCE_DIR}/src/shared)
    if(TARGET revision_data.h)
        add_dependencies(mangos-extractor revision_data.h)
    endif()
endif()

set_target_properties(mangos-extractor PROPERTIES FOLDER "tools")
install(TARGETS mangos-extractor DESTINATION ${BIN_DIR}/tools)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/vessels.txt
              ${CMAKE_CURRENT_SOURCE_DIR}/offmesh.txt
        DESTINATION ${BIN_DIR}/tools)

add_custom_command(TARGET mangos-extractor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/vessels.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/offmesh.txt
        $<TARGET_FILE_DIR:mangos-extractor>)

if(WIN32)
    add_executable(mangos-extractor-gui WIN32
        ExtractorGui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mangos-extractor-gui.rc)
    target_link_libraries(mangos-extractor-gui PRIVATE mangos_version shell32 ole32 oleaut32 comctl32 comdlg32)

    target_include_directories(mangos-extractor-gui PRIVATE ${CMAKE_SOURCE_DIR}/src/shared)
    if(TARGET revision_data.h)
        add_dependencies(mangos-extractor-gui revision_data.h)
    endif()
    target_compile_definitions(mangos-extractor-gui
        PRIVATE MANGOS_CLIENT_NAME="${MANGOS_CLIENT_NAME}")
    set_target_properties(mangos-extractor-gui PROPERTIES FOLDER "tools")
    install(TARGETS mangos-extractor-gui DESTINATION ${BIN_DIR}/tools)
endif()
