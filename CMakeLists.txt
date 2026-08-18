#===----------------------------------------------------------------------===#
#
# LLVM IR Generation and Optimization - Kandi Jeevitesh Reddy (24BCE0350)
#
# Individual contribution branch for MatrixDSL, Team 13.
#
# Builds the runtime library and mtxref, the reference operation runner. Both
# are deliberately free of LLVM dependencies so they build with nothing but a
# C++17 compiler, and so the runtime can link into both natively compiled
# programs and the MDT verification path.
#
# The IR generator itself (LLVMCodeGen.cpp, LLVMOptimizer.cpp) requires LLVM
# and lands in Review 2; only the interfaces are present at Review 1.
#
#===----------------------------------------------------------------------===#

cmake_minimum_required(VERSION 3.16)

project(MatrixDSLCodeGen
        VERSION 1.0.0
        DESCRIPTION "MatrixDSL LLVM IR generation, optimization and runtime"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

#===----------------------------------------------------------------------===#
# Runtime library - no LLVM dependency
#===----------------------------------------------------------------------===#

add_library(matrixdsl_runtime STATIC runtime/MatrixRuntime.cpp)
target_include_directories(matrixdsl_runtime PUBLIC ${CMAKE_SOURCE_DIR}/runtime)

#===----------------------------------------------------------------------===#
# mtxref - reference operation runner
#
# Exercises every reference implementation and checks the algebraic identities
# that must hold. This is the oracle the compiled MDT path is verified
# against, so it has to be correct before anything else can be trusted.
#===----------------------------------------------------------------------===#

add_executable(mtxref tools/mtxref/main.cpp)
target_link_libraries(mtxref PRIVATE matrixdsl_runtime)

#===----------------------------------------------------------------------===#
# Tests
#===----------------------------------------------------------------------===#

enable_testing()

foreach(op add sub scalar matmul transpose relu layer tiling)
  add_test(NAME reference_${op} COMMAND mtxref ${op})
endforeach()

add_test(NAME reference_all COMMAND mtxref all)

message(STATUS "")
message(STATUS "MatrixDSL codegen ${PROJECT_VERSION} configured")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output     : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
message(STATUS "  Build : cmake --build ${CMAKE_BINARY_DIR} -j")
message(STATUS "  Test  : ctest --test-dir ${CMAKE_BINARY_DIR} --output-on-failure")
message(STATUS "")
