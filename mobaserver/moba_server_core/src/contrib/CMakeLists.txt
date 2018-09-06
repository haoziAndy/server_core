cmake_minimum_required(VERSION 3.12)
# build libcommon library 

set(build_target libzcontrib)

include_directories(.)
aux_source_directory(. SRC_FILES)

add_library(${build_target} STATIC ${SRC_FILES})
set_target_properties(${build_target} PROPERTIES PREFIX "")

if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-std=c++11 -g -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-local-typedefs)
endif()


add_custom_command(TARGET ${build_target}
          POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy libznet.a ${PROJECT_SOURCE_DIR}/../../lib
)
