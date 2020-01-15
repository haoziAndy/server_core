cmake_minimum_required(VERSION 3.12)
# build libcommon library 

set(build_target libzcontrib)

include_directories(.)
aux_source_directory(. SRC_FILES)

if(CMAKE_COMPILER_IS_GNUCXX)
	if(CMAKE_BUILD_TYPE MATCHES Debug)
		  add_definitions( -DDEBUG -g  )
	else(CMAKE_BUILD_TYPE MATCHES Release)
		add_definitions( -O3 )
	endif()
	add_definitions(-std=c++11 -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-local-typedefs)
	#add_definitions(-std=c++11  -g -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-local-typedefs)
endif()


add_library(${build_target} STATIC ${SRC_FILES})
set_target_properties(${build_target} PROPERTIES PREFIX "")




add_custom_command(TARGET ${build_target}
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy libzcontrib.a ${PROJECT_SOURCE_DIR}/../../lib
)
