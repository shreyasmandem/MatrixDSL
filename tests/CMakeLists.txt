#===----------------------------------------------------------------------===#
# MatrixDSL - Test Suite
#
# Pure C++ and CTest. No scripting-language dependency.
#===----------------------------------------------------------------------===#

add_executable(testrunner TestRunner.cpp)
set_target_properties(testrunner PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Category-level suites. Each shells out to the compiler and checks the
# outcome against the `// Expect:` line inside every test file.
foreach(category valid invalid boundary)
  add_test(NAME suite_${category}
           COMMAND testrunner
                   $<TARGET_FILE:matrixdslc>
                   ${CMAKE_SOURCE_DIR}/tests
                   ${category})
endforeach()

add_test(NAME suite_all
         COMMAND testrunner
                 $<TARGET_FILE:matrixdslc>
                 ${CMAKE_SOURCE_DIR}/tests)

# Every example in examples/ must compile. These are the programs shown in the
# documentation, so a broken example is a documentation bug.
file(GLOB EXAMPLE_PROGRAMS ${CMAKE_SOURCE_DIR}/examples/*.mtx)
foreach(example ${EXAMPLE_PROGRAMS})
  get_filename_component(name ${example} NAME_WE)
  add_test(NAME example_${name}
           COMMAND $<TARGET_FILE:matrixdslc> ${example} -o ${CMAKE_BINARY_DIR}/${name}.s)
endforeach()

message(STATUS "MatrixDSL: test suite configured")
