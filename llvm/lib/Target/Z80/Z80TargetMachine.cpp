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
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/ValueTracking.h"
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
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar/LoopIdiomRecognize.h"
#include "llvm/Transforms/Utils/LoopRotationUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

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
  initializeZ80MachineAddressOptimizationPass(PR);
  initializeZ80MachinePeepholeOptimizationPass(PR);
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

static cl::opt<bool> EnableSizeLoopIdioms("z80-size-loop-idioms", cl::Hidden,
                                        cl::init(true));

namespace {
// -Oz normally avoids header duplication. For a simple byte copy loop,
// rotation exposes a single-block idiom that can become a compact libcall or
// block instruction. Use the generic passes for all alias/trip-count checks.
struct Z80SizeLoopIdiomPass : PassInfoMixin<Z80SizeLoopIdiomPass> {
  static bool isByteCopyLoop(Loop &L, LoopStandardAnalysisResults &AR) {
    const Function &F = *L.getHeader()->getParent();
    if (F.hasOptNone() || F.getName() == "memcpy" || F.getName() == "memset")
      return false;
    if (DisableLIRP::All || !L.isInnermost() || L.getNumBlocks() != 2 ||
        L.getExitingBlock() != L.getHeader() || L.getHeader()->size() > 8 ||
        !AR.SE.hasLoopInvariantBackedgeTakenCount(&L))
      return false;
    StoreInst *Store = nullptr;
    LoadInst *Load = nullptr;
    for (BasicBlock *BB : L.blocks()) {
      for (Instruction &I : *BB) {
        if (auto *SI = dyn_cast<StoreInst>(&I)) {
          if (Store || SI->isVolatile() || SI->isAtomic() ||
              !SI->getValueOperand()->getType()->isIntegerTy(8))
            return false;
          Store = SI;
        } else if (auto *LI = dyn_cast<LoadInst>(&I)) {
          if (Load || LI->isVolatile() || LI->isAtomic() ||
              !LI->getType()->isIntegerTy(8))
            return false;
          Load = LI;
        } else if (!isa<BranchInst>(I) &&
                   (I.mayHaveSideEffects() || I.mayReadOrWriteMemory()))
          return false;
      }
    }
    if (!Store || Store->getParent() == L.getHeader())
      return false;
    if (!Load)
      return false;
    if (DisableLIRP::Memcpy || !AR.TLI.has(LibFunc_memcpy) ||
        Store->getValueOperand() != Load)
      return false;
    unsigned CountBits =
        AR.SE.getBackedgeTakenCount(&L)->getType()->getScalarSizeInBits();
    const DataLayout &DL = F.getDataLayout();
    if (CountBits >
            DL.getIndexTypeSizeInBits(Store->getPointerOperand()->getType()) ||
        CountBits >
            DL.getIndexTypeSizeInBits(Load->getPointerOperand()->getType()))
      return false;
    auto IsUnitStride = [&](Value *Ptr) {
      auto *Rec = dyn_cast<SCEVAddRecExpr>(AR.SE.getSCEV(Ptr));
      if (!Rec || Rec->getLoop() != &L || !Rec->isAffine())
        return false;
      auto *Step = dyn_cast<SCEVConstant>(Rec->getStepRecurrence(AR.SE));
      return Step && Step->getAPInt().isOne();
    };
    if (!IsUnitStride(Store->getPointerOperand()) ||
        !IsUnitStride(Load->getPointerOperand()))
      return false;
    auto *Dst =
        dyn_cast<Argument>(getUnderlyingObject(Store->getPointerOperand()));
    auto *Src =
        dyn_cast<Argument>(getUnderlyingObject(Load->getPointerOperand()));
    // Avoid rotating an unprofitable may-alias loop. The generic recognizer
    // independently checks the complete accessed ranges before making a copy.
    return Dst && Src && Dst != Src &&
           (Dst->hasNoAliasAttr() || Src->hasNoAliasAttr());
  }

  PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM,
                        LoopStandardAnalysisResults &AR, LPMUpdater &U) {
    if (!isByteCopyLoop(L, AR))
      return PreservedAnalyses::all();
    std::optional<MemorySSAUpdater> MSSAU;
    if (AR.MSSA)
      MSSAU.emplace(AR.MSSA);
    auto SQ = getBestSimplifyQuery(AR, L.getHeader()->getDataLayout());
    if (!LoopRotation(&L, &AR.LI, &AR.TTI, &AR.AC, &AR.DT, &AR.SE,
                      MSSAU ? &*MSSAU : nullptr, SQ, /*RotationOnly=*/false,
                      /*Threshold=*/8, /*IsUtilMode=*/false,
                      /*PrepareForLTO=*/false))
      return PreservedAnalyses::all();
    auto PA = getLoopPassPreservedAnalyses();
    if (AR.MSSA)
      PA.preserve<MemorySSAAnalysis>();
    PA.intersect(LoopIdiomRecognizePass().run(L, AM, AR, U));
    return PA;
  }
};
} // namespace

void Z80TargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
  PB.registerLateLoopOptimizationsEPCallback(
      [](LoopPassManager &LPM, OptimizationLevel Level) {
        if (EnableSizeLoopIdioms &&
            (Level == OptimizationLevel::Os || Level == OptimizationLevel::Oz))
          LPM.addPass(Z80SizeLoopIdiomPass());
      });
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
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(createZ80MachineAddressOptimizationPass());
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(createZ80MachinePeepholeOptimizationPass());
  addPass(createZ80BranchSelectorPass());
}

std::unique_ptr<CSEConfigBase> Z80PassConfig::getCSEConfig() const {
  return getStandardCSEConfigForOpt(TM->getOptLevel());
}
