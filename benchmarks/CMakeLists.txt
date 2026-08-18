#===----------------------------------------------------------------------===#
# MatrixDSL - Benchmarks
#===----------------------------------------------------------------------===#

add_executable(benchmark_matmul benchmark_matmul.cpp)
set_target_properties(benchmark_matmul PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT MSVC)
  target_compile_options(benchmark_matmul PRIVATE -O2)
endif()

if(TARGET matrixdsl_runtime)
  target_link_libraries(benchmark_matmul PRIVATE matrixdsl_runtime)
endif()

message(STATUS "MatrixDSL: benchmarks configured")
