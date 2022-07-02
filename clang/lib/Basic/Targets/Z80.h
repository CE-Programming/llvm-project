//===--- Z80.h - Declare Z80 target feature support -------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file declares Z80 TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_Z80_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_Z80_H

#include "Targets.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/Support/Compiler.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY Z80TargetInfoBase : public TargetInfo {
public:
  Z80TargetInfoBase(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    TLSSupported = false;
    PointerAlign = BoolAlign = ShortAlign = IntAlign = HalfAlign = FloatAlign =
        DoubleAlign = LongDoubleAlign = LongAlign = LongLongAlign =
            SuitableAlign = MinGlobalAlign = 8;
    DoubleWidth = 32;
    DoubleFormat = &llvm::APFloat::IEEEsingle();
    DefaultAlignForAttributeAligned = 32;
    SizeType = UnsignedInt;
    WCharType = SignedShort;
    PtrDiffType = IntPtrType = WIntType = SignedInt;
    Char32Type = UnsignedLong;
  }

  bool hasInt48Type() const override { return true; }
  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::CharPtrBuiltinVaList;
  }

  StringRef getConstraintRegister(StringRef Constraint,
                                  StringRef Expression) const override;
  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override;
  std::string convertConstraint(const char *&Constraint) const override;

  const char *getClobbers() const override { return ""; }
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    return None;
  }
  ArrayRef<TargetInfo::AddlRegName> getGCCAddlRegNames() const override;

  bool allowsLargerPreferedTypeAlignment() const override { return false; }
};

class LLVM_LIBRARY_VISIBILITY Z80TargetInfo : public Z80TargetInfoBase {
public:
  explicit Z80TargetInfo(const llvm::Triple &T, const TargetOptions &Opts)
      : Z80TargetInfoBase(T, Opts) {
    PointerWidth = IntWidth = 16;
    resetDataLayout("e-m:z-p:16:8-p2:8:8-p3:16:8-i16:8-i24:8-i32:8-i48:8-i64:8-i96:8-"
                    "f32:8-f64:8-a:8-n8:16-S8", "_");
  }

private:
  bool setCPU(const std::string &Name) override;
  bool
  initFeatureMap(llvm::StringMap<bool> &Features, DiagnosticsEngine &Diags,
                 StringRef CPU,
                 const std::vector<std::string> &FeaturesVec) const override;
  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
  ArrayRef<Builtin::Info> getTargetBuiltins() const override;
  ArrayRef<const char *> getGCCRegNames() const override;
};

class LLVM_LIBRARY_VISIBILITY EZ80TargetInfo : public Z80TargetInfoBase {
public:
  explicit EZ80TargetInfo(const llvm::Triple &T, const TargetOptions &Opts)
      : Z80TargetInfoBase(T, Opts) {
    if (T.getEnvironment() == llvm::Triple::CODE16) {
      PointerWidth = IntWidth = 16;
      resetDataLayout("e-m:z-p:16:8-p1:24:8-p2:8:8-p3:16:8-p4:24:8-i16:8-i24:8-i32:8-i48:8-"
                      "i64:8-i96:8-f32:8-f64:8-a:8-n8:16-S8", "_");
    } else {
      PointerWidth = IntWidth = 24;
      resetDataLayout("e-m:z-p:24:8-p1:16:8-p2:8:8-p3:16:8-p4:24:8-i16:8-i24:8-i32:8-i48:8-"
                      "i64:8-i96:8-f32:8-f64:8-a:8-n8:16:24-S8", "_");
    }
  }

private:
  bool setCPU(const std::string &Name) override;
  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
  ArrayRef<Builtin::Info> getTargetBuiltins() const override;
  ArrayRef<const char *> getGCCRegNames() const override;
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_Z80_H
