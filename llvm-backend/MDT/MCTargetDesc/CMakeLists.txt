#===----------------------------------------------------------------------===#
# MDT MC layer - assembly syntax properties and instruction printing
#===----------------------------------------------------------------------===#

set(MDT_MC_SOURCES)

foreach(src
        MDTMCTargetDesc.cpp
        MDTMCAsmInfo.cpp
        MDTInstPrinter.cpp)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${src})
    list(APPEND MDT_MC_SOURCES ${src})
  endif()
endforeach()

if(NOT MDT_MC_SOURCES)
  message(STATUS "MDT: MC layer not implemented yet")
  return()
endif()

add_llvm_component_library(LLVMMDTDesc
  ${MDT_MC_SOURCES}

  LINK_COMPONENTS
  MC
  Support
  MDTInfo
)
