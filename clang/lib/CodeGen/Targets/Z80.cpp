#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// Z80 ABI Implementation
//===----------------------------------------------------------------------===//

namespace {
class Z80ABIInfo : public DefaultABIInfo {
public:
  Z80ABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

private:
  void removeExtend(ABIArgInfo &AI) const {
    if (AI.isExtend()) {
      bool InReg = AI.getInReg();
      AI = ABIArgInfo::getDirect(AI.getCoerceToType());
      AI.setInReg(InReg);
    }
  }
  // DefaultABIInfo's classifyReturnType and classifyArgumentType are
  // non-virtual, but computeInfo and EmitVAArg are virtual, so we
  // overload them.
  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    removeExtend(FI.getReturnInfo());
    for (auto &Arg : FI.arguments())
      removeExtend(Arg.info = classifyArgumentType(Arg.type));
  }

  Address EmitVAArg(CodeGenFunction &CGF, Address VAListAddr,
                    QualType Ty) const override;
};

class Z80TargetCodeGenInfo : public TargetCodeGenInfo {
public:
  Z80TargetCodeGenInfo(CodeGen::CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<Z80ABIInfo>(CGT)) {}
  void setTargetAttributes(const Decl *D, llvm::GlobalValue *GV,
                           CodeGen::CodeGenModule &CGM) const override;
};
} // namespace

Address Z80ABIInfo::EmitVAArg(CodeGenFunction &CGF,
                              Address VAListAddr, QualType Ty) const {
  Address Addr = emitVoidPtrVAArg(
      CGF, VAListAddr, Ty, /*Indirect*/ false,
      getContext().getTypeInfoInChars(Ty),
      CharUnits::fromQuantity(getDataLayout().getPointerSize()),
      /*AllowHigherAlign*/ false);
  // Remove SlotSize over-alignment, since stack is never aligned.
  return Address(Addr.getPointer(), CharUnits::fromQuantity(1));
}

void Z80TargetCodeGenInfo::setTargetAttributes(
    const Decl *D, llvm::GlobalValue *GV, CodeGen::CodeGenModule &CGM) const {
  const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(D);
  if (!FD) return;
  llvm::Function *Fn = cast<llvm::Function>(GV);

  if (Fn->isDeclaration()) {
    if (FD->getAttr<AnyZ80TIFlagsAttr>())
      Fn->setCallingConv(llvm::CallingConv::Z80_TIFlags);
    return;
  }

  const AnyZ80InterruptAttr *Attr = FD->getAttr<AnyZ80InterruptAttr>();
  if (!Attr)
    return;

  const char *Kind;
  switch (Attr->getInterrupt()) {
  case AnyZ80InterruptAttr::Generic: Kind = "Generic"; break;
  case AnyZ80InterruptAttr::Nested:  Kind = "Nested"; break;
  case AnyZ80InterruptAttr::NMI:     Kind = "NMI"; break;
  }

  Fn->setCallingConv(llvm::CallingConv::PreserveAll);
  Fn->addFnAttr("interrupt", Kind);
}

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createZ80TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<Z80TargetCodeGenInfo>(CGM.getTypes());
}

