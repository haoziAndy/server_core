cmake_minimum_required(VERSION 3.12)
# build libcommon library 

set(build_target libzcommon)

include_directories(.)
aux_source_directory(. SRC_FILES)

include_directories(../../contrib)
include_directories(../../../include)
include_directories(../../)
include_directories(../../../include/google_breakpad)
include_directories(../../../include/snappy)

set(HAVE_SYS_UIO_H_01 ${HAVE_SYS_UIO_H})
if(NOT HAVE_SYS_UIO_H_01)
  set(HAVE_SYS_UIO_H_01 0)
endif(NOT HAVE_SYS_UIO_H_01)

MACRO(ADD_PRECOMPILED_HEADER _targetName _input)
	GET_FILENAME_COMPONENT(_inputWe ${_input} NAME_WE)
	SET(pch_source ${_inputWe}.cpp)
	FOREACH(arg ${ARGN})
		IF(arg STREQUAL FORCEINCLUDE)
			SET(FORCEINCLUDE ON)
		ELSE(arg STREQUAL FORCEINCLUDE)
			SET(FORCEINCLUDE OFF)
		ENDIF(arg STREQUAL FORCEINCLUDE)
	ENDFOREACH(arg)


	IF(CMAKE_COMPILER_IS_GNUCXX)
		GET_FILENAME_COMPONENT(_name ${_input} NAME)
		SET(_source "${CMAKE_CURRENT_SOURCE_DIR}/${_input}")
		SET(_outdir "${CMAKE_CURRENT_BINARY_DIR}/${_name}.gch")
		MAKE_DIRECTORY(${_outdir})
		SET(_output "${_outdir}/.c++")

		STRING(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" _flags_var_name)
		SET(_compiler_FLAGS ${${_flags_var_name}})

		GET_DIRECTORY_PROPERTY(_directory_flags INCLUDE_DIRECTORIES)
		FOREACH(item ${_directory_flags})
			LIST(APPEND _compiler_FLAGS "-I${item}")
		ENDFOREACH(item)

		LIST(APPEND _compiler_FLAGS "-std=c++11")

		GET_DIRECTORY_PROPERTY(_directory_flags DEFINITIONS)
		LIST(APPEND _compiler_FLAGS ${_directory_flags})

		SEPARATE_ARGUMENTS(_compiler_FLAGS)
		MESSAGE("${CMAKE_CXX_COMPILER} -DPCHCOMPILE ${_compiler_FLAGS} -x c++-header -o {_output} ${_source}")
		ADD_CUSTOM_COMMAND(
			OUTPUT ${_output}
			COMMAND ${CMAKE_CXX_COMPILER} ${_compiler_FLAGS} -x c++-header -o ${_output} ${_source}
			DEPENDS ${_source} )
		ADD_CUSTOM_TARGET(${_targetName}_gch DEPENDS ${_output})
		ADD_DEPENDENCIES(${_targetName} ${_targetName}_gch)
		SET_TARGET_PROPERTIES(${_targetName} PROPERTIES COMPILE_FLAGS "-include ${_name} -Winvalid-pch")
	ENDIF(CMAKE_COMPILER_IS_GNUCXX)
ENDMACRO()


add_library(${build_target} STATIC ${SRC_FILES})
set_target_properties(${build_target} PROPERTIES PREFIX "")

if(CMAKE_COMPILER_IS_GNUCXX)
	add_definitions(-std=c++11 -g -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-local-typedefs)
endif()


add_precompiled_header(${build_target} stdafx.h FORCEINCLUDE)
#add_dependencies(${build_target} libzcommon)

add_custom_command(TARGET ${build_target}
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy libzcommon.a ${PROJECT_SOURCE_DIR}/../../../lib
)
