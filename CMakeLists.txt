#===----------------------------------------------------------------------===#
#
# MatrixDSL - A Domain-Specific Language and LLVM-Based Compiler
#             for Matrix and AI Workloads
#
# BCSE307P Compiler Design | Project A3 | Team 13
#
#===----------------------------------------------------------------------===#

cmake_minimum_required(VERSION 3.16)

project(MatrixDSL
        VERSION 1.0.0
        DESCRIPTION "DSL and LLVM-based compiler for matrix and AI workloads"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

#===----------------------------------------------------------------------===#
# Options
#
# The simulator and the frontend have no LLVM dependency. Building only those
# lets Vinay and Parth work before LLVM is installed, and lets Shreyas develop
# the ISA independently of the backend.
#===----------------------------------------------------------------------===#

option(MATRIXDSL_BUILD_SIM_ONLY "Build only the MDT simulator (no LLVM)" OFF)
option(MATRIXDSL_BUILD_FRONTEND_ONLY "Build only the frontend (no LLVM)" OFF)
option(MATRIXDSL_BUILD_TESTS "Build the test suite" ON)
option(MATRIXDSL_BUILD_BENCHMARKS "Build the benchmark suite" OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

#===----------------------------------------------------------------------===#
# MDT simulator - standalone, no dependencies
#===----------------------------------------------------------------------===#

file(GLOB MDTSIM_SOURCES ${CMAKE_SOURCE_DIR}/tools/mdtsim/*.cpp)

if(MDTSIM_SOURCES)
  add_executable(mdtsim ${MDTSIM_SOURCES})
  target_include_directories(mdtsim PRIVATE ${CMAKE_SOURCE_DIR}/tools/mdtsim)
  message(STATUS "MatrixDSL: MDT simulator enabled")
endif()

if(MATRIXDSL_BUILD_SIM_ONLY)
  message(STATUS "MatrixDSL: simulator-only build; skipping compiler")
  return()
endif()

#===----------------------------------------------------------------------===#
# Runtime library - no LLVM dependency
#===----------------------------------------------------------------------===#

file(GLOB RUNTIME_SOURCES ${CMAKE_SOURCE_DIR}/runtime/*.cpp)

if(RUNTIME_SOURCES)
  add_library(matrixdsl_runtime STATIC ${RUNTIME_SOURCES})
  target_include_directories(matrixdsl_runtime PUBLIC ${CMAKE_SOURCE_DIR}/runtime)
endif()

#===----------------------------------------------------------------------===#
# Frontend - lexer, parser, AST, semantic analysis. No LLVM dependency.
#===----------------------------------------------------------------------===#

file(GLOB_RECURSE FRONTEND_SOURCES
     ${CMAKE_SOURCE_DIR}/compiler/frontend/*.cpp)

if(FRONTEND_SOURCES)
  add_library(matrixdsl_frontend STATIC ${FRONTEND_SOURCES})
  target_include_directories(matrixdsl_frontend PUBLIC
                             ${CMAKE_SOURCE_DIR}/compiler)
endif()

if(MATRIXDSL_BUILD_FRONTEND_ONLY)
  message(STATUS "MatrixDSL: frontend-only build; skipping LLVM components")
  return()
endif()

#===----------------------------------------------------------------------===#
# LLVM
#
# Pin one version across the team. API drift between LLVM releases is a real
# source of "works on my machine" failures.
#===----------------------------------------------------------------------===#

find_package(LLVM 18 CONFIG QUIET)
if(NOT LLVM_FOUND)
  find_package(LLVM 17 CONFIG QUIET)
endif()

if(NOT LLVM_FOUND)
  message(WARNING
    "LLVM 17 or 18 not found. Building frontend and simulator only.\n"
    "  Install with:  vcpkg install llvm[core]\n"
    "  Or configure:  cmake -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm ..")
  return()
endif()

message(STATUS "MatrixDSL: found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "MatrixDSL: LLVM cmake dir ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR})
include(AddLLVM)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(LLVM_LIBS
  core support irreader passes analysis
  target codegen mc mcparser asmprinter selectiondag)

#===----------------------------------------------------------------------===#
# IR generation and optimization
#===----------------------------------------------------------------------===#

file(GLOB CODEGEN_SOURCES ${CMAKE_SOURCE_DIR}/compiler/llvm/*.cpp)

if(CODEGEN_SOURCES)
  add_library(matrixdsl_codegen STATIC ${CODEGEN_SOURCES})
  target_include_directories(matrixdsl_codegen
                             PUBLIC ${CMAKE_SOURCE_DIR}/compiler
                             PRIVATE ${LLVM_INCLUDE_DIRS})
  target_compile_definitions(matrixdsl_codegen PRIVATE ${LLVM_DEFINITIONS_LIST})
  target_link_libraries(matrixdsl_codegen PRIVATE ${LLVM_LIBS})
endif()

#===----------------------------------------------------------------------===#
# MDT backend
#===----------------------------------------------------------------------===#

if(EXISTS ${CMAKE_SOURCE_DIR}/llvm-backend/MDT/CMakeLists.txt)
  add_subdirectory(llvm-backend/MDT)
endif()

#===----------------------------------------------------------------------===#
# Driver executable
#===----------------------------------------------------------------------===#

file(GLOB DRIVER_SOURCES ${CMAKE_SOURCE_DIR}/compiler/driver/*.cpp)

add_executable(matrixdslc
               ${CMAKE_SOURCE_DIR}/compiler/main.cpp
               ${DRIVER_SOURCES})

target_include_directories(matrixdslc
                           PRIVATE ${CMAKE_SOURCE_DIR}/compiler
                                   ${LLVM_INCLUDE_DIRS})
target_compile_definitions(matrixdslc PRIVATE ${LLVM_DEFINITIONS_LIST})

foreach(lib matrixdsl_frontend matrixdsl_codegen matrixdsl_runtime)
  if(TARGET ${lib})
    target_link_libraries(matrixdslc PRIVATE ${lib})
  endif()
endforeach()

target_link_libraries(matrixdslc PRIVATE ${LLVM_LIBS})

#===----------------------------------------------------------------------===#
# Tests and benchmarks
#===----------------------------------------------------------------------===#

if(MATRIXDSL_BUILD_TESTS AND EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
  enable_testing()
  add_subdirectory(tests)
endif()

if(MATRIXDSL_BUILD_BENCHMARKS AND EXISTS ${CMAKE_SOURCE_DIR}/benchmarks/CMakeLists.txt)
  add_subdirectory(benchmarks)
endif()

#===----------------------------------------------------------------------===#

message(STATUS "")
message(STATUS "MatrixDSL ${PROJECT_VERSION} configured")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++        : ${CMAKE_CXX_STANDARD}")
message(STATUS "  LLVM       : ${LLVM_PACKAGE_VERSION}")
message(STATUS "  Output     : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
