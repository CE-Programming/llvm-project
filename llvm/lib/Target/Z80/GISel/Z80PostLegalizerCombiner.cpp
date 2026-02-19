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
#include "Z80Subtarget.h"
#include "llvm/CodeGen/GlobalISel/Combiner.h"
#include "llvm/CodeGen/GlobalISel/CombinerHelper.h"
#include "llvm/CodeGen/GlobalISel/CombinerInfo.h"
#include "llvm/CodeGen/GlobalISel/GIMatchTableExecutorImpl.h"
#include "llvm/CodeGen/GlobalISel/GISelKnownBits.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"

#define DEBUG_TYPE "z80-postlegalizer-combiner"

using namespace llvm;

#define GET_GICOMBINER_DEPS
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_DEPS

namespace {
#define GET_GICOMBINER_TYPES
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_TYPES

class Z80PostLegalizerCombinerImpl : public Combiner {
protected:
  mutable CombinerHelper Helper;
  const Z80PostLegalizerCombinerImplRuleConfig &RuleConfig;
  const Z80Subtarget &STI;

public:
  Z80PostLegalizerCombinerImpl(
      MachineFunction &MF, CombinerInfo &CInfo, const TargetPassConfig *TPC,
      GISelKnownBits &KB, GISelCSEInfo *CSEInfo,
      const Z80PostLegalizerCombinerImplRuleConfig &RuleConfig,
      const Z80Subtarget &STI, MachineDominatorTree *MDT,
      const LegalizerInfo *LI);

  static const char *getName() { return "Z80PostLegalizerCombiner"; }

  bool tryCombineAll(MachineInstr &I) const override;

private:
#define GET_GICOMBINER_CLASS_MEMBERS
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_CLASS_MEMBERS
};

#define GET_GICOMBINER_IMPL
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_IMPL

Z80PostLegalizerCombinerImpl::Z80PostLegalizerCombinerImpl(
    MachineFunction &MF, CombinerInfo &CInfo, const TargetPassConfig *TPC,
    GISelKnownBits &KB, GISelCSEInfo *CSEInfo,
    const Z80PostLegalizerCombinerImplRuleConfig &RuleConfig,
    const Z80Subtarget &STI, MachineDominatorTree *MDT,
    const LegalizerInfo *LI)
    : Combiner(MF, CInfo, TPC, &KB, CSEInfo),
      Helper(Observer, B, /*IsPreLegalize*/ false, &KB, MDT, LI),
      RuleConfig(RuleConfig), STI(STI),
#define GET_GICOMBINER_CONSTRUCTOR_INITS
#include "Z80GenPostLegalizeGICombiner.inc"
#undef GET_GICOMBINER_CONSTRUCTOR_INITS
{
}

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
  Z80PostLegalizerCombinerImplRuleConfig RuleConfig;
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

  if (!RuleConfig.parseCommandLineOption())
    report_fatal_error("Invalid rule identifier");
}

bool Z80PostLegalizerCombiner::runOnMachineFunction(MachineFunction &MF) {
  if (MF.getProperties().hasProperty(
          MachineFunctionProperties::Property::FailedISel))
    return false;
  auto &TPC = getAnalysis<TargetPassConfig>();
  const Z80Subtarget &ST = MF.getSubtarget<Z80Subtarget>();
  const auto *LI = ST.getLegalizerInfo();
  const Function &F = MF.getFunction();
  bool EnableOpt =
      MF.getTarget().getOptLevel() != CodeGenOptLevel::None &&
      !skipFunction(F);
  GISelKnownBits *KB = &getAnalysis<GISelKnownBitsAnalysis>().get(MF);
  MachineDominatorTree *MDT =
      IsOptNone ? nullptr : &getAnalysis<MachineDominatorTree>();
  CombinerInfo CInfo(/*AllowIllegalOps*/ true, /*ShouldLegalizeIllegal*/ false,
                     /*LegalizerInfo*/ nullptr, EnableOpt, F.hasOptSize(),
                     F.hasMinSize());
  Z80PostLegalizerCombinerImpl Impl(MF, CInfo, &TPC, *KB, /*CSEInfo*/ nullptr,
                                    RuleConfig, ST, MDT, LI);
  return Impl.combineMachineInstrs();
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
