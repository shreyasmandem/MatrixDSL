#===----------------------------------------------------------------------===#
# MatrixDSL - test registration (final, integrated)
#
#   ctest --test-dir build              run everything
#   ctest --test-dir build -L e2e       one group: lexer parser semantic
#                                       reference backend driver e2e bench
#===----------------------------------------------------------------------===#

set(T ${CMAKE_SOURCE_DIR}/tests)

# --- Lexer (Vinay A) --------------------------------------------------------
foreach(test keywords identifiers numbers operators punctuation comments
             keyword_prefix empty line_tracking full_program)
  add_test(NAME lexer_${test} COMMAND mtxlex ${T}/lexer/${test}.mtx --quiet)
  set_tests_properties(lexer_${test} PROPERTIES LABELS lexer)
endforeach()
foreach(test invalid_character invalid_symbols)
  add_test(NAME lexer_reject_${test} COMMAND mtxlex ${T}/lexer/${test}.mtx --quiet)
  set_tests_properties(lexer_reject_${test} PROPERTIES WILL_FAIL TRUE LABELS lexer)
endforeach()

# --- Parser (Vinay A) -------------------------------------------------------
file(GLOB PARSER_TESTS ${T}/parser/*.mtx)
foreach(file ${PARSER_TESTS})
  get_filename_component(name ${file} NAME_WE)
  add_test(NAME parser_${name} COMMAND mtxparse ${file} --quiet)
  set_tests_properties(parser_${name} PROPERTIES LABELS parser)
  if(name STREQUAL "missing_semicolon" OR name STREQUAL "unbalanced_bracket")
    set_tests_properties(parser_${name} PROPERTIES WILL_FAIL TRUE)
  endif()
endforeach()

# --- Semantic analysis (Mudpe Parth Tulsidas) ----------------------------------
foreach(section add matmul scalar transpose relu bounds symbols diag all)
  add_test(NAME semantic_${section} COMMAND mtxcheck ${section})
  set_tests_properties(semantic_${section} PROPERTIES LABELS semantic)
endforeach()

# --- Reference runtime (Kandi Jeevitesh Reddy) -------------------------------
foreach(op add sub scalar matmul transpose relu layer tiling all)
  add_test(NAME reference_${op} COMMAND mtxref ${op})
  set_tests_properties(reference_${op} PROPERTIES LABELS reference)
endforeach()

# --- MDT simulator, hand-written assembly (Shreyas Mandem) -------------------
file(GLOB BACKEND_TESTS ${T}/backend/*.s)
foreach(file ${BACKEND_TESTS})
  get_filename_component(name ${file} NAME_WE)
  add_test(NAME backend_${name} COMMAND mdtsim ${file} --quiet)
  set_tests_properties(backend_${name} PROPERTIES LABELS backend)
endforeach()

# --- Compiler driver: accept/reject suites (team) ---------------------------
add_executable(testrunner TestRunner.cpp)
foreach(category valid invalid boundary)
  add_test(NAME driver_${category}
           COMMAND testrunner $<TARGET_FILE:mtxc> ${T} ${category})
  # TestRunner writes one scratch file in its working directory.
  set_tests_properties(driver_${category} PROPERTIES LABELS driver
                       RESOURCE_LOCK testrunner_tmp)
endforeach()

# --- End to end: compiled MDT output == reference interpreter (Parth) ------
add_test(NAME e2e_corpus COMMAND mtxverify --quiet ${T}/e2e)
add_test(NAME e2e_examples COMMAND mtxverify --quiet ${CMAKE_SOURCE_DIR}/examples)
add_test(NAME e2e_workloads
         COMMAND mtxverify --quiet ${CMAKE_SOURCE_DIR}/benchmarks/workloads)
set_tests_properties(e2e_corpus e2e_examples e2e_workloads PROPERTIES LABELS e2e)

# --- Ablation study runs and verifies (Shreyas Mandem) -----------------------
add_test(NAME bench_ablation
         COMMAND mtxbench ${CMAKE_SOURCE_DIR}/benchmarks/workloads)
set_tests_properties(bench_ablation PROPERTIES LABELS bench)
