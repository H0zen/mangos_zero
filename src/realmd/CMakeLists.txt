file(GLOB_RECURSE SRC_GRP_AUTH Auth/*.cpp Auth/*.h)
source_group("Authentication" FILES ${SRC_GRP_AUTH})

file(GLOB_RECURSE SRC_GRP_REALM Realm/*.cpp Realm/*.h)
source_group("Realmlist" FILES ${SRC_GRP_REALM})

configure_file(realmd.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/realmd.conf.dist)

add_executable(realmd
    ${SRC_GRP_AUTH}
    ${SRC_GRP_REALM}
    Main.cpp
    $<$<BOOL:${WIN32}>:${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.rc>
)

if(WIN32)
    target_compile_definitions(realmd PRIVATE MANGOS_RC_REALMD)
endif()

target_compile_features(realmd PUBLIC cxx_std_17)
set_target_properties(realmd PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(realmd
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(realmd
    PUBLIC
        shared
        Threads::Threads
        mangos_openssl
    PRIVATE
        mangos_openssl_strict
)

install(
    TARGETS realmd
    DESTINATION ${BIN_DIR}
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/realmd.conf.dist
    DESTINATION ${CONF_INSTALL_DIR}
)

if(WIN32 AND MSVC)
    install(
        FILES $<TARGET_PDB_FILE:realmd>
        DESTINATION ${BIN_DIR}
        OPTIONAL
        )
endif()
