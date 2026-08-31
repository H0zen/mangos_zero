if(BUILD_MANGOSD AND SCRIPT_LIB_SD3)
    add_subdirectory(SD3)

    set(SD3_COMPAT_DIR "${CMAKE_SOURCE_DIR}/src/shared/Compat/sd3")
    set(SD3_COMPAT_PRELUDE "${SD3_COMPAT_DIR}/Common.h")

    if(NOT TARGET mangosscript)
        message(FATAL_ERROR "SD3 did not define the 'mangosscript' target.")
    endif()

    if(NOT EXISTS "${SD3_COMPAT_PRELUDE}")
        message(FATAL_ERROR "Missing ${SD3_COMPAT_PRELUDE}.")
    endif()

    target_include_directories(mangosscript PRIVATE "${SD3_COMPAT_DIR}")

    if(MSVC)
        target_compile_options(mangosscript PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:/FI${SD3_COMPAT_PRELUDE}>")
    else()
        target_compile_options(mangosscript PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:-include${SD3_COMPAT_PRELUDE}>")
    endif()
endif()
