add_llvm_component_library(LLVMZ80Info
  Z80TargetInfo.cpp

  LINK_COMPONENTS
  Support

  ADD_TO_COMPONENT
  Z80
  )
