cmake_minimum_required(VERSION 3.13)

project(swine)

set(CMAKE_CXX_STANDARD 20)

set(EXECUTABLE swine)

set(INCLUDE_DIRS
  "${PROJECT_SOURCE_DIR}/include")

add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -g")

message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
message(STATUS "Compiler flags:" ${CMAKE_CXX_COMPILE_FLAGS})
message(STATUS "Compiler cxx debug flags:" ${CMAKE_CXX_FLAGS_DEBUG})
message(STATUS "Compiler cxx release flags:" ${CMAKE_CXX_FLAGS_RELEASE})
message(STATUS "Compiler cxx relwithdebinfo flags:" ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
message(STATUS "Compiler cxx min size flags:" ${CMAKE_CXX_FLAGS_MINSIZEREL})
message(STATUS "Compiler cxx flags:" ${CMAKE_CXX_FLAGS})

find_library(SWITCH smt-switch)
message(STATUS "smt-switch: ${SWITCH}")

add_executable(${EXECUTABLE} "")

target_sources(${EXECUTABLE} PRIVATE
    "${PROJECT_SOURCE_DIR}/include/swine.h"
    "${PROJECT_SOURCE_DIR}/src/swine.cpp"
    "${PROJECT_SOURCE_DIR}/include/term_flattener.h"
    "${PROJECT_SOURCE_DIR}/src/term_flattener.cpp"
)

include_directories(${INCLUDE_DIRS})

target_link_libraries(${EXECUTABLE}
  ${SWITCH})
