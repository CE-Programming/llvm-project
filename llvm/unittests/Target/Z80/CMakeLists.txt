include_directories(
  ${CMAKE_SOURCE_DIR}/lib/Target/Z80
  ${CMAKE_BINARY_DIR}/lib/Target/Z80
  )

set(LLVM_LINK_COMPONENTS
  CodeGen
  Core
  GlobalISel
  MC
  MIRParser
  SelectionDAG
  Support
  Target
  Z80CodeGen
  Z80Desc
  Z80Info
  )

add_llvm_target_unittest(Z80Tests
  InstSizes.cpp
  )
