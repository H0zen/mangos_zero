cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Version.cmake)

project(MaNGOS VERSION ${MANGOS_VERSION} LANGUAGES C CXX)

if(CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. Configure from a separate build "
        "directory, e.g.  cmake -S . -B ../build")
endif()

cmake_policy(SET CMP0028 NEW)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0063 NEW)

if(APPLE)
  cmake_policy(SET CMP0042 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_MANGOSD        "Build the main server"                         ON)
option(BUILD_REALMD         "Build the login server"                        ON)
option(BUILD_TOOLS          "Build the client-data extractor"               ON)
option(SCRIPT_LIB_SD3       "Compile with support for ScriptDev3 scripts"   ON)
option(PCH                  "Enable precompiled headers"                    ON)
option(WITH_IO_URING        "Use the io_uring network backend (Linux, needs liburing)" OFF)
option(DEBUG                "Enable debug build (only on non IDEs)"         OFF)
option(WITHOUT_GIT          "Disable Git revision detection"                OFF)
option(WITH_TESTS           "Build the unit tests (ctest)"                  ON)

if(WITH_TESTS)
    enable_testing()
endif()

if(NOT CMAKE_CONFIGURATION_TYPES)
    if(DEBUG)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    else()
        set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    endif()
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}_install CACHE PATH "Default install directory" FORCE)
endif()

include(GNUInstallDirs)

set(BIN_DIR "${CMAKE_INSTALL_FULL_BINDIR}")
set(CONF_INSTALL_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}")

if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT WITHOUT_GIT)
  find_package(Git QUIET)
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL 3.0 REQUIRED)

if(OPENSSL_VERSION VERSION_GREATER_EQUAL "4.0.0")
  message(FATAL_ERROR
    "Unsupported OpenSSL ${OPENSSL_VERSION}: MaNGOS currently requires >=3.0.0 and <4.0.0")
endif()
find_package(MySQL REQUIRED)

add_library(mangos_openssl INTERFACE)
target_link_libraries(mangos_openssl
    INTERFACE
        OpenSSL::SSL
        OpenSSL::Crypto
)

add_library(mangos_openssl_strict INTERFACE)
target_compile_definitions(mangos_openssl_strict
    INTERFACE
        OPENSSL_API_COMPAT=0x30000000L
        OPENSSL_NO_DEPRECATED
)

set(MANGOS_USE_IO_URING OFF)
if(WITH_IO_URING AND UNIX AND NOT APPLE)
    find_library(URING_LIBRARY NAMES uring)
    find_path(URING_INCLUDE_DIR NAMES liburing.h)
    if(URING_LIBRARY AND URING_INCLUDE_DIR)
        set(MANGOS_USE_IO_URING ON)
        message(STATUS "Networking backend: io_uring (${URING_LIBRARY})")
    else()
        message(WARNING "WITH_IO_URING was requested but liburing was not found; "
                        "falling back to the epoll reactor.")
    endif()
endif()

find_package(ZLIB QUIET)
find_package(BZip2 QUIET)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenRevision.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetDefinitions.cmake)

if(PCH)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PCHSupport.cmake)
endif()

add_subdirectory(dep)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
  add_subdirectory(src)
else()
  message(WARNING "Source directory 'src' not found. Nothing to build.")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/StatusInfo.cmake)
