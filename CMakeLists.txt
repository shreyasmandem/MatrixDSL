#===----------------------------------------------------------------------===#
#
# Semantic Analysis and Symbol Table - Mudpe Parth Tulsidas (24BDS0353)
#
# Individual contribution branch for MatrixDSL, Team 13.
#
# Builds mtxcheck, which exercises every shape rule and symbol table behaviour.
#
# Note what is NOT built here: SemanticAnalyzer.cpp. That class walks the AST,
# so it depends on Vinay's AST.h, which lives on his branch. The shape rules
# and the symbol table are deliberately independent of the AST, which is why
# the core logic of this module is testable before the parser exists.
#
#===----------------------------------------------------------------------===#

cmake_minimum_required(VERSION 3.16)

project(MatrixDSLSemantic
        VERSION 1.0.0
        DESCRIPTION "MatrixDSL semantic analysis, symbol table and matrix type system"
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
# Semantic library - shape rules and symbol table, no AST dependency
#===----------------------------------------------------------------------===#

add_library(matrixdsl_semantic STATIC
            compiler/frontend/semantic/MatrixType.cpp
            compiler/frontend/semantic/SymbolTable.cpp)

target_include_directories(matrixdsl_semantic
                           PUBLIC ${CMAKE_SOURCE_DIR}/compiler)

#===----------------------------------------------------------------------===#
# mtxcheck - shape rule and symbol table checker
#===----------------------------------------------------------------------===#

add_executable(mtxcheck tools/mtxcheck/main.cpp)
target_link_libraries(mtxcheck PRIVATE matrixdsl_semantic)

#===----------------------------------------------------------------------===#
# Tests
#===----------------------------------------------------------------------===#

enable_testing()

foreach(section add matmul scalar transpose relu bounds symbols diag)
  add_test(NAME semantic_${section} COMMAND mtxcheck ${section})
endforeach()

add_test(NAME semantic_all COMMAND mtxcheck all)

message(STATUS "")
message(STATUS "MatrixDSL semantic ${PROJECT_VERSION} configured")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output     : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
message(STATUS "  Build : cmake --build ${CMAKE_BINARY_DIR} -j")
message(STATUS "  Test  : ctest --test-dir ${CMAKE_BINARY_DIR} --output-on-failure")
message(STATUS "")
