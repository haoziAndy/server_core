
# chat_server  

set(build_target testlib)

aux_source_directory(. SRC_FILES)

add_executable(${build_target} ${SRC_FILES})

target_link_libraries(${build_target} libzcommon libzcontrib libznet)

add_dependencies(${build_target} libzcommon libzcontrib libznet)

add_precompiled_header(${build_target} stdafx.h FORCEINCLUDE)

#add boost lib
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED OFF) 
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost REQUIRED COMPONENTS chrono system filesystem date_time program_options random)
link_directories(${BOOST_LIBRARYDIR})
target_link_libraries(${build_target} ${Boost_LIBRARIES})

if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-std=c++11 -g -Wall)
endif()
