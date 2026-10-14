//===-- Z80InternalRegArgs.cpp - Internal register arguments ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// At the end of LTO, internal functions whose entire call graph is visible can
// pass up to three scalar arguments in HL/DE/BC instead of on the stack. Keep
// exported functions, escaped pointers and calls with special ABI requirements
// on the public C convention. Keep the same returns and IX preservation.
//
//===----------------------------------------------------------------------===//

#include "Z80InternalRegArgs.h"
#include "Z80TargetMachine.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

static cl::opt<bool> EnableInternalRegArgs(
    "z80-internal-regargs", cl::Hidden, cl::init(true),
    cl::desc(
        "Use register arguments for eligible internal functions after LTO"));

// Some calculator builds perform LTO as llvm-link followed by clang -x ir.
// That pipeline has no LTO phase marker, so require an explicit opt-in there.
static cl::opt<bool> InternalRegArgsMergedIR(
    "z80-internal-regargs-on-merged-ir", cl::Hidden, cl::init(false),
    cl::desc("Enable internal register arguments in ordinary optimization of "
             "externally merged LTO IR"));

namespace {

bool hasSpecialABIAttrs(const AttributeList &Attrs) {
  for (Attribute::AttrKind Kind :
       {Attribute::ByVal, Attribute::ByRef, Attribute::InAlloca,
        Attribute::InReg, Attribute::Nest, Attribute::Preallocated,
        Attribute::StructRet, Attribute::SwiftSelf, Attribute::SwiftError,
        Attribute::SwiftAsync, Attribute::Returned})
    if (Attrs.hasAttrSomewhere(Kind))
      return true;
  return false;
}

bool hasModeOverride(const Function &F) {
  // This backend's frame/register ABI is selected by the module triple. Avoid
  // per-function mode overrides; interworking would need a separate ABI.
  StringRef Features = F.getFnAttribute("target-features").getValueAsString();
  return Features.contains("16bit-mode") || Features.contains("24bit-mode");
}

bool isSupportedScalar(Type *Ty, unsigned NativeBits) {
  if (auto *PTy = dyn_cast<PointerType>(Ty))
    return PTy->getAddressSpace() == 0;
  auto *ITy = dyn_cast<IntegerType>(Ty);
  if (!ITy)
    return false;
  unsigned Bits = ITy->getBitWidth();
  return Bits <= NativeBits &&
         (Bits == 1 || Bits == 8 || Bits == 16 || Bits == 24);
}

class Z80InternalRegArgsPass : public PassInfoMixin<Z80InternalRegArgsPass> {
  const Z80TargetMachine &TM;

  bool isEligible(Function &F, SmallVectorImpl<CallInst *> &Calls) const {
    // Local linkage is mandatory even under LTO: dynamic/public ABI and any
    // reference outside this module must remain the standard C convention.
    if (!F.hasLocalLinkage() || F.isDeclaration() || F.isVarArg() ||
        (F.getCallingConv() != CallingConv::C &&
         F.getCallingConv() != CallingConv::Fast) ||
        F.hasOptNone() || F.hasFnAttribute("interrupt") ||
        F.hasFnAttribute(Attribute::Naked) ||
        F.hasFnAttribute(Attribute::ReturnsTwice) || hasModeOverride(F) ||
        F.arg_empty() || F.arg_size() > 3 ||
        hasSpecialABIAttrs(F.getAttributes()))
      return false;

    unsigned NativeBits = TM.getSubtargetImpl(F)->is24Bit() ? 24 : 16;
    if (!F.getReturnType()->isVoidTy() &&
        !isSupportedScalar(F.getReturnType(), NativeBits))
      return false;
    for (Argument &Arg : F.args())
      if (!isSupportedScalar(Arg.getType(), NativeBits))
        return false;

    // Changing a caller's convention would invalidate a musttail call's ABI
    // contract even when the call targets a different function.
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *Call = dyn_cast<CallInst>(&I); Call && Call->isMustTailCall())
          return false;

    for (Use &U : F.uses()) {
      auto *Call = dyn_cast<CallInst>(U.getUser());
      if (!Call || &U != &Call->getCalledOperandUse() ||
          Call->getCalledOperand() != &F || Call->isMustTailCall() ||
          Call->getCallingConv() != F.getCallingConv() ||
          Call->getFunctionType() != F.getFunctionType() ||
          Call->hasOperandBundles() ||
          hasSpecialABIAttrs(Call->getAttributes()) ||
          Call->hasFnAttr(Attribute::ReturnsTwice) ||
          hasModeOverride(*Call->getFunction()) ||
          TM.getSubtargetImpl(*Call->getFunction())->is24Bit() !=
              TM.getSubtargetImpl(F)->is24Bit())
        return false;
      Calls.push_back(Call);
    }
    return !Calls.empty();
  }

public:
  explicit Z80InternalRegArgsPass(const Z80TargetMachine &TM) : TM(TM) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    if (!EnableInternalRegArgs)
      return PreservedAnalyses::all();
    bool Changed = false;
    for (Function &F : M) {
      SmallVector<CallInst *, 8> Calls;
      if (!isEligible(F, Calls))
        continue;
      F.setCallingConv(CallingConv::Z80_Internal);
      for (CallInst *Call : Calls)
        Call->setCallingConv(CallingConv::Z80_Internal);
      Changed = true;
    }
    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

} // namespace

void llvm::registerZ80InternalRegArgsCallbacks(PassBuilder &PB,
                                               const Z80TargetMachine &TM) {
  PB.registerPipelineParsingCallback(
      [&TM](StringRef Name, ModulePassManager &MPM,
            ArrayRef<PassBuilder::PipelineElement>) {
        if (Name != "z80-internal-regargs")
          return false;
        MPM.addPass(Z80InternalRegArgsPass(TM));
        return true;
      });
  PB.registerFullLinkTimeOptimizationLastEPCallback(
      [&TM](ModulePassManager &MPM, OptimizationLevel Level) {
        if (Level != OptimizationLevel::O0)
          MPM.addPass(Z80InternalRegArgsPass(TM));
      });
  PB.registerOptimizerLastEPCallback([&TM](ModulePassManager &MPM,
                                           OptimizationLevel Level,
                                           ThinOrFullLTOPhase Phase) {
    if (Level != OptimizationLevel::O0 &&
        (Phase == ThinOrFullLTOPhase::ThinLTOPostLink ||
         (Phase == ThinOrFullLTOPhase::None && InternalRegArgsMergedIR)))
      MPM.addPass(Z80InternalRegArgsPass(TM));
  });
}
