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
#include "Z80Subtarget.h"
#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80InstrInfo.h"
#include "llvm/CodeGen/GlobalISel/CSEInfo.h"
#include "llvm/CodeGen/GlobalISel/Combiner.h"
#include "llvm/CodeGen/GlobalISel/CombinerHelper.h"
#include "llvm/CodeGen/GlobalISel/CombinerInfo.h"
#include "llvm/CodeGen/GlobalISel/GIMatchTableExecutorImpl.h"
#include "llvm/CodeGen/GlobalISel/GISelKnownBits.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/GlobalISel/MIPatternMatch.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetMachine.h"

#define GET_GICOMBINER_DEPS
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_DEPS

#define DEBUG_TYPE "z80-prelegalizer-combiner"

using namespace llvm;
using namespace llvm::MIPatternMatch;

namespace {

// Alias for std::optional to satisfy generated code which uses Optional<T>
template <typename T> using Optional = std::optional<T>;

#define GET_GICOMBINER_TYPES
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_TYPES

bool matchCombineTruncShift(MachineInstr &MI, MachineRegisterInfo &MRI,
                            Register &SrcReg) {
  Register DstReg = MI.getOperand(0).getReg();
  LLT DstTy = MRI.getType(DstReg);
  if (DstTy != LLT::scalar(8))
    return false;
  unsigned ShiftAmt;
  if (!mi_match(DstReg, MRI,
                m_GTrunc(m_GLShr(m_Reg(SrcReg), m_ICst(ShiftAmt)))) ||
      ShiftAmt != 8)
    return false;
  LLT SrcTy = MRI.getType(SrcReg);
  return SrcTy.isScalar() && SrcTy.getSizeInBits() >= 16;
}

void applyCombineTruncShift(MachineInstr &MI, MachineIRBuilder &Builder,
                            GISelChangeObserver &Observer,
                            Register SrcReg) {
  Builder.setInstrAndDebugLoc(MI);
  Builder.buildExtract(MI.getOperand(0), SrcReg, 8);

  Observer.erasingInstr(MI);
  MI.eraseFromParent();
}

bool matchFlipSetCCCond(MachineInstr &MI, MachineRegisterInfo &MRI,
                        MachineInstr *&SetCCMI) {
  bool Imm;
  return mi_match(MI.getOperand(0).getReg(), MRI,
                  m_GXor(m_OneUse(m_MInstr(SetCCMI)), m_ICst(Imm))) &&
         SetCCMI->getOpcode() == Z80::SetCC && Imm;
}

void applyFlipSetCCCond(MachineInstr &MI, MachineIRBuilder &Builder,
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

  bool tryCombineAllImpl(MachineInstr &I) const;

private:
#define GET_GICOMBINER_CLASS_MEMBERS
#include "Z80GenPreLegalizeGICombiner.inc"
#undef GET_GICOMBINER_CLASS_MEMBERS
};

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

bool Z80PreLegalizerCombinerImpl::tryCombineAll(MachineInstr &I) const {
  return tryCombineAllImpl(I);
}

// Pass boilerplate
// ================

class Z80PreLegalizerCombiner : public MachineFunctionPass {
public:
  static char ID;

  Z80PreLegalizerCombiner();

  StringRef getPassName() const override {
    return "Z80 Pre-Legalizer Combiner";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override;

private:
  Z80PreLegalizerCombinerImplRuleConfig RuleConfig;
};
} // end anonymous namespace

void Z80PreLegalizerCombiner::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<TargetPassConfig>();
  AU.setPreservesCFG();
  getSelectionDAGFallbackAnalysisUsage(AU);
  AU.addRequired<GISelKnownBitsAnalysis>();
  AU.addPreserved<GISelKnownBitsAnalysis>();
  AU.addRequired<MachineDominatorTreeWrapperPass>();
  AU.addPreserved<MachineDominatorTreeWrapperPass>();
  AU.addRequired<GISelCSEAnalysisWrapperPass>();
  AU.addPreserved<GISelCSEAnalysisWrapperPass>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

Z80PreLegalizerCombiner::Z80PreLegalizerCombiner()
    : MachineFunctionPass(ID) {
  initializeZ80PreLegalizerCombinerPass(*PassRegistry::getPassRegistry());

  if (!RuleConfig.parseCommandLineOption())
    report_fatal_error("Invalid rule identifier");
}

bool Z80PreLegalizerCombiner::runOnMachineFunction(MachineFunction &MF) {
  if (MF.getProperties().hasProperty(
          MachineFunctionProperties::Property::FailedISel))
    return false;
  auto &TPC = getAnalysis<TargetPassConfig>();

  // Enable CSE.
  GISelCSEAnalysisWrapper &Wrapper =
      getAnalysis<GISelCSEAnalysisWrapperPass>().getCSEWrapper();
  auto *CSEInfo = &Wrapper.get(TPC.getCSEConfig());

  const Z80Subtarget &ST = MF.getSubtarget<Z80Subtarget>();
  const auto *LI = ST.getLegalizerInfo();

  const Function &F = MF.getFunction();
  bool EnableOpt =
      MF.getTarget().getOptLevel() != CodeGenOptLevel::None && !skipFunction(F);
  GISelKnownBits *KB = &getAnalysis<GISelKnownBitsAnalysis>().get(MF);
  MachineDominatorTree *MDT =
      &getAnalysis<MachineDominatorTreeWrapperPass>().getDomTree();
  CombinerInfo CInfo(/*AllowIllegalOps*/ true, /*ShouldLegalizeIllegal*/ false,
                     /*LegalizerInfo*/ nullptr, EnableOpt, F.hasOptSize(),
                     F.hasMinSize());
  Z80PreLegalizerCombinerImpl Impl(MF, CInfo, &TPC, *KB, CSEInfo, RuleConfig,
                                   ST, MDT, LI);
  return Impl.combineMachineInstrs();
}

char Z80PreLegalizerCombiner::ID = 0;
INITIALIZE_PASS_BEGIN(Z80PreLegalizerCombiner, DEBUG_TYPE,
                      "Combine Z80 machine instrs before legalization", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_DEPENDENCY(GISelKnownBitsAnalysis)
INITIALIZE_PASS_DEPENDENCY(GISelCSEAnalysisWrapperPass)
INITIALIZE_PASS_END(Z80PreLegalizerCombiner, DEBUG_TYPE,
                    "Combine Z80 machine instrs before legalization", false,
                    false)

FunctionPass *llvm::createZ80PreLegalizeCombiner() {
  return new Z80PreLegalizerCombiner();
}
