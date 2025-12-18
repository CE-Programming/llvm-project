add_llvm_component_group(Z80)

set(LLVM_TARGET_DEFINITIONS Z80.td)

tablegen(LLVM EZ80GenAsmWriter.inc -gen-asm-writer -asmwriternum=1)
tablegen(LLVM Z80GenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM Z80GenCallingConv.inc -gen-callingconv)
tablegen(LLVM Z80GenPostLegalizeGICombiner.inc -gen-global-isel-combiner
              -combiners="Z80PostLegalizerCombinerHelper")
tablegen(LLVM Z80GenPreLegalizeGICombiner.inc -gen-global-isel-combiner
              -combiners="Z80PreLegalizerCombinerHelper")
tablegen(LLVM Z80GenGlobalISel.inc -gen-global-isel)
tablegen(LLVM Z80GenInstrInfo.inc -gen-instr-info)
tablegen(LLVM Z80GenRegisterBank.inc -gen-register-bank)
tablegen(LLVM Z80GenRegisterInfo.inc -gen-register-info)
tablegen(LLVM Z80GenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(Z80CommonTableGen)

set(sources
  GISel/Z80CallLowering.cpp
  GISel/Z80InlineAsmLowering.cpp
  GISel/Z80InstructionSelector.cpp
  GISel/Z80LegalizerInfo.cpp
  GISel/Z80PostLegalizerCombiner.cpp
  GISel/Z80PreLegalizerCombiner.cpp
  GISel/Z80RegisterBankInfo.cpp
  Z80AsmPrinter.cpp
  Z80BranchSelector.cpp
  Z80CallingConv.cpp
  Z80FrameLowering.cpp
  Z80ISelLowering.cpp
  Z80InstrInfo.cpp
  Z80MCInstLower.cpp
  Z80MachineEarlyOptimization.cpp
  Z80MachineLateOptimization.cpp
  Z80MachinePreRAOptimization.cpp
  Z80PostSelectCombiner.cpp
  Z80RegisterInfo.cpp
  Z80Subtarget.cpp
  Z80TargetMachine.cpp
  Z80TargetObjectFile.cpp
  )

add_llvm_target(Z80CodeGen ${sources}
  LINK_COMPONENTS
  AsmPrinter
  CodeGen
  Core
  GlobalISel
  MC
  SelectionDAG
  Support
  Target
  Z80Desc
  Z80Info

  ADD_TO_COMPONENT
  Z80
  )

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
