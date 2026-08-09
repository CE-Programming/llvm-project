//===-- Z80.td - Target definition file for the Zilog Z80 --*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a target description file for the Zilog z80 architecture, referred
// to here as the "Z80" architecture.
//
//===----------------------------------------------------------------------===//

// Get the target-independent interfaces which we are implementing...
//
include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// Z80 Subtarget features
//===----------------------------------------------------------------------===//

class Z80Feature<string name, string property, string description,
                 list<SubtargetFeature> implies = []>
    : SubtargetFeature<name, property, "true", description, implies>;

def FeatureUndoc   : Z80Feature<"undoc", "HasUndocOps", "Enable undocumented z80 instructions">;
def FeatureZ180    : Z80Feature<"z180", "HasZ180Ops", "Support z180 instructions">;
def FeatureEZ80    : Z80Feature<"ez80", "HasEZ80Ops", "Support ez80 instructions">;
def FeatureIdxHalf : Z80Feature<"idxhalf", "HasIdxHalfRegs", "Support index half registers">;
def FeatureSli     : Z80Feature<"sli", "HasSliOp", "Support SLI instruction">;

//===----------------------------------------------------------------------===//
// Z80 Subtarget state
//===----------------------------------------------------------------------===//

def Mode24Bit : Z80Feature<"24bit-mode", "In24BitMode", "24-bit mode (ez80)",
                           [FeatureEZ80]>;
def Mode16Bit : Z80Feature<"16bit-mode", "In16BitMode", "16-bit mode (z80)">;

//===----------------------------------------------------------------------===//
// Z80 processors supported
//===----------------------------------------------------------------------===//

let CompleteModel = 0 in def GenericModel : SchedMachineModel;
class Proc<string Name, list<SubtargetFeature> Features>
  : ProcessorModel<Name, GenericModel, Features>;
def : Proc<"generic",     []>;
def : Proc<"z80",         [FeatureUndoc, FeatureIdxHalf, FeatureSli]>;
def : Proc<"z180",        [FeatureZ180]>;
def : Proc<"ez80",        [FeatureZ180, FeatureEZ80, FeatureIdxHalf]>;

//===----------------------------------------------------------------------===//
// Register File Description
//===----------------------------------------------------------------------===//

include "Z80RegisterInfo.td"
include "Z80RegisterBanks.td"

//===----------------------------------------------------------------------===//
// Instruction Descriptions
//===----------------------------------------------------------------------===//

include "Z80InstrInfo.td"
include "Z80Combine.td"

def Z80InstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Calling Conventions
//===----------------------------------------------------------------------===//

include "Z80CallingConv.td"

//===----------------------------------------------------------------------===//
// Assembly writer
//===----------------------------------------------------------------------===//

let ShouldEmitMatchRegisterName = 0 in {
  def Z80AsmParser : AsmParser;
  def EZ80AsmParser : AsmParser;
}
def Z80AsmWriter : AsmWriter;
def EZ80AsmWriter : AsmWriter {
    string AsmWriterClassName = "EInstPrinter";
    let Variant = 1;
}

def Z80 : Target {
  // Information about the instructions...
  let InstructionSet = Z80InstrInfo;
  let AssemblyParsers = [Z80AsmParser, EZ80AsmParser];
  let AssemblyWriters = [Z80AsmWriter, EZ80AsmWriter];
}
