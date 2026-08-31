if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PLATFORM 64)
else()
    set(PLATFORM 32)
endif()

if(XCODE)
    if(PLATFORM EQUAL 32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
        set(CMAKE_OSX_ARCHITECTURES ARM32)
    elseif(PLATFORM EQUAL 32)
        set(CMAKE_OSX_ARCHITECTURES i386)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
        set(CMAKE_OSX_ARCHITECTURES ARM64)
    else()
        set(CMAKE_OSX_ARCHITECTURES x86_64)
    endif()
endif()

if(WIN32)
    add_compile_definitions(
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
endif()

if(MINGW)
    add_compile_definitions(
        WINVER=0x0600
        _WIN32_WINNT=0x0600
    )
    if(PLATFORM EQUAL 32)
        add_compile_definitions(HAVE_SSE2 __SSE2__)
    endif()
endif()

if(MSVC)
    add_compile_options(
        /MP
        /W4
        $<$<EQUAL:${PLATFORM},32>:/arch:SSE2>
        $<$<CONFIG:Release>:/Gw>
        $<$<CONFIG:Release>:/GF>
        $<$<CONFIG:Debug>:/bigobj>

        /wd4018 /wd4100 /wd4101 /wd4127 /wd4131 /wd4189 /wd4244 /wd4245
        /wd4267 /wd4302 /wd4305 /wd4311 /wd4389 /wd4456 /wd4458 /wd4581
        /wd4589 /wd4701 /wd4702 /wd4703 /wd4706 /wd4840 /wd4996
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        $<$<CONFIG:Debug>:-Wall>
        $<$<CONFIG:Debug>:-Wextra>
        $<$<CONFIG:Debug>:-Winit-self>
        $<$<CONFIG:Debug>:-Winvalid-pch>
        $<$<CONFIG:Debug>:-g3>

        $<$<CONFIG:Release>:-Wno-psabi>
    )
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "i386")
        add_compile_options(-msse2 -mfpmath=sse)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        $<$<CONFIG:Release>:-Wno-c++11-narrowing>
        $<$<CONFIG:Release>:-Wno-inconsistent-missing-override>
        $<$<CONFIG:Release>:-Wno-switch>
        $<$<CONFIG:Debug>:-Wall>
        $<$<CONFIG:Debug>:-Wextra>
        $<$<CONFIG:Debug>:-Winit-self>
        $<$<CONFIG:Debug>:-Woverloaded-virtual>
        $<$<CONFIG:Debug>:-g>
    )
endif()

if(MSVC)
    set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ON)
endif()
