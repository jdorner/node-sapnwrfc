cmake_minimum_required(VERSION 2.8.6)
project(sapnwrfc.node)

add_definitions(-DBUILDING_NODE_EXTENSION)
add_definitions(-D_LARGEFILE_SOURCE)
add_definitions(-D_FILE_OFFSET_BITS=64)
add_definitions(-DSAPwithUNICODE)
add_definitions(-DSAPwithTHREADS)
add_definitions(-DSAPonUNIX)
add_definitions(-DSAPonLIN)
add_definitions(-DSAPwithTHREADS)
add_definitions(-D__NO_MATH_INLINES)

link_directories(${NODE_ROOT}/lib)
include_directories(${NODE_ROOT}/include/node)
include_directories(${NODE_ROOT}/src)
include_directories(${NODE_ROOT}/deps/v8/include)

include_directories(
   "${CMAKE_CURRENT_SOURCE_DIR}"
   ${PROJECT_SOURCE_DIR}/nwrfcsdk/include
   ${PROJECT_SOURCE_DIR}/node_modules/nan
)

link_directories(
   ${PROJECT_SOURCE_DIR}/nwrfcsdk/lib
)

set(Sources
src/binding.cc
src/current_function.hpp
src/Common.h
src/Loggable.h
src/Loggable.cc
src/Connection.h
src/Connection.cc
src/Function.h
src/Function.cc
examples/example1.js
)

add_library(sapnwrfc.node SHARED ${Sources})
target_link_libraries(sapnwrfc.node sapnwrfc sapucum)
set_target_properties(sapnwrfc.node PROPERTIES OUTPUT_NAME sapnwrfc
                                             PREFIX ""
                                             SUFFIX .node
                                             LINK_FLAGS -rdynamic)
