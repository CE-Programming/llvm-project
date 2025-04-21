#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_Z80_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_Z80_H

#include "Gnu.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace toolchains {

class LLVM_LIBRARY_VISIBILITY Z80ToolChain : public Generic_ELF {
public:
  Z80ToolChain(const Driver &D, const llvm::Triple &Triple,
               const llvm::opt::ArgList &Args);
  
  void addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                             llvm::opt::ArgStringList &CC1Args,
                             Action::OffloadKind DeviceOffloadKind) const override;
  
  Tool *buildAssembler() const override;
  Tool *buildLinker() const override;
};

} // namespace toolchains

namespace tools {
namespace z80 {

class LLVM_LIBRARY_VISIBILITY Assembler : public Tool {
public:
  Assembler(const ToolChain &TC) : Tool("Z80::Assembler", "assembler (via GNU as)", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return false; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                   const InputInfo &Output, const InputInfoList &Inputs,
                   const llvm::opt::ArgList &TCArgs,
                   const char *LinkingOutput) const override;
};

class LLVM_LIBRARY_VISIBILITY Linker : public Tool {
public:
  Linker(const ToolChain &TC) : Tool("Z80::Linker", "linker (via GNU ld)", TC) {}

  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }

  void ConstructJob(Compilation &C, const JobAction &JA,
                   const InputInfo &Output, const InputInfoList &Inputs,
                   const llvm::opt::ArgList &TCArgs,
                   const char *LinkingOutput) const override;
};

} // namespace z80
} // namespace tools

} // namespace driver
} // namespace clang

#endif
