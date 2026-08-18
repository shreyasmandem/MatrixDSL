#===----------------------------------------------------------------------===#
#
# MDT ISA and LLVM Backend - Shreyas Mandem (24BCE0381)
#
# Individual contribution branch for MatrixDSL, Team 13.
#
# Builds the MDT instruction set simulator, which is standalone and has no
# LLVM dependency. The TableGen backend description in llvm-backend/MDT/ is
# built as part of an LLVM tree, not from here - see docs/llvm-backend.md.
#
#===----------------------------------------------------------------------===#

cmake_minimum_required(VERSION 3.16)

project(MDTBackend
        VERSION 1.0.0
        DESCRIPTION "MDT instruction set simulator and LLVM backend description"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

#===----------------------------------------------------------------------===#
# mdtsim - MDT instruction set simulator
#===----------------------------------------------------------------------===#

add_executable(mdtsim
               tools/mdtsim/main.cpp
               tools/mdtsim/MDTSim.cpp
               tools/mdtsim/Assembler.cpp)

target_include_directories(mdtsim PRIVATE ${CMAKE_SOURCE_DIR}/tools/mdtsim)

#===----------------------------------------------------------------------===#
# Backend tests
#
# Each test runs its assembly through the simulator. A non-zero exit status
# means either an assembly error or a runtime fault, both of which are
# failures.
#===----------------------------------------------------------------------===#

enable_testing()

foreach(test
        scalar_arith
        memory
        control_flow
        call_return
        vector_ops
        matmul_4x4
        transpose
        relu
        ai_layer
        tile_strided)
  add_test(NAME backend_${test}
           COMMAND mdtsim ${CMAKE_SOURCE_DIR}/tests/backend/${test}.s --quiet)
endforeach()

message(STATUS "")
message(STATUS "MDT backend ${PROJECT_VERSION} configured")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output     : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
message(STATUS "  Build : cmake --build ${CMAKE_BINARY_DIR} -j")
message(STATUS "  Test  : ctest --test-dir ${CMAKE_BINARY_DIR} --output-on-failure")
message(STATUS "")
