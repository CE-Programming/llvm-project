add_llvm_component_library(LLVMZ80Desc
  EZ80InstPrinter.cpp
  Z80InstPrinter.cpp
  Z80InstPrinterCommon.cpp
  Z80MCAsmInfo.cpp
  Z80MCTargetDesc.cpp
  Z80TargetStreamer.cpp

  LINK_COMPONENTS
  MC
  MCDisassembler
  Object
  Support
  Z80Info

  ADD_TO_COMPONENT
  Z80
  )
