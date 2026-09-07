add_library(clientarchive STATIC
    Inflate.cpp
    Inflate.hpp
    MpqArchive.cpp
    MpqArchive.hpp
    MpqChain.cpp
    MpqChain.hpp
    PkwareExplode.cpp
    PkwareExplode.hpp
)

target_include_directories(clientarchive
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(clientarchive PROPERTIES FOLDER "shared")
