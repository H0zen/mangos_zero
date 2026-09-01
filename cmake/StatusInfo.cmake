set(_w 24)

function(_status label value)
    string(LENGTH "${label}" _len)
    math(EXPR _pad "${_w} - ${_len}")
    if(_pad GREATER 0)
        string(REPEAT " " ${_pad} _spaces)
    else()
        set(_spaces "")
    endif()
    message("${label}${_spaces}: ${value}")
endfunction()

function(_status_sub label value)
    _status("  ${label}" "${value}")
endfunction()

message("")
message("======================================================================")

_status("${MANGOS_PKG}" "${MANGOS_VERSION}  (WoW client 1.12.x)")
_status("Source revision"
        "${rev_hash}  ${rev_date}  [${rev_branch}]")

_status("Requires database"
        "world ${MANGOS_DB_WORLD_VERSION}.${MANGOS_DB_WORLD_STRUCTURE}.${MANGOS_DB_WORLD_CONTENT} / characters ${MANGOS_DB_CHAR_VERSION}.${MANGOS_DB_CHAR_STRUCTURE}.${MANGOS_DB_CHAR_CONTENT} / realmd ${MANGOS_DB_REALMD_VERSION}.${MANGOS_DB_REALMD_STRUCTURE}.${MANGOS_DB_REALMD_CONTENT}")

message("")

_status("Host" "${CMAKE_HOST_SYSTEM}")
_status("Target" "${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")

if(CMAKE_CONFIGURATION_TYPES)
    string(REPLACE ";" ", " _cfgs "${CMAKE_CONFIGURATION_TYPES}")
    _status("Build type" "chosen at build time (${_cfgs})")
else()
    _status("Build type" "${CMAKE_BUILD_TYPE}")
endif()

if(CMAKE_GENERATOR_PLATFORM)
    _status("Generator" "${CMAKE_GENERATOR} (${CMAKE_GENERATOR_PLATFORM})")
else()
    _status("Generator" "${CMAKE_GENERATOR}")
endif()

math(EXPR _bits "${CMAKE_SIZEOF_VOID_P} * 8")
_status("C++ compiler"
        "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${_bits}-bit, C++${CMAKE_CXX_STANDARD})")
_status("CMake" "${CMAKE_VERSION}")

message("")

_status("OpenSSL" "${OPENSSL_VERSION}  (${OPENSSL_INCLUDE_DIR})")

if(MySQL_LIBRARIES)
    _status("MySQL/MariaDB client" "${MySQL_LIBRARIES}")
else()
    _status("MySQL/MariaDB client" "NOT FOUND")
endif()

if(ZLIB_FOUND)
    _status("zlib" "system ${ZLIB_VERSION_STRING}")
else()
    _status("zlib" "bundled (dep/zlib)")
endif()
if(BZIP2_FOUND)
    _status("bzip2" "system ${BZIP2_VERSION_STRING}")
else()
    _status("bzip2" "bundled (dep/bzip2)")
endif()

message("")

if(WIN32)
    _status("Network backend" "IOCP")
elseif(MANGOS_USE_IO_URING)
    _status("Network backend" "io_uring (${URING_LIBRARY})")
elseif(APPLE OR CMAKE_SYSTEM_NAME MATCHES "BSD")
    _status("Network backend" "kqueue reactor")
else()
    _status("Network backend" "epoll reactor")
    if(WITH_IO_URING)
        _status_sub("note" "io_uring was requested but liburing was not found")
    endif()
endif()

set(_targets)
if(BUILD_MANGOSD)
    list(APPEND _targets "mangosd")
endif()
if(BUILD_REALMD)
    list(APPEND _targets "realmd")
endif()
if(BUILD_TOOLS)
    list(APPEND _targets "extractor")
endif()
if(_targets)
    string(REPLACE ";" ", " _targets "${_targets}")
else()
    set(_targets "none -- nothing will be built")
endif()
_status("Building" "${_targets}")

if(SCRIPT_LIB_SD3)
    _status("Script engine" "ScriptDev3")
else()
    _status("Script engine" "none")
endif()

if(PCH)
    _status("Precompiled headers" "on")
else()
    _status("Precompiled headers" "off")
endif()

message("")
_status("Install binaries to" "${BIN_DIR}")
_status("Install configs to" "${CONF_INSTALL_DIR}")

if(WITHOUT_GIT)
    message("")
    message("  !  WITHOUT_GIT: the revision baked into the binaries is a placeholder,")
    message("  !  so crash reports and .server info cannot identify this build.")
endif()

message("======================================================================")
message("")
