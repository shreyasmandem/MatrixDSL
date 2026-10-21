#===----------------------------------------------------------------------===#
# MatrixDSL - top-level build (final, integrated)
#
# Builds the complete pipeline with nothing but a C++17 compiler:
#
#   .mtx -> lexer -> parser -> semantic analysis -> Tensor IR
#        -> fusion / DCE / scratchpad tiling -> MDT assembly -> mdtsim
#
# The LLVM TableGen target description (llvm-backend/MDT) and the LLVM C++ API
# code generator need an LLVM source tree; they are opt-in. The default build
# emits LLVM IR as text (compiler/llvm/LLVMTextEmitter) so --emit-llvm works
# everywhere.
#===----------------------------------------------------------------------===#

cmake_minimum_required(VERSION 3.16)
project(MatrixDSL VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# MinGW: link the C++ runtime statically so every tool runs from any shell
# without the compiler's DLLs on PATH.
if(MINGW)
  add_link_options(-static)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(MATRIXDSL_BUILD_TESTS "Build and register the test suites" ON)
option(MATRIXDSL_WITH_LLVM "Also build the LLVM TableGen target (needs LLVM)" OFF)

# --- Libraries -------------------------------------------------------------

# Front end - Vinay A (lexer, parser, AST) and Mudpe Parth Tulsidas (semantic)
add_library(matrixdsl_frontend STATIC
  compiler/frontend/lexer/Lexer.cpp
  compiler/frontend/parser/Parser.cpp
  compiler/frontend/ast/AST.cpp
  compiler/frontend/semantic/MatrixType.cpp
  compiler/frontend/semantic/SymbolTable.cpp
  compiler/frontend/semantic/SemanticAnalyzer.cpp
  compiler/frontend/Diagnostics.cpp)
target_include_directories(matrixdsl_frontend PUBLIC compiler)

# Middle end - Kandi Jeevitesh Reddy (Tensor IR, lowering, passes, oracle)
add_library(matrixdsl_tensorir STATIC
  compiler/tensor-ir/TensorIR.cpp
  compiler/tensor-ir/Lowering.cpp
  compiler/tensor-ir/FusionPass.cpp
  compiler/tensor-ir/Passes.cpp
  compiler/tensor-ir/Interpreter.cpp
  compiler/llvm/LLVMTextEmitter.cpp)
target_link_libraries(matrixdsl_tensorir PUBLIC matrixdsl_frontend)

# Reference runtime - Kandi Jeevitesh Reddy
add_library(matrixdsl_runtime STATIC runtime/MatrixRuntime.cpp)

# MDT simulator core - Shreyas Mandem
add_library(mdtsim_core STATIC
  tools/mdtsim/Assembler.cpp
  tools/mdtsim/MDTSim.cpp)
target_include_directories(mdtsim_core PUBLIC tools/mdtsim)

# Back end - Shreyas Mandem (Tensor IR -> MDT assembly)
add_library(matrixdsl_backend STATIC compiler/backend/MDTCodeGen.cpp)
target_link_libraries(matrixdsl_backend PUBLIC matrixdsl_tensorir)

# Driver - Vinay A (one pipeline object shared by every tool below)
add_library(matrixdsl_driver STATIC compiler/driver/Pipeline.cpp)
target_link_libraries(matrixdsl_driver PUBLIC matrixdsl_backend mdtsim_core)

# --- Tools -----------------------------------------------------------------

add_executable(mtxc tools/mtxc/main.cpp)             # end-to-end compiler
target_link_libraries(mtxc PRIVATE matrixdsl_driver)
set_target_properties(mtxc PROPERTIES OUTPUT_NAME matrixdslc)

add_executable(mdtsim tools/mdtsim/main.cpp)         # simulator CLI
target_link_libraries(mdtsim PRIVATE mdtsim_core)

add_executable(mtxverify tools/mtxverify/main.cpp)   # differential tester
target_link_libraries(mtxverify PRIVATE matrixdsl_driver)

add_executable(mtxbench tools/mtxbench/main.cpp)     # ablation study
target_link_libraries(mtxbench PRIVATE matrixdsl_driver)

add_executable(mtxserve tools/mtxserve/main.cpp)     # dashboard backend
target_link_libraries(mtxserve PRIVATE matrixdsl_driver)
target_compile_definitions(mtxserve PRIVATE
  MATRIXDSL_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(WIN32)
  target_link_libraries(mtxserve PRIVATE ws2_32)
endif()

add_executable(mtxlex tools/mtxlex/main.cpp)
target_link_libraries(mtxlex PRIVATE matrixdsl_frontend)
add_executable(mtxparse tools/mtxparse/main.cpp)
target_link_libraries(mtxparse PRIVATE matrixdsl_frontend)
add_executable(mtxcheck tools/mtxcheck/main.cpp)
target_link_libraries(mtxcheck PRIVATE matrixdsl_frontend)
add_executable(mtxtensorir tools/mtxtensorir/main.cpp)
target_link_libraries(mtxtensorir PRIVATE matrixdsl_tensorir)
add_executable(mtxref tools/mtxref/main.cpp)
target_link_libraries(mtxref PRIVATE matrixdsl_runtime)

if(MATRIXDSL_WITH_LLVM)
  add_subdirectory(llvm-backend/MDT)
endif()

# --- Tests -----------------------------------------------------------------

if(MATRIXDSL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
