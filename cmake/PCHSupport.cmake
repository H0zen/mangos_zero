include(CheckCXXCompilerFlag)

check_cxx_compiler_flag("-fpch-instantiate-templates" HAVE_FPCH_INSTANTIATE_TEMPLATES)

function(ADD_CXX_PCH TARGET_NAME PRECOMPILED_HEADER)
	if(NOT TARGET ${TARGET_NAME})
		message(FATAL_ERROR "ADD_CXX_PCH: '${TARGET_NAME}' is not a target.")
	endif()

	if(NOT PRECOMPILED_HEADER)
		message(FATAL_ERROR "ADD_CXX_PCH(${TARGET_NAME}): no precompiled header given.")
	endif()

	get_filename_component(_pch "${PRECOMPILED_HEADER}" ABSOLUTE
		BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

	if(NOT EXISTS "${_pch}")
		message(FATAL_ERROR "ADD_CXX_PCH(${TARGET_NAME}): header '${_pch}' does not exist.")
	endif()

	target_precompile_headers(${TARGET_NAME} PRIVATE "${_pch}")

	if(HAVE_FPCH_INSTANTIATE_TEMPLATES)
		target_compile_options(${TARGET_NAME} PRIVATE -fpch-instantiate-templates)
	endif()
endfunction()
