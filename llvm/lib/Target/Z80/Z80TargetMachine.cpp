//===-- Z80TargetMachine.cpp - Define TargetMachine for the Z80 -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the Z80 specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#include "Z80TargetMachine.h"
#include "TargetInfo/Z80TargetInfo.h"
#include "Z80.h"
#include "Z80MachineFunctionInfo.h"
#include "Z80Subtarget.h"
#include "Z80TargetObjectFile.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/GlobalISel/CallLowering.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Function.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"

#include <llvm/CodeGen/GlobalISel/CSEInfo.h>
using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeZ80Target() {
  // Register the target.
  RegisterTargetMachine<Z80TargetMachine> X(getTheZ80Target());
  RegisterTargetMachine<Z80TargetMachine> Y(getTheEZ80Target());

  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeZ80PreLegalizerCombinerPass(PR);
  initializeGlobalISel(PR);
  initializeZ80PostLegalizerCombinerPass(PR);
  initializeZ80PostSelectCombinerPass(PR);
  initializeZ80MachineEarlyOptimizationPass(PR);
  initializeZ80R64SpillPassPass(PR);
  initializeZ80MachineLateOptimizationPass(PR);
  initializeZ80BranchSelectorPass(PR);
}


static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  if (RM)
    return *RM;
  return Reloc::Static;
}

/// Create a Z80 target.
///
Z80TargetMachine::Z80TargetMachine(const Target &T, const Triple &TT,
                                   StringRef CPU, StringRef FS,
                                   const TargetOptions &Options,
                                   std::optional<Reloc::Model> RM,
                                   std::optional<CodeModel::Model> CM,
                                   CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, TT.computeDataLayout(), TT, CPU, FS, Options,
                               getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<Z80ELFTargetObjectFile>()) {
  initAsmInfo();

  setGlobalISel(true);
  setGlobalISelAbort(GlobalISelAbortMode::Enable);
}

Z80TargetMachine::~Z80TargetMachine() {}

const Z80Subtarget *
Z80TargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute TuneAttr = F.getFnAttribute("tune-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  StringRef CPU = !CPUAttr.hasAttribute(Attribute::None)
                      ? CPUAttr.getValueAsString()
                      : (StringRef)TargetCPU;
  StringRef TuneCPU = !TuneAttr.hasAttribute(Attribute::None)
                          ? TuneAttr.getValueAsString()
                          : (StringRef)CPU;
  StringRef FS = !FSAttr.hasAttribute(Attribute::None)
                     ? FSAttr.getValueAsString()
                     : (StringRef)TargetFS;

  SmallString<512> Key;
  Key.reserve(CPU.size() + 5 + TuneCPU.size() + FS.size());
  Key += CPU;
  Key += "tune=";
  Key += TuneCPU;
  Key += FS;

  auto &I = SubtargetMap[Key];
  if (!I) {
    // This needs to be done before we create a new subtarget since any
    // creation will depend on the TM and the code generation flags on the
    // function that reside in TargetOptions.
    resetTargetOptions(F);
    I = std::make_unique<Z80Subtarget>(TargetTriple, CPU, TuneCPU, FS, *this);
  }
  return I.get();
}

//===----------------------------------------------------------------------===//
// Pass Pipeline Configuration
//===----------------------------------------------------------------------===//

namespace {
/// Z80 Code Generator Pass Configuration Options.
class Z80PassConfig : public TargetPassConfig {
public:
  Z80PassConfig(Z80TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  Z80TargetMachine &getZ80TargetMachine() const {
    return getTM<Z80TargetMachine>();
  }

  bool addIRTranslator() override;
  void addPreLegalizeMachineIR() override;
  bool addLegalizeMachineIR() override;
  void addPreRegBankSelect() override;
  bool addRegBankSelect() override;
  bool addGlobalInstructionSelect() override;
  void addMachineSSAOptimization() override;
  void addFastRegAlloc() override;
  bool addRegAssignAndRewriteOptimized() override;
  void addMachineLateOptimization() override;
  void addPreEmitPass2() override;

  std::unique_ptr<CSEConfigBase> getCSEConfig() const override;
};
} // end anonymous namespace

TargetPassConfig *Z80TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new Z80PassConfig(*this, PM);
}

MachineFunctionInfo *Z80TargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return Z80MachineFunctionInfo::create<Z80MachineFunctionInfo>(Allocator, F,
                                                                STI);
}

bool Z80PassConfig::addIRTranslator() {
  addPass(new IRTranslator);
  return false;
}

void Z80PassConfig::addPreLegalizeMachineIR() {
  bool IsOptNone = getOptLevel() == CodeGenOptLevel::None;
  addPass(createZ80PreLegalizeCombiner(IsOptNone));
}

bool Z80PassConfig::addLegalizeMachineIR() {
  addPass(new Legalizer);
  return false;
}

void Z80PassConfig::addPreRegBankSelect() {
  // For now we don't add this to the pipeline for -O0. We could do in future
  // if we split the combines into separate O0/opt groupings.
  bool IsOptNone = getOptLevel() == CodeGenOptLevel::None;
  if (!IsOptNone)
    addPass(createZ80PostLegalizeCombiner(IsOptNone));
}

bool Z80PassConfig::addRegBankSelect() {
  addPass(new RegBankSelect);
  return false;
}

bool Z80PassConfig::addGlobalInstructionSelect() {
  addPass(new InstructionSelect);
  return false;
}

void Z80PassConfig::addMachineSSAOptimization() {
  addPass(createZ80PostSelectCombiner());
  addPass(createZ80R64SpillPass());
  TargetPassConfig::addMachineSSAOptimization();
  addPass(createZ80MachineEarlyOptimizationPass());
  addPass(createZ80MachinePreRAOptimizationPass());
}

void Z80PassConfig::addFastRegAlloc() {
  // FastRegAlloc can't handle the register pressure on the Z80
  if (usingDefaultRegAlloc()) {
    addPass(createZ80R64SpillPass());
    addOptimizedRegAlloc();
  } else
    TargetPassConfig::addFastRegAlloc();
}

bool Z80PassConfig::addRegAssignAndRewriteOptimized() {
  return TargetPassConfig::addRegAssignAndRewriteOptimized();
}

void Z80PassConfig::addMachineLateOptimization() {
  TargetPassConfig::addMachineLateOptimization();
  addPass(createZ80MachineLateOptimizationPass());
}

void Z80PassConfig::addPreEmitPass2() {
  TargetPassConfig::addPreEmitPass2();
  addPass(createZ80BranchSelectorPass());
}

std::unique_ptr<CSEConfigBase> Z80PassConfig::getCSEConfig() const {
  return getStandardCSEConfigForOpt(TM->getOptLevel());
}
