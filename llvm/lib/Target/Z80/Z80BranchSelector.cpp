//===-- Z80BranchSelector.cpp - Emit absolute conditional branches --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that scans a machine function to determine which
// conditional branches need more than 8 bits of displacement to reach their
// target basic block.  It does this in two passes; a calculation of basic block
// positions pass, and a branch pseudo op to machine branch opcode pass.  This
// pass should be run last, just before the assembly printer.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/Z80MCTargetDesc.h"
#include "Z80.h"
#include "Z80InstrInfo.h"
#include "Z80Subtarget.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SparseSet.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Support/MathExtras.h"
using namespace llvm;

#define DEBUG_TYPE "z80-branch-select"

namespace {

template <typename T> struct Range {
  T Min, Max;

  constexpr Range(T Val = T()) : Min(Val), Max(Val) {}
  constexpr Range(T Min, T Max) : Min(Min), Max(Max) {}

  constexpr T delta() const { return Max - Min; }

  constexpr Range operator+() const { return {+Min, +Max}; }
  constexpr Range operator-() const { return {-Min, -Max}; }

  constexpr Range &operator+=(const Range &Other) {
    Min += Other.Min;
    Max += Other.Max;
    return *this;
  }
  constexpr Range &operator-=(const Range &Other) {
    Min -= Other.Min;
    Max -= Other.Max;
    return *this;
  }

  constexpr Range operator+(const Range &Other) const {
    Range Result(*this);
    Result += Other;
    return Result;
  }
  constexpr Range operator-(const Range &Other) const {
    Range Result(*this);
    Result -= Other;
    return Result;
  }

  constexpr bool operator==(const Range &Other) const {
    return Min == Other.Min && Max == Other.Max;
  }
  constexpr bool operator!=(const Range &Other) const {
    return !(*this == Other);
  }
};

template <typename T>
constexpr raw_ostream &operator<<(raw_ostream &S, const Range<T> &R) {
  S << R.Min;
  if (R.Max != R.Min)
    S << '-' << R.Max;
  return S;
}

template <typename T> class OffsetTree {
public:
  using size_type = uint32_t;
  using value_type = T;

  enum State {
    Unfrozen,
    Frozen,
#ifndef NDEBUG
    Debug,
#endif
  };

  size_type size() const { return Tree.size() - 1; }
  size_type capacity() const { return Tree.capacity() - 1; }

  bool empty() const { return !size(); }

  void clear() {
    Tree.clear();
    Tree.emplace_back();
#ifndef NDEBUG
    IsFrozen = false;
#endif
  }
  void reserve(size_type Size) { Tree.reserve(Size + 1); }

  void advance(const T &Amt) {
    assert(!IsFrozen);
    Tree.back() += Amt;
  }

  void add() {
    assert(!IsFrozen);
    Tree.push_back(Tree.back());
  }

  template <State S> T get(size_type Idx) const {
    assert(S == (IsFrozen ? Frozen : Unfrozen) || S == Debug);
    assert(Idx <= size());
    if (S == Unfrozen
#ifndef NDEBUG
        || (S == Debug && !IsFrozen)
#endif
    )
      return Tree[Idx];
    T Result{};
    Optional<size_type> CurrentIdx = Idx;
    do
      Result += Tree[*CurrentIdx];
    while ((CurrentIdx = toParentIndex(*CurrentIdx)));
    return Result;
  }

  template <State S> T current() const { return get<S>(size()); }

  void freeze() {
    assert(!std::exchange(IsFrozen, true));
    for (size_type FirstIdx = 0, Stride = 1; FirstIdx + Stride <= size();
         FirstIdx = Stride - 1) {
      Stride <<= 1;
      for (size_type ChildIdx = FirstIdx; ChildIdx <= size();
           ChildIdx += Stride)
        Tree[ChildIdx] -= Tree[*toParentIndex(ChildIdx)];
    }
  }

  void adjust(size_type Idx, T Amt) {
    assert(IsFrozen);
    Optional<size_type> ParentIdx = Idx, ChildIdx = toFirstChildIndex(Idx);
    do {
      if (ChildIdx)
        Tree[*ChildIdx] -= Amt;
      do
        ParentIdx = toParentIndex(*(ChildIdx = ParentIdx));
      while (ParentIdx && *ParentIdx > Idx);
      assert(!ParentIdx || *ParentIdx != Idx);
      Tree[*ChildIdx] += Amt;
      while (ParentIdx && *ParentIdx < Idx)
        ParentIdx = toParentIndex(*(ChildIdx = ParentIdx));
      assert(!ParentIdx || *ParentIdx != Idx);
    } while (ParentIdx);
  }

private:
  // This returns a mask with a single bit set in the same location as the least
  // significant unset bit of X.
  static size_type lowestZeroMask(size_type X) { return ~X & (X + 1); }
  Optional<size_type> toParentIndex(size_type Idx) const {
    assert(Idx <= size());
    size_type Mask = lowestZeroMask(Idx);
    do {
      if (Mask > size())
        return None;
      Idx |= Mask;
      Mask <<= 1;
      Idx &= ~Mask;
    } while (Idx > size());
    return Idx;
  }
  Optional<size_type> toFirstChildIndex(size_type Idx) {
    assert(Idx <= size());
    size_type Mask = lowestZeroMask(Idx) >> 1;
    if (!Mask)
      return None;
    return Idx & ~Mask;
  }
  Optional<size_type> toSecondChildIndex(size_type Idx) {
    assert(Idx <= size());
    size_type Mask = lowestZeroMask(Idx);
    Idx |= Mask;
    do {
      Mask >>= 1;
      if (!Mask)
        return None;
      Idx &= ~Mask;
    } while (Idx > size());
    return Idx;
  }
  Optional<size_type> toSiblingIndex(size_type Idx) {
    assert(Idx <= size());
    Idx ^= lowestZeroMask(Idx) << 1;
    if (Idx > size())
      return None;
    return Idx;
  }

  SmallVector<T, 0> Tree{{T()}};
#ifndef NDEBUG
  bool IsFrozen = false;
#endif
};

class BranchTracker {
public:
  using Offset = int32_t;
  using Tree = OffsetTree<Range<Offset>>;
  using size_type = Tree::size_type;

  BranchTracker() {}
  BranchTracker(MachineFunction &MF) { init(MF); }
  ~BranchTracker() {
    assert(Phase == Phase::Invalid && "Forgot to call done()");
  }

  MachineInstr *operator[](size_type BranchIdx) { return Branches[BranchIdx]; }
  size_type size() {
    assert(Branches.size() == BranchTree.size());
    return Branches.size();
  }

  void init(MachineFunction &MF) {
    assert(std::exchange(Phase, Phase::Build) == Phase::Invalid);
#ifndef NDEBUG
    DebugMF = &MF;
#endif
    size_type BranchCountGuess = MF.size() * 2;
    Branches.clear();
    Branches.reserve(BranchCountGuess);
    BranchTree.clear();
    BranchTree.reserve(BranchCountGuess);
    MF.RenumberBlocks();
    TargetTree.clear();
    TargetTree.reserve(MF.getNumBlockIDs());
  }

  void advance(Range<Offset> Amt) {
    assert(Phase == Phase::Build);
    BranchTree.advance(-Amt);
    TargetTree.advance(-Amt);
  }

  void addBranch(MachineInstr &Branch) {
    assert(Phase == Phase::Build);
    assert(Branch.getParent()->getParent() == DebugMF);
    Branches.push_back(&Branch);
    BranchTree.add();
  }

  void addTarget(MachineBasicBlock &Target) {
    assert(Phase == Phase::Build);
    assert(Target.getParent() == DebugMF);
    assert(getTargetIndex(Target) == TargetTree.size());
    (void)Target;
    TargetTree.add();
  }

  Optional<Range<Offset>>
  getBranchDisplacementIfAdded(const MachineInstr &Branch) const {
    assert(Phase == Phase::Build);
    size_type TargetIdx = getBranchTargetIndex(Branch);
    if (TargetIdx >= TargetTree.size())
      return None;
    return TargetTree.get<Tree::Unfrozen>(TargetIdx) -
           TargetTree.current<Tree::Unfrozen>();
  }

  void freeze() {
    assert(std::exchange(Phase, Phase::Query) == Phase::Build);
    BranchTree.freeze();
    TargetTree.freeze();
  }

  Range<Offset> getBranchDisplacement(size_type BranchIdx) const {
    assert(Phase == Phase::Query);
    return TargetTree.get<Tree::Frozen>(
               getBranchTargetIndex(*Branches[BranchIdx])) -
           BranchTree.get<Tree::Frozen>(BranchIdx);
  }

  Range<Offset> getBranchToBranchOffset(size_type FirstBranchIdx,
                                        size_type SecondBranchIdx) {
    assert(Phase == Phase::Query);
    return BranchTree.get<Tree::Frozen>(SecondBranchIdx) -
           BranchTree.get<Tree::Frozen>(FirstBranchIdx);
  }

  void adjustBranchSize(size_type BranchIdx, Range<Offset> Amt) {
    assert(Phase == Phase::Query);
    BranchTree.adjust(BranchIdx + 1, -Amt);
    TargetTree.adjust(
        getTargetIndex(
            *std::exchange(Branches[BranchIdx], nullptr)->getParent()),
        -Amt);
  }

  void done() { assert(std::exchange(Phase, Phase::Invalid) == Phase::Query); }

private:
  size_type getTargetIndex(const MachineBasicBlock &MBB) const {
    assert(MBB.getParent() == DebugMF);
    return MBB.getParent()->getNumBlockIDs() - 1 - MBB.getNumber();
  }
  size_type getBranchTargetIndex(const MachineInstr &Branch) const {
    return getTargetIndex(*Branch.getOperand(0).getMBB());
  }

#ifndef NDEBUG
public:
  Range<Offset> current() const {
    assert(BranchTree.current<Tree::Debug>() ==
           TargetTree.current<Tree::Debug>());
    return BranchTree.current<Tree::Debug>();
  }

  void dump() const {
    for (size_type Idx = TargetTree.size(), Num = 0; Idx--; ++Num)
      dbgs() << TargetTree.get<Tree::Debug>(Idx) -
                    TargetTree.current<Tree::Debug>()
             << '\t' << printMBBReference(*DebugMF->getBlockNumbered(Num))
             << ":\n";
    for (size_type Idx = BranchTree.size(); Idx--;) {
      dbgs() << BranchTree.get<Tree::Debug>(Idx) -
                    BranchTree.current<Tree::Debug>()
             << "\t\t";
      if (Branches[Idx])
        Branches[Idx]->dump();
      else
        dbgs() << "\t(selected)\n";
    }
    dbgs() << '\n';
  }

private:
  enum class Phase {
    Invalid,
    Build,
    Query,
  } Phase = Phase::Invalid;
  // For edge case of dumping with targets but no branches left
  MachineFunction *DebugMF;
#endif

  SmallVector<MachineInstr *, 0> Branches;
  Tree BranchTree, TargetTree;
};

class Z80BranchSelector : public MachineFunctionPass {
public:
  static char ID;
  Z80BranchSelector() : MachineFunctionPass(ID) {}
  MachineFunctionProperties getRequiredProperties() const override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return "Z80 Branch Selector"; }
};

} // end anonymous namespace

MachineFunctionProperties Z80BranchSelector::getRequiredProperties() const {
  return MachineFunctionProperties().set(
      MachineFunctionProperties::Property::NoVRegs);
}

void Z80BranchSelector::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  MachineFunctionPass::getAnalysisUsage(AU);
}

bool Z80BranchSelector::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  auto &STI = MF.getSubtarget<Z80Subtarget>();
  auto &TII = *STI.getInstrInfo();

  // ld[.il] (0).bc
  const BranchTracker::Offset LongestInstructionSize =
      4 + STI.hasEZ80Ops() + STI.is24Bit();
  const Range<BranchTracker::Offset> BranchSize = {2, 3 + STI.is24Bit()};
  const auto makeBranchRelative = [&](MachineInstr &Branch) {
    unsigned BranchOpc = Branch.getOpcode();
    assert(BranchOpc == Z80::JQ || BranchOpc == Z80::JQCC);
    Branch.setDesc(TII.get(BranchOpc == Z80::JQ ? Z80::JR : Z80::JRCC));
    Changed = true;
  };
  const auto makeBranchAbsolute = [&](MachineInstr &Branch) {
    unsigned BranchOpc = Branch.getOpcode();
    assert(BranchOpc == Z80::JQ || BranchOpc == Z80::JQCC);
    Branch.setDesc(TII.get(BranchOpc == Z80::JQ
                               ? STI.is24Bit() ? Z80::JP24 : Z80::JP16
                           : STI.is24Bit() ? Z80::JP24CC
                                           : Z80::JP16CC));
    Changed = true;
  };

  BranchTracker Tracker(MF);

  // Phase 1: Collect all basic blocks and branches in reverse.
  LLVM_DEBUG(dbgs() << MF.getName() << ":\n");
  for (MachineBasicBlock &MBB : reverse(MF)) {
    for (MachineInstr &MI : reverse(MBB)) {
      switch (MI.getOpcode()) {
      case Z80::JQCC:
        if (Z80::CondCode(MI.getOperand(1).getImm()) >
            Z80::CondCode::LAST_SIMPLE_COND) {
          makeBranchAbsolute(MI);
          break;
        }
        LLVM_FALLTHROUGH;
      case Z80::JQ:
        if (!MI.getOperand(0).isMBB()) {
          makeBranchAbsolute(MI);
          break;
        }
        if (auto Displacement = Tracker.getBranchDisplacementIfAdded(MI)) {
          if (isInt<8>(Displacement->Max)) {
            makeBranchRelative(MI);
            break;
          }
          if (!isInt<8>(Displacement->Min)) {
            makeBranchAbsolute(MI);
            break;
          }
        }
        Tracker.addBranch(MI);
        Tracker.advance(BranchSize);
        LLVM_DEBUG(dbgs() << Tracker.current(); MI.dump());
        continue;
      }
      if (MI.isPseudo())
        switch (MI.getOpcode()) {
        case TargetOpcode::INLINEASM:
        case TargetOpcode::INLINEASM_BR: {
          bool BeginningOfLine = true;
          BranchTracker::Offset Lines = 0;
          for (const char *C = MI.getOperand(0).getSymbolName(); *C != '\0';
               ++C)
            switch (*C) {
            default:
              Lines += BeginningOfLine;
              LLVM_FALLTHROUGH;
            case ';':
              BeginningOfLine = false;
              break;
            case '\n':
            case '\r':
              BeginningOfLine = true;
              break;
            case '\f':
            case '\t':
            case '\v':
            case ' ':
              break;
            }
          Tracker.advance(Lines * LongestInstructionSize);
          break;
        }
        default:
          // Assume worst case.
          Tracker.advance(128);
          LLVM_DEBUG(dbgs() << "(Unknown Instruction) ");
          break;
        }
      else
        Tracker.advance(TII.getInstSizeInBytes(MI));
      LLVM_DEBUG(dbgs() << Tracker.current(); MI.dump());
    }
    LLVM_DEBUG(dbgs() << printMBBReference(MBB) << ":\n");
    Tracker.addTarget(MBB);
  }
  Tracker.freeze();
  LLVM_DEBUG(dbgs() << '\n'; Tracker.dump());

  // Phase 2: Select definite relative and required absolute branches.
  SparseSet<BranchTracker::size_type, identity<unsigned>, uint16_t>
      PendingRelaxBranch;
  PendingRelaxBranch.setUniverse(Tracker.size());
  const auto relaxBranch = [&](BranchTracker::size_type BranchIdx,
                               BranchTracker::size_type MaxBranchIdx) {
    makeBranchAbsolute(*Tracker[BranchIdx]);
    Tracker.adjustBranchSize(BranchIdx, {BranchSize.delta(), 0});
    LLVM_DEBUG(dbgs() << ".. Making absolute\n"; Tracker.dump());
    for (auto Idx = BranchIdx;
         ++Idx != MaxBranchIdx &&
         isInt<8>(Tracker.getBranchToBranchOffset(Idx, BranchIdx).Min -
                  BranchSize.delta());)
      if (Tracker[Idx] && !isInt<8>(Tracker.getBranchDisplacement(Idx).Min))
        PendingRelaxBranch.insert(Idx);
    for (auto Idx = BranchIdx;
         Idx && isInt<8>(Tracker.getBranchToBranchOffset(--Idx, BranchIdx).Min -
                         BranchSize.delta());)
      if (Tracker[Idx] && !isInt<8>(Tracker.getBranchDisplacement(Idx).Min))
        PendingRelaxBranch.insert(Idx);
  };
  for (BranchTracker::size_type BranchIdx = 0; BranchIdx != Tracker.size();
       ++BranchIdx) {
    if (!Tracker[BranchIdx])
      continue;
    auto Displacement = Tracker.getBranchDisplacement(BranchIdx);
    LLVM_DEBUG(dbgs() << Displacement; Tracker[BranchIdx]->dump());
    if (isInt<8>(Displacement.Max)) {
      makeBranchRelative(*Tracker[BranchIdx]);
      Tracker.adjustBranchSize(BranchIdx, {0, -BranchSize.delta()});
      LLVM_DEBUG(dbgs() << ".. Making relative\n"; Tracker.dump());
    } else if (!isInt<8>(Displacement.Min)) {
      relaxBranch(BranchIdx, BranchIdx + 1);
      while (!PendingRelaxBranch.empty())
        relaxBranch(PendingRelaxBranch.pop_back_val(), BranchIdx);
    }
  }
  assert(PendingRelaxBranch.empty());

  // Phase 3: All of the remaining branches can now be made relative.
  for (BranchTracker::size_type BranchIdx = 0; BranchIdx != Tracker.size();
       ++BranchIdx)
    if (MachineInstr *Branch = Tracker[BranchIdx])
      makeBranchRelative(*Branch);

  Tracker.done();
  return Changed;
}

char Z80BranchSelector::ID = 0;
INITIALIZE_PASS(Z80BranchSelector, DEBUG_TYPE, "Z80 Branch Selector", false,
                false)

/// createZ80BranchSelectionPass - returns an instance fo the Branch Selection
/// Pass
FunctionPass *llvm::createZ80BranchSelectorPass() {
  return new Z80BranchSelector();
}
