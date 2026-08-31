add_subdirectory(shared/Geometry)
add_subdirectory(shared/terrain)

add_subdirectory(shared)

add_subdirectory(proto)

add_subdirectory(genrev)

if(BUILD_MANGOSD)
    add_subdirectory(game)
endif()

if(BUILD_MANGOSD)
    add_subdirectory(mangosd)
endif()

if(BUILD_MANGOSD)
    add_subdirectory(modules)
endif()

if(BUILD_REALMD)
    add_subdirectory(realmd)
endif()

add_subdirectory(tools/extractor)

add_subdirectory(tools/height-check)

if (BUILD_MANGOSD OR BUILD_REALMD)
    if(WIN32)
        get_filename_component(MYSQL_LIB_DIR ${MySQL_LIBRARIES} DIRECTORY)
        get_filename_component(MYSQL_LIB_DLL ${MySQL_LIBRARIES} NAME)
        STRING(REPLACE ".lib" ".dll" MYSQL_LIB_DLL ${MYSQL_LIB_DLL})
        install(FILES ${MYSQL_LIB_DIR}/${MYSQL_LIB_DLL} DESTINATION ${BIN_DIR})
    endif()
endif()
