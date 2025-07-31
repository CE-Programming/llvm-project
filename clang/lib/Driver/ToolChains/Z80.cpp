#include "Z80.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

Z80ToolChain::Z80ToolChain(const Driver &D, const llvm::Triple &Triple,
                           const llvm::opt::ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
}

void Z80ToolChain::addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                                        llvm::opt::ArgStringList &CC1Args,
                                        Action::OffloadKind) const {
  // TODO: Uncomment once GNU binutils is the default
  // CC1Args.push_back("-mllvm");
  // CC1Args.push_back("--z80-gas-style");
}

Tool *Z80ToolChain::buildAssembler() const {
  return new z80::Assembler(*this);
}

Tool *Z80ToolChain::buildLinker() const {
  return new z80::Linker(*this);
}

void tools::z80::Assembler::ConstructJob(Compilation &C, const JobAction &JA,
                                         const InputInfo &Output,
                                         const InputInfoList &Inputs,
                                         const ArgList &Args,
                                         const char *LinkingOutput) const {
  const auto &D = getToolChain().getDriver();

  claimNoWarnArgs(Args);

  ArgStringList CmdArgs;

  // FIXME: This is a workaround for a bug where the codegen emits eZ80 bit
  // operations with explicit output operands. Thankfully, unless you use
  // `.assume ADL = 1`, the assembler will pretty much output Z80 code even with
  // `-march=ez80`.
  CmdArgs.push_back("-march=ez80");

  llvm::Reloc::Model RelocationModel;
  unsigned PICLevel;
  bool IsPIE;
  const char *DefaultAssembler = "as";
  std::tie(RelocationModel, PICLevel, IsPIE) =
      ParsePICArgs(getToolChain(), Args);

  if (const Arg *A = Args.getLastArg(options::OPT_gz, options::OPT_gz_EQ)) {
    if (A->getOption().getID() == options::OPT_gz) {
      CmdArgs.push_back("--compress-debug-sections");
    } else {
      StringRef Value = A->getValue();
      if (Value == "none" || Value == "zlib") {
        CmdArgs.push_back(
            Args.MakeArgString("--compress-debug-sections=" + Twine(Value)));
      } else {
        D.Diag(diag::err_drv_unsupported_option_argument)
            << A->getOption().getName() << Value;
      }
    }
  }

  for (const Arg *A : Args.filtered(options::OPT_ffile_prefix_map_EQ,
                                    options::OPT_fdebug_prefix_map_EQ)) {
    StringRef Map = A->getValue();
    if (!Map.contains('='))
      D.Diag(diag::err_drv_invalid_argument_to_option)
          << Map << A->getOption().getName();
    else {
      CmdArgs.push_back(Args.MakeArgString("--debug-prefix-map"));
      CmdArgs.push_back(Args.MakeArgString(Map));
    }
    A->claim();
  }

  Args.AddAllArgs(CmdArgs, options::OPT_I);
  Args.AddAllArgValues(CmdArgs, options::OPT_Wa_COMMA, options::OPT_Xassembler);

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  for (const auto &II : Inputs)
    CmdArgs.push_back(II.getFilename());

  const char *Exec =
      Args.MakeArgString(getToolChain().GetProgramPath(DefaultAssembler));
  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));

  // Handle the debug info splitting at object creation time if we're
  // creating an object.
  // TODO: Currently only works on linux with newer objcopy.
  if (Args.hasArg(options::OPT_gsplit_dwarf) &&
      getToolChain().getTriple().isOSLinux())
    SplitDebugInfo(getToolChain(), C, *this, JA, Args, Output,
                   SplitDebugName(JA, Args, Inputs[0], Output));
}

static bool getPIE(const ArgList &Args, const ToolChain &TC) {
  if (Args.hasArg(options::OPT_shared) || Args.hasArg(options::OPT_static) ||
      Args.hasArg(options::OPT_r) || Args.hasArg(options::OPT_static_pie))
    return false;

  Arg *A = Args.getLastArg(options::OPT_pie, options::OPT_no_pie,
                           options::OPT_nopie);
  if (!A)
    return TC.isPIEDefault(Args);
  return A->getOption().matches(options::OPT_pie);
}

static bool getStaticPIE(const ArgList &Args, const ToolChain &TC) {
  bool HasStaticPIE = Args.hasArg(options::OPT_static_pie);
  // -no-pie is an alias for -nopie. So, handling -nopie takes care of
  // -no-pie as well.
  if (HasStaticPIE && Args.hasArg(options::OPT_nopie)) {
    const Driver &D = TC.getDriver();
    const llvm::opt::OptTable &Opts = D.getOpts();
    const char *StaticPIEName = Opts.getOptionName(options::OPT_static_pie);
    const char *NoPIEName = Opts.getOptionName(options::OPT_nopie);
    D.Diag(diag::err_drv_cannot_mix_options) << StaticPIEName << NoPIEName;
  }
  return HasStaticPIE;
}

static bool getStatic(const ArgList &Args) {
  return Args.hasArg(options::OPT_static) &&
      !Args.hasArg(options::OPT_static_pie);
}

void tools::z80::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                      const InputInfo &Output,
                                      const InputInfoList &Inputs,
                                      const ArgList &Args,
                                      const char *LinkingOutput) const {
  // FIXME: The Linker class constructor takes a ToolChain and not a
  // Generic_ELF, so the static_cast might return a reference to a invalid
  // instance (see PR45061). Ideally, the Linker constructor needs to take a
  // Generic_ELF instead.
  const toolchains::Generic_ELF &ToolChain =
      static_cast<const toolchains::Generic_ELF &>(getToolChain());
  const Driver &D = ToolChain.getDriver();

  const llvm::Triple &Triple = getToolChain().getEffectiveTriple();

  const llvm::Triple::ArchType Arch = ToolChain.getArch();
  const bool IsPIE = getPIE(Args, ToolChain);
  const bool IsStaticPIE = getStaticPIE(Args, ToolChain);
  const bool IsStatic = getStatic(Args);
  const bool HasCRTBeginEndFiles =
      ToolChain.getTriple().hasEnvironment() ||
      (ToolChain.getTriple().getVendor() != llvm::Triple::MipsTechnologies);

  ArgStringList CmdArgs;

  // Silence warning for "clang -g foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_g_Group);
  // and "clang -emit-llvm foo.o -o foo"
  Args.ClaimAllArgs(options::OPT_emit_llvm);
  // and for "clang -w foo.o -o foo". Other warning options are already
  // handled somewhere else.
  Args.ClaimAllArgs(options::OPT_w);

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));

  if (IsPIE)
    CmdArgs.push_back("-pie");

  if (IsStaticPIE) {
    CmdArgs.push_back("-static");
    CmdArgs.push_back("-pie");
    CmdArgs.push_back("--no-dynamic-linker");
    CmdArgs.push_back("-z");
    CmdArgs.push_back("text");
  }

  if (Args.hasArg(options::OPT_rdynamic))
    CmdArgs.push_back("-export-dynamic");

  if (Args.hasArg(options::OPT_s))
    CmdArgs.push_back("-s");

  ToolChain.addExtraOpts(CmdArgs);

  // NOTE: This causes linker error for Z80 targets
  // CmdArgs.push_back("--eh-frame-hdr");

  if (Args.hasArg(options::OPT_shared))
    CmdArgs.push_back("-shared");

  if (IsStatic) {
    CmdArgs.push_back("-static");
  } else {
    if (Args.hasArg(options::OPT_rdynamic))
      CmdArgs.push_back("-export-dynamic");

    if (!Args.hasArg(options::OPT_shared) && !IsStaticPIE &&
        !Args.hasArg(options::OPT_r)) {
      CmdArgs.push_back("-dynamic-linker");
      CmdArgs.push_back(Args.MakeArgString(Twine(D.DyldPrefix) +
                                           ToolChain.getDynamicLinker(Args)));
    }
  }

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles,
                   options::OPT_r)) {
    const char *crt1 = nullptr;
    if (!Args.hasArg(options::OPT_shared)) {
      if (Args.hasArg(options::OPT_pg))
        crt1 = "gcrt1.o";
      else if (IsPIE)
        crt1 = "Scrt1.o";
      else if (IsStaticPIE)
        crt1 = "rcrt1.o";
      else
        crt1 = "crt1.o";
    }
    if (crt1)
      CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath(crt1)));

    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crti.o")));

    if (HasCRTBeginEndFiles) {
      std::string P;
      if (ToolChain.GetRuntimeLibType(Args) == ToolChain::RLT_CompilerRT) {
        std::string crtbegin = ToolChain.getCompilerRT(Args, "crtbegin",
                                                       ToolChain::FT_Object);
        if (ToolChain.getVFS().exists(crtbegin))
          P = crtbegin;
      }
      if (P.empty()) {
        const char *crtbegin;
        if (Args.hasArg(options::OPT_shared))
          crtbegin = "crtbeginS.o";
        else if (IsStatic)
          crtbegin = "crtbeginT.o";
        else if (IsPIE || IsStaticPIE)
          crtbegin = "crtbeginS.o";
        else
          crtbegin = "crtbegin.o";
        P = ToolChain.GetFilePath(crtbegin);
      }
      CmdArgs.push_back(Args.MakeArgString(P));
    }

    // Add crtfastmath.o if available and fast math is enabled.
    ToolChain.addFastMathRuntimeIfAvailable(Args, CmdArgs);
  }

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  Args.AddAllArgs(CmdArgs, options::OPT_u);

  ToolChain.AddFilePathLibArgs(Args, CmdArgs);

  if (D.isUsingLTO()) {
    assert(!Inputs.empty() && "Must have at least one input.");
    addLTOOptions(ToolChain, Args, CmdArgs, Output, Inputs[0],
                  D.getLTOMode() == LTOK_Thin);
  }

  if (Args.hasArg(options::OPT_Z_Xlinker__no_demangle))
    CmdArgs.push_back("--no-demangle");

  bool NeedsSanitizerDeps = addSanitizerRuntimes(ToolChain, Args, CmdArgs);
  bool NeedsXRayDeps = addXRayRuntime(ToolChain, Args, CmdArgs);
  addLinkerCompressDebugSectionsOption(ToolChain, Args, CmdArgs);
  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  addHIPRuntimeLibArgs(ToolChain, Args, CmdArgs);

  // The profile runtime also needs access to system libraries.
  getToolChain().addProfileRTLibs(Args, CmdArgs);

  if (D.CCCIsCXX() &&
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nodefaultlibs,
                   options::OPT_r)) {
    if (ToolChain.ShouldLinkCXXStdlib(Args)) {
      bool OnlyLibstdcxxStatic = Args.hasArg(options::OPT_static_libstdcxx) &&
                                 !Args.hasArg(options::OPT_static);
      if (OnlyLibstdcxxStatic)
        CmdArgs.push_back("-Bstatic");
      ToolChain.AddCXXStdlibLibArgs(Args, CmdArgs);
      if (OnlyLibstdcxxStatic)
        CmdArgs.push_back("-Bdynamic");
    }
    CmdArgs.push_back("-lm");
  }

  // If we are linking for the device all symbols should be bound locally. The
  // symbols are already protected which makes this redundant. This is only
  // necessary to work around a problem in bfd.
  // TODO: Remove this once 'lld' becomes the only linker for offloading.
  if (JA.isDeviceOffloading(Action::OFK_OpenMP))
    CmdArgs.push_back("-Bsymbolic");

  // Silence warnings when linking C code with a C++ '-stdlib' argument.
  Args.ClaimAllArgs(options::OPT_stdlib_EQ);

  // Additional linker set-up and flags for Fortran. This is required in order
  // to generate executables. As Fortran runtime depends on the C runtime,
  // these dependencies need to be listed before the C runtime below (i.e.
  // AddRuntTimeLibs).
  if (D.IsFlangMode()) {
    addFortranRuntimeLibraryPath(ToolChain, Args, CmdArgs);
    addFortranRuntimeLibs(ToolChain, CmdArgs);
    CmdArgs.push_back("-lm");
  }

  if (!Args.hasArg(options::OPT_nostdlib, options::OPT_r)) {
    if (!Args.hasArg(options::OPT_nodefaultlibs)) {
      if (IsStatic || IsStaticPIE)
        CmdArgs.push_back("--start-group");

      if (NeedsSanitizerDeps)
        linkSanitizerRuntimeDeps(ToolChain, CmdArgs);

      if (NeedsXRayDeps)
        linkXRayRuntimeDeps(ToolChain, CmdArgs);

      bool WantPthread = Args.hasArg(options::OPT_pthread) ||
                         Args.hasArg(options::OPT_pthreads);

      // Use the static OpenMP runtime with -static-openmp
      bool StaticOpenMP = Args.hasArg(options::OPT_static_openmp) &&
                          !Args.hasArg(options::OPT_static);

      // FIXME: Only pass GompNeedsRT = true for platforms with libgomp that
      // require librt. Most modern Linux platforms do, but some may not.
      if (addOpenMPRuntime(CmdArgs, ToolChain, Args, StaticOpenMP,
                           JA.isHostOffloading(Action::OFK_OpenMP),
                           /* GompNeedsRT= */ true))
        // OpenMP runtimes implies pthreads when using the GNU toolchain.
        // FIXME: Does this really make sense for all GNU toolchains?
        WantPthread = true;

      AddRunTimeLibs(ToolChain, D, CmdArgs, Args);

      if (WantPthread)
        CmdArgs.push_back("-lpthread");

      if (Args.hasArg(options::OPT_fsplit_stack))
        CmdArgs.push_back("--wrap=pthread_create");

      if (!Args.hasArg(options::OPT_nolibc))
        CmdArgs.push_back("-lc");

      if (IsStatic || IsStaticPIE)
        CmdArgs.push_back("--end-group");
      else
        AddRunTimeLibs(ToolChain, D, CmdArgs, Args);
    }

    if (!Args.hasArg(options::OPT_nostartfiles)) {
      if (HasCRTBeginEndFiles) {
        std::string P;
        if (ToolChain.GetRuntimeLibType(Args) == ToolChain::RLT_CompilerRT) {
          std::string crtend = ToolChain.getCompilerRT(Args, "crtend",
                                                       ToolChain::FT_Object);
          if (ToolChain.getVFS().exists(crtend))
            P = crtend;
        }
        if (P.empty()) {
          const char *crtend;
          if (Args.hasArg(options::OPT_shared))
            crtend = "crtendS.o";
          else if (IsPIE || IsStaticPIE)
            crtend = "crtendS.o";
          else
            crtend = "crtend.o";
          P = ToolChain.GetFilePath(crtend);
        }
        CmdArgs.push_back(Args.MakeArgString(P));
      }
      CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtn.o")));
    }
  }

  Args.AddAllArgs(CmdArgs, options::OPT_T);

  const char *Exec = Args.MakeArgString(ToolChain.GetLinkerPath());
  C.addCommand(std::make_unique<Command>(JA, *this,
                                         ResponseFileSupport::AtFileCurCP(),
                                         Exec, CmdArgs, Inputs, Output));
}
