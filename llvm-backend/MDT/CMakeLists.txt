#===----------------------------------------------------------------------===#
# MDT - MatrixDSL Target backend
# Owner: Shreyas Mandem (24BCE0381)
#
# Builds against an existing LLVM source/install tree. The LLVM source tree is
# deliberately NOT vendored into this repository.
#===----------------------------------------------------------------------===#

set(LLVM_TARGET_DEFINITIONS MDT.td)

# TableGen generates the register info, instruction info, instruction selector
# and calling-convention tables from the .td description.
tablegen(LLVM MDTGenRegisterInfo.inc      -gen-register-info)
tablegen(LLVM MDTGenInstrInfo.inc         -gen-instr-info)
tablegen(LLVM MDTGenAsmWriter.inc         -gen-asm-writer)
tablegen(LLVM MDTGenDAGISel.inc           -gen-dag-isel)
tablegen(LLVM MDTGenCallingConv.inc       -gen-callingconv)
tablegen(LLVM MDTGenSubtargetInfo.inc     -gen-subtarget)

add_public_tablegen_target(MDTCommonTableGen)

# Source files are added as each backend phase lands - see docs/llvm-backend.md
# section 8. Phases: registration, scalar, memory, control flow, vector,
# matrix, tiling.
set(MDT_SOURCES)

foreach(src
        MDTTargetMachine.cpp
        MDTSubtarget.cpp
        MDTInstrInfo.cpp
        MDTRegisterInfo.cpp
        MDTISelLowering.cpp
        MDTISelDAGToDAG.cpp
        MDTFrameLowering.cpp
        MDTAsmPrinter.cpp
        MDTTilingPass.cpp)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${src})
    list(APPEND MDT_SOURCES ${src})
  endif()
endforeach()

if(NOT MDT_SOURCES)
  message(STATUS "MDT: TableGen description only; no C++ sources yet")
  return()
endif()

add_llvm_target(MDTCodeGen
  ${MDT_SOURCES}

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  Core
  MC
  SelectionDAG
  Support
  Target

  ADD_TO_COMPONENT
  MDT
)

add_subdirectory(MCTargetDesc)
