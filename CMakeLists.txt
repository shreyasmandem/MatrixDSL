#===----------------------------------------------------------------------===#
#
# Lexer, Parser and AST - Vinay A (24BCB0131)
#
# Individual contribution branch for MatrixDSL, Team 13.
#
# Builds mtxlex, the token dumper. The frontend has no LLVM dependency by
# design, so this branch builds and tests with nothing but a C++17 compiler.
#
#===----------------------------------------------------------------------===#

cmake_minimum_required(VERSION 3.16)

project(MatrixDSLFrontend
        VERSION 1.0.0
        DESCRIPTION "MatrixDSL lexer, parser and AST"
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
# mtxlex - token dumper
#
# Only the lexer is compiled at Review 1. Parser.h and AST.h are interface
# definitions whose implementations land in Review 2; AST.h additionally
# depends on MatrixType.h, which belongs to the semantic analysis module and
# is therefore not present on this branch.
#===----------------------------------------------------------------------===#

add_executable(mtxlex
               tools/mtxlex/main.cpp
               compiler/frontend/lexer/Lexer.cpp)

target_include_directories(mtxlex PRIVATE ${CMAKE_SOURCE_DIR}/compiler)

#===----------------------------------------------------------------------===#
# Lexer tests
#
# Valid inputs must tokenize cleanly (exit 0). Inputs containing an illegal
# character must be rejected (non-zero exit), and are marked as expected
# failures so a silent acceptance shows up as a test failure.
#===----------------------------------------------------------------------===#

enable_testing()

foreach(test
        keywords
        identifiers
        numbers
        operators
        punctuation
        comments
        keyword_prefix
        empty
        line_tracking
        full_program)
  add_test(NAME lexer_${test}
           COMMAND mtxlex ${CMAKE_SOURCE_DIR}/tests/lexer/${test}.mtx --quiet)
endforeach()

foreach(test invalid_character invalid_symbols)
  add_test(NAME lexer_reject_${test}
           COMMAND mtxlex ${CMAKE_SOURCE_DIR}/tests/lexer/${test}.mtx --quiet)
  set_tests_properties(lexer_reject_${test} PROPERTIES WILL_FAIL TRUE)
endforeach()

message(STATUS "")
message(STATUS "MatrixDSL frontend ${PROJECT_VERSION} configured")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output     : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
message(STATUS "  Build : cmake --build ${CMAKE_BINARY_DIR} -j")
message(STATUS "  Test  : ctest --test-dir ${CMAKE_BINARY_DIR} --output-on-failure")
message(STATUS "")
