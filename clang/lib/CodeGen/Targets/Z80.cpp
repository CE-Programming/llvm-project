//===- Z80.cpp - Z80 ABI Implementation -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"
#include "clang/AST/DeclCXX.h"

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

Address Z80ABIInfo::EmitVAArg(CodeGenFunction &CGF, Address VAListAddr,
                              QualType Ty) const {
  ABIArgInfo AI = classifyArgumentType(Ty);
  removeExtend(AI);
  return EmitVAArgInstr(CGF, VAListAddr, Ty, AI);
}

void Z80TargetCodeGenInfo::setTargetAttributes(const Decl *D,
                                               llvm::GlobalValue *GV,
                                               CodeGen::CodeGenModule &CGM) const {
  const FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(D);
  if (!FD)
    return;
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
  case AnyZ80InterruptAttr::Generic:
    Kind = "Generic";
    break;
  case AnyZ80InterruptAttr::Nested:
    Kind = "Nested";
    break;
  case AnyZ80InterruptAttr::NMI:
    Kind = "NMI";
    break;
  }

  Fn->setCallingConv(llvm::CallingConv::PreserveAll);
  Fn->addFnAttr("interrupt", Kind);
}

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createZ80TargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<Z80TargetCodeGenInfo>(CGM.getTypes());
}
