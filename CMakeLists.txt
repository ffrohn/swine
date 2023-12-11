cmake_minimum_required(VERSION 3.13)

project(swine)

set(CMAKE_CXX_STANDARD 20)

set(EXECUTABLE swine)

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBS OFF)
add_link_options(-static)

set(INCLUDE_DIRS
  "${PROJECT_SOURCE_DIR}/include")

add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELEASE} -g")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC SHA)
git_local_changes(DIRTY)

# generate version.cc
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp" @ONLY)

message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
message(STATUS "Compiler flags:" ${CMAKE_CXX_COMPILE_FLAGS})
message(STATUS "Compiler cxx debug flags:" ${CMAKE_CXX_FLAGS_DEBUG})
message(STATUS "Compiler cxx release flags:" ${CMAKE_CXX_FLAGS_RELEASE})
message(STATUS "Compiler cxx relwithdebinfo flags:" ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
message(STATUS "Compiler cxx min size flags:" ${CMAKE_CXX_FLAGS_MINSIZEREL})
message(STATUS "Compiler cxx flags:" ${CMAKE_CXX_FLAGS})

find_library(SWITCH smt-switch)
message(STATUS "smt-switch: ${SWITCH}")
find_library(SWITCH_CVC5 smt-switch-cvc5)
message(STATUS "smt-switch-cvc5: ${SWITCH_CVC5}")
find_library(SWITCH_Z3 smt-switch-z3)
message(STATUS "smt-switch-z3: ${SWITCH_Z3}")
find_library(Z3 z3)
message(STATUS "z3: ${Z3}")
find_library(CLN cln)
message(STATUS "cln: ${CLN}")
find_library(GMP gmp)
message(STATUS "gmp: ${GMP}")

add_executable(${EXECUTABLE} "")

set (SOURCES
    "${PROJECT_SOURCE_DIR}/include/swine.h"
    "${PROJECT_SOURCE_DIR}/src/swine.cpp"
    "${PROJECT_SOURCE_DIR}/src/term_evaluator.cpp"
    "${PROJECT_SOURCE_DIR}/include/term_evaluator.h"
    "${PROJECT_SOURCE_DIR}/include/constant_propagator.h"
    "${PROJECT_SOURCE_DIR}/src/constant_propagator.cpp"
    "${PROJECT_SOURCE_DIR}/include/rewriter.h"
    "${PROJECT_SOURCE_DIR}/src/rewriter.cpp"
    "${PROJECT_SOURCE_DIR}/include/preprocessor.h"
    "${PROJECT_SOURCE_DIR}/src/preprocessor.cpp"
    "${PROJECT_SOURCE_DIR}/include/exp_finder.h"
    "${PROJECT_SOURCE_DIR}/src/exp_finder.cpp"
    "${PROJECT_SOURCE_DIR}/include/brute_force.h"
    "${PROJECT_SOURCE_DIR}/src/brute_force.cpp"
    "${PROJECT_SOURCE_DIR}/include/util.h"
    "${PROJECT_SOURCE_DIR}/src/util.cpp"
    "${PROJECT_SOURCE_DIR}/include/lemma_kind.h"
    "${PROJECT_SOURCE_DIR}/src/lemma_kind.cpp"
    "${PROJECT_SOURCE_DIR}/include/preproc_kind.h"
    "${PROJECT_SOURCE_DIR}/src/preproc_kind.cpp"
    "${PROJECT_SOURCE_DIR}/include/config.h"
    "${PROJECT_SOURCE_DIR}/src/main.cpp"
    "${PROJECT_SOURCE_DIR}/include/version.h"
    "${PROJECT_SOURCE_DIR}/src/version.cpp"
)

target_sources(${EXECUTABLE} PRIVATE ${SOURCES})

add_library(libswine STATIC ${SOURCES})

set_property(TARGET libswine PROPERTY OUTPUT_NAME swine)

include_directories(${INCLUDE_DIRS})

target_link_libraries(${EXECUTABLE}
    ${SWITCH_CVC5}
    ${SWITCH_Z3}
    ${Z3}
    ${SWITCH}
    ${CLN}
    ${GMP}
)
