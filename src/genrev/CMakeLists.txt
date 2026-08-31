set(VERSION_DATA "${CMAKE_CURRENT_BINARY_DIR}/VersionData.h")

if(NOT WITHOUT_GIT AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
    set(GIT_STATE_FILES)
    foreach(state HEAD index)
        if(EXISTS "${CMAKE_SOURCE_DIR}/.git/${state}")
            list(APPEND GIT_STATE_FILES "${CMAKE_SOURCE_DIR}/.git/${state}")
        endif()
    endforeach()

    if(GIT_STATE_FILES)
        add_custom_command(
            OUTPUT "${VERSION_DATA}"
            COMMAND "${CMAKE_COMMAND}"
                    -DBUILDDIR=${CMAKE_BINARY_DIR}
                    -DCMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
                    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                    -DCONF_INSTALL_DIR=${CONF_INSTALL_DIR}
                    -DHOST_SYSTEM_NAME=${CMAKE_HOST_SYSTEM_NAME}
                    -DHOST_SYSTEM_VERSION=${CMAKE_HOST_SYSTEM_VERSION}
                    -P "${CMAKE_SOURCE_DIR}/cmake/GenRevision.cmake"
            DEPENDS ${GIT_STATE_FILES}
                    "${CMAKE_CURRENT_SOURCE_DIR}/VersionData.h.in"
                    "${CMAKE_SOURCE_DIR}/cmake/Version.cmake"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Refreshing VersionData.h (repository HEAD moved)"
            VERBATIM
        )
    endif()
endif()

set_source_files_properties("${VERSION_DATA}" PROPERTIES GENERATED TRUE)

add_library(mangos_version STATIC
    Version.cpp
    Version.h
    "${VERSION_DATA}"
)

target_include_directories(mangos_version
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)
