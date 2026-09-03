if(NOT TARGET "ZLIB::ZLIB")
    add_subdirectory(zlib)
endif()

if(NOT TARGET "BZip2::BZip2")
    add_subdirectory(bzip2)
endif()

add_subdirectory(utf8cpp)

add_subdirectory(recastnavigation)

add_subdirectory(StormLib)

if(WITH_TESTS)
    add_subdirectory(doctest)
endif()
