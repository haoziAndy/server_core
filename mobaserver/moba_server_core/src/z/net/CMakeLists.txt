
# build libznet library 

set(build_target libznet)

include_directories(.)
aux_source_directory(. SRC_FILES)
aux_source_directory(./http SRC_FILES)

include_directories(../../contrib)
add_library(${build_target} STATIC ${SRC_FILES})
set_target_properties(${build_target} PROPERTIES PREFIX "")


add_precompiled_header(${build_target} stdafx.h FORCEINCLUDE)
add_dependencies(${build_target} libzcommon)
if (MSVC)
	target_link_libraries(${build_target} ${ZLIB_DIR}/libzcommon.lib)
else()
	target_link_libraries(${build_target} ${ZLIB_DIR}/libzcommon.a)
endif(MSVC)
