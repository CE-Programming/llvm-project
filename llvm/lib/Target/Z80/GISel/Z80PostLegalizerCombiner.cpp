//=== lib/CodeGen/GlobalISel/Z80PostLegalizerCombiner.cpp -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass does combining of machine instructions at the generic MI level,
// after the legalizer.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "llvm/CodeGen/GlobalISel/Combiner.h"
#include "llvm/CodeGen/GlobalISel/CombinerHelper.h"
#include "llvm/CodeGen/GlobalISel/CombinerInfo.h"
#include "llvm/CodeGen/GlobalISel/GISelKnownBits.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"

#define DEBUG_TYPE "z80-postlegalizer-combiner"

using namespace llvm;

#define Z80POSTLEGALIZERCOMBINERHELPER_GENCOMBINERHELPER_DEPS
#include "Z80GenPostLegalizeGICombiner.inc"
#undef Z80POSTLEGALIZERCOMBINERHELPER_GENCOMBINERHELPER_DEPS

namespace {
#define Z80POSTLEGALIZERCOMBINERHELPER_GENCOMBINERHELPER_H
#include "Z80GenPostLegalizeGICombiner.inc"
#undef Z80POSTLEGALIZERCOMBINERHELPER_GENCOMBINERHELPER_H

class Z80PostLegalizerCombinerInfo : public CombinerInfo {
  GISelKnownBits *KB;
  MachineDominatorTree *MDT;
  Z80GenPostLegalizerCombinerHelperRuleConfig GeneratedRuleCfg;

public:
  Z80PostLegalizerCombinerInfo(bool EnableOpt, bool OptSize, bool MinSize,
                               GISelKnownBits *KB, MachineDominatorTree *MDT)
      : CombinerInfo(/*AllowIllegalOps*/ true, /*ShouldLegalizeIllegal*/ false,
                     /*LegalizerInfo*/ nullptr, EnableOpt, OptSize, MinSize),
        KB(KB), MDT(MDT) {
    if (!GeneratedRuleCfg.parseCommandLineOption())
      report_fatal_error("Invalid rule identifier");
  }

  virtual bool combine(GISelChangeObserver &Observer, MachineInstr &MI,
                       MachineIRBuilder &B) const override;
};

bool Z80PostLegalizerCombinerInfo::combine(GISelChangeObserver &Observer,
                                           MachineInstr &MI,
                                           MachineIRBuilder &B) const {
  const auto *LI =
      MI.getParent()->getParent()->getSubtarget().getLegalizerInfo();
  CombinerHelper Helper(Observer, B, KB, MDT, LI);
  Z80GenPostLegalizerCombinerHelper Generated(GeneratedRuleCfg, Helper);
  return Generated.tryCombineAll(Observer, MI, B, Helper);
}

#define Z80POSTLEGALIZERCOMBINERHELPER_GENCOMBINERHELPER_CPP
#include "Z80GenPostLegalizeGICombiner.inc"
#undef Z80POSTLEGALIZERCOMBINERHELPER_GENCOMBINERHELPER_CPP

// Pass boilerplate
// ================

class Z80PostLegalizerCombiner : public MachineFunctionPass {
public:
  static char ID;

  Z80PostLegalizerCombiner(bool IsOptNone = false);

  StringRef getPassName() const override {
    return "Z80 Post-Legalizer Combiner";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override;
private:
  bool IsOptNone;
};
} // end anonymous namespace

void Z80PostLegalizerCombiner::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetPassConfig>();
  AU.setPreservesCFG();
  getSelectionDAGFallbackAnalysisUsage(AU);
  AU.addRequired<GISelKnownBitsAnalysis>();
  AU.addPreserved<GISelKnownBitsAnalysis>();
  if (!IsOptNone) {
    AU.addRequired<MachineDominatorTree>();
    AU.addPreserved<MachineDominatorTree>();
  }
  MachineFunctionPass::getAnalysisUsage(AU);
}

Z80PostLegalizerCombiner::Z80PostLegalizerCombiner(bool IsOptNone)
    : MachineFunctionPass(ID), IsOptNone(IsOptNone) {
  initializeZ80PostLegalizerCombinerPass(*PassRegistry::getPassRegistry());
}

bool Z80PostLegalizerCombiner::runOnMachineFunction(MachineFunction &MF) {
  if (MF.getProperties().hasProperty(
          MachineFunctionProperties::Property::FailedISel))
    return false;
  auto *TPC = &getAnalysis<TargetPassConfig>();
  const Function &F = MF.getFunction();
  bool EnableOpt =
      MF.getTarget().getOptLevel() != CodeGenOpt::None && !skipFunction(F);
  GISelKnownBits *KB = &getAnalysis<GISelKnownBitsAnalysis>().get(MF);
  MachineDominatorTree *MDT =
      IsOptNone ? nullptr : &getAnalysis<MachineDominatorTree>();
  Z80PostLegalizerCombinerInfo PCInfo(EnableOpt, F.hasOptSize(), F.hasMinSize(),
                                      KB, MDT);
  Combiner C(PCInfo, TPC);
  return C.combineMachineInstrs(MF, /*CSEInfo*/ nullptr);
}

char Z80PostLegalizerCombiner::ID = 0;
INITIALIZE_PASS_BEGIN(Z80PostLegalizerCombiner, DEBUG_TYPE,
                      "Combine Z80 machine instrs after legalization", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_DEPENDENCY(GISelKnownBitsAnalysis)
INITIALIZE_PASS_END(Z80PostLegalizerCombiner, DEBUG_TYPE,
                    "Combine Z80 machine instrs after legalization", false,
                    false)

FunctionPass *llvm::createZ80PostLegalizeCombiner(bool IsOptNone) {
  return new Z80PostLegalizerCombiner(IsOptNone);
}
