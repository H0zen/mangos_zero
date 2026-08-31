if(NOT BUILDDIR)
  set(BUILDDIR ${CMAKE_BINARY_DIR})
endif()

include("${CMAKE_SOURCE_DIR}/cmake/Version.cmake")

if(NOT HOST_SYSTEM_NAME)
    set(HOST_SYSTEM_NAME "${CMAKE_HOST_SYSTEM_NAME}")
endif()
if(NOT HOST_SYSTEM_VERSION)
    set(HOST_SYSTEM_VERSION "${CMAKE_HOST_SYSTEM_VERSION}")
endif()
if(NOT CONF_INSTALL_DIR)
    message(FATAL_ERROR
        "CONF_INSTALL_DIR is empty; VersionData.h would claim the configuration "
        "files live at the filesystem root.")
endif()

if(WITHOUT_GIT)
  set(rev_date            "1970-01-01 00:00:00 +0000" )
  set(rev_hash            "unknown"                   )
  set(rev_branch          "Archived"                  )

  string(TIMESTAMP rev_date_fallback "%Y-%m-%d %H:%M:%S" UTC)
else()
  if(GIT_EXECUTABLE)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --long --match init --dirty=+ --abbrev=12 --always
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE rev_info
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    execute_process(
      COMMAND "${GIT_EXECUTABLE}" show -s --format=%ci
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE rev_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE rev_branch
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

  endif()

  if(NOT rev_info)
    message(STATUS "
    Could not find a proper repository signature (hash) - you may need to pull tags with git fetch -t
    Continuing anyway - note that the versionstring will be set to \"unknown 1970-01-01 00:00:00 (Archived)\"")
    set(rev_date          "1970-01-01 00:00:00 +0000" )
    set(rev_hash          "unknown"                   )
    set(rev_branch        "Archived"                  )

    string(TIMESTAMP rev_date_fallback "%Y-%m-%d %H:%M:%S" UTC)
  else()
    set(rev_date_fallback ${rev_date})

    string(REGEX REPLACE init-|[0-9]+-g "" rev_hash ${rev_info})
  endif()
endif()

string(REGEX MATCH "([0-9]+)-([0-9]+)-([0-9]+)" rev_date_fallback_match ${rev_date_fallback})
set(rev_year  ${CMAKE_MATCH_1})
set(rev_month ${CMAKE_MATCH_2})
set(rev_day   ${CMAKE_MATCH_3})

set(version_stamp
  "${MANGOS_VERSION}|${MANGOS_PKG}\
|${MANGOS_DB_REALMD_VERSION}.${MANGOS_DB_REALMD_STRUCTURE}.${MANGOS_DB_REALMD_CONTENT}.${MANGOS_DB_REALMD_DESCRIPT}\
|${MANGOS_DB_CHAR_VERSION}.${MANGOS_DB_CHAR_STRUCTURE}.${MANGOS_DB_CHAR_CONTENT}.${MANGOS_DB_CHAR_DESCRIPT}\
|${MANGOS_DB_WORLD_VERSION}.${MANGOS_DB_WORLD_STRUCTURE}.${MANGOS_DB_WORLD_CONTENT}.${MANGOS_DB_WORLD_DESCRIPT}")

if(
     NOT "${rev_hash_cached}"           STREQUAL "${rev_hash}"
  OR NOT "${rev_branch_cached}"         STREQUAL "${rev_branch}"
  OR NOT "${version_stamp_cached}"      STREQUAL "${version_stamp}"
  OR NOT EXISTS "${BUILDDIR}/src/genrev/VersionData.h"
)
  configure_file(
    "${CMAKE_SOURCE_DIR}/src/genrev/VersionData.h.in"
    "${BUILDDIR}/src/genrev/VersionData.h"
    @ONLY
  )
  set(rev_hash_cached           "${rev_hash}"           CACHE INTERNAL "Cached commit-hash"     )
  set(rev_branch_cached         "${rev_branch}"         CACHE INTERNAL "Cached branch name"     )

  set(version_stamp_cached      "${version_stamp}"      CACHE INTERNAL "Cached cmake/Version.cmake values")
endif()
