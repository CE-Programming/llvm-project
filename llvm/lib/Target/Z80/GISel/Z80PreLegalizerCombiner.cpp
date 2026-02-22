//=== lib/CodeGen/GlobalISel/Z80PreLegalizerCombiner.cpp ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass does combining of machine instructions at the generic MI level,
// before the legalizer.
//
//===----------------------------------------------------------------------===//

#include "Z80.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"
#include "llvm/CodeGen/GlobalISel/Combiner.h"
#include "llvm/CodeGen/GlobalISel/CombinerHelper.h"
#include "llvm/CodeGen/GlobalISel/CombinerInfo.h"
#include "llvm/CodeGen/GlobalISel/GIMatchTableExecutorImpl.h"
#include "llvm/CodeGen/GlobalISel/GISelKnownBits.h"
#include "llvm/CodeGen/GlobalISel/MIPatternMatch.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"

#define DEBUG_TYPE "z80-prelegalizer-combiner"

using namespace llvm;
using namespace llvm::MIPatternMatch;

#define GET_GICOMBINER_DEPS
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_DEPS

namespace {
#define GET_GICOMBINER_TYPES
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_TYPES

class Z80PreLegalizerCombinerImpl : public Combiner {
protected:
  mutable CombinerHelper Helper;
  const Z80PreLegalizerCombinerImplRuleConfig &RuleConfig;
  const Z80Subtarget &STI;

public:
  Z80PreLegalizerCombinerImpl(
      MachineFunction &MF, CombinerInfo &CInfo, const TargetPassConfig *TPC,
      GISelKnownBits &KB, GISelCSEInfo *CSEInfo,
      const Z80PreLegalizerCombinerImplRuleConfig &RuleConfig,
      const Z80Subtarget &STI, MachineDominatorTree *MDT,
      const LegalizerInfo *LI);

  static const char *getName() { return "Z80PreLegalizerCombiner"; }

  bool tryCombineAll(MachineInstr &I) const override;

private:
#define GET_GICOMBINER_CLASS_MEMBERS
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_CLASS_MEMBERS
};

static bool matchCombineTruncShift(MachineInstr &MI, MachineRegisterInfo &MRI,
                                   Register &SrcReg) {
  Register DstReg = MI.getOperand(0).getReg();
  LLT DstTy = MRI.getType(DstReg);
  if (DstTy != LLT::scalar(8))
    return false;
  int64_t ShiftAmt;
  if (!mi_match(DstReg, MRI,
                m_GTrunc(m_GLShr(m_Reg(SrcReg), m_ICst(ShiftAmt)))) ||
      ShiftAmt != 8)
    return false;
  LLT SrcTy = MRI.getType(SrcReg);
  return SrcTy.isScalar() && SrcTy.getSizeInBits() >= 16;
}

static void applyCombineTruncShift(MachineInstr &MI, MachineIRBuilder &Builder,
                                   GISelChangeObserver &Observer,
                                   Register SrcReg) {
  Builder.setInstrAndDebugLoc(MI);
  Builder.buildExtract(MI.getOperand(0), SrcReg, 8);

  Observer.erasingInstr(MI);
  MI.eraseFromParent();
}

static bool matchFlipSetCCCond(MachineInstr &MI, MachineRegisterInfo &MRI,
                               MachineInstr *&SetCCMI) {
  int64_t Imm;
  return mi_match(MI.getOperand(0).getReg(), MRI,
                  m_GXor(m_OneUse(m_MInstr(SetCCMI)), m_ICst(Imm))) &&
         SetCCMI->getOpcode() == Z80::SetCC && Imm;
}

static void applyFlipSetCCCond(MachineInstr &MI, MachineIRBuilder &Builder,
                               GISelChangeObserver &Observer,
                               MachineInstr &SetCCMI) {
  // Warning, SetCC has a physreg use, so don't create the SetCC at the G_XOR!
  Observer.changingInstr(SetCCMI);
  SetCCMI.getOperand(0).setReg(MI.getOperand(0).getReg());
  MachineOperand &Cond = SetCCMI.getOperand(1);
  Cond.setImm(Z80::GetOppositeBranchCondition(Z80::CondCode(Cond.getImm())));
  Observer.changedInstr(SetCCMI);

  Observer.erasingInstr(MI);
  MI.eraseFromParent();
}

#define GET_GICOMBINER_IMPL
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_IMPL

Z80PreLegalizerCombinerImpl::Z80PreLegalizerCombinerImpl(
    MachineFunction &MF, CombinerInfo &CInfo, const TargetPassConfig *TPC,
    GISelKnownBits &KB, GISelCSEInfo *CSEInfo,
    const Z80PreLegalizerCombinerImplRuleConfig &RuleConfig,
    const Z80Subtarget &STI, MachineDominatorTree *MDT,
    const LegalizerInfo *LI)
    : Combiner(MF, CInfo, TPC, &KB, CSEInfo),
      Helper(Observer, B, /*IsPreLegalize*/ true, &KB, MDT, LI),
      RuleConfig(RuleConfig), STI(STI),
#define GET_GICOMBINER_CONSTRUCTOR_INITS
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_CONSTRUCTOR_INITS
{
}

// Pass boilerplate
// ================

class Z80PreLegalizerCombiner : public MachineFunctionPass {
public:
  static char ID;

  Z80PreLegalizerCombiner(bool IsOptNone = false);

  StringRef getPassName() const override {
    return "Z80 Pre-Legalizer Combiner";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override;
private:
  bool IsOptNone;
  Z80PreLegalizerCombinerImplRuleConfig RuleConfig;
};
} // end anonymous namespace

void Z80PreLegalizerCombiner::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetPassConfig>();
  AU.setPreservesCFG();
  getSelectionDAGFallbackAnalysisUsage(AU);
  AU.addRequired<GISelKnownBitsAnalysis>();
  AU.addPreserved<GISelKnownBitsAnalysis>();
  if (!IsOptNone) {
    AU.addRequired<MachineDominatorTreeWrapperPass>();
    AU.addPreserved<MachineDominatorTreeWrapperPass>();
  }
  MachineFunctionPass::getAnalysisUsage(AU);
}

Z80PreLegalizerCombiner::Z80PreLegalizerCombiner(bool IsOptNone)
    : MachineFunctionPass(ID), IsOptNone(IsOptNone) {
  initializeZ80PreLegalizerCombinerPass(*PassRegistry::getPassRegistry());

  // FIXME: On Z80/eZ80, the generic sdiv_by_pow2 pre-legalizer combine can
  // regress signed i24 arithmetic used in layout-heavy code (for example
  // libtexce matrix sizing paths), producing incorrect rendering. Keep it
  // disabled by default for this target until we have a target-aware variant.
  // This remains overridable via
  // -z80prelegalizercombiner-(only-)enable/disable-rule.
  if (!RuleConfig.setRuleDisabled("sdiv_by_pow2"))
    report_fatal_error("Invalid rule identifier");

  if (!RuleConfig.parseCommandLineOption())
    report_fatal_error("Invalid rule identifier");
}

bool Z80PreLegalizerCombiner::runOnMachineFunction(MachineFunction &MF) {
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
      IsOptNone ? nullptr
                : &getAnalysis<MachineDominatorTreeWrapperPass>().getDomTree();
  CombinerInfo CInfo(/*AllowIllegalOps*/ true, /*ShouldLegalizeIllegal*/ false,
                     /*LegalizerInfo*/ nullptr, EnableOpt, F.hasOptSize(),
                     F.hasMinSize());
  Z80PreLegalizerCombinerImpl Impl(MF, CInfo, &TPC, *KB, /*CSEInfo*/ nullptr,
                                   RuleConfig, ST, MDT, LI);
  return Impl.combineMachineInstrs();
}

char Z80PreLegalizerCombiner::ID = 0;
INITIALIZE_PASS_BEGIN(Z80PreLegalizerCombiner, DEBUG_TYPE,
                      "Combine Z80 machine instrs before legalization", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_DEPENDENCY(GISelKnownBitsAnalysis)
INITIALIZE_PASS_END(Z80PreLegalizerCombiner, DEBUG_TYPE,
                    "Combine Z80 machine instrs before legalization", false,
                    false)

FunctionPass *llvm::createZ80PreLegalizeCombiner(bool IsOptNone) {
  return new Z80PreLegalizerCombiner(IsOptNone);
}
