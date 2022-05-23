//===-- Z80InstrInfo.td - Main Z80 Instruction Definition --*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes the Z80 instruction set, defining the instructions, and
// properties of the instructions which are needed for code generation, machine
// code emission, and analysis.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Type Constraints.
//===----------------------------------------------------------------------===//

class SDTCisChain<int OpNum> : SDTCisVT<OpNum, OtherVT>;
class SDTCisI8   <int OpNum> : SDTCisVT<OpNum, i8>;
class SDTCisFlag <int OpNum> : SDTCisI8<OpNum>;
class SDTCisI16  <int OpNum> : SDTCisVT<OpNum, i16>;
class SDTCisI24  <int OpNum> : SDTCisVT<OpNum, i24>;
class SDTCisPtr  <int OpNum> : SDTCisVT<OpNum, iPTR>;

def p0 : LLT;

//===----------------------------------------------------------------------===//
// Type Profiles.
//===----------------------------------------------------------------------===//

def SDTUnOpRF   : SDTypeProfile<2, 1, [SDTCisInt<0>,
                                       SDTCisFlag<1>,
                                       SDTCisSameAs<2, 0>]>;
def SDTUnOpRFF  : SDTypeProfile<2, 2, [SDTCisInt<0>,
                                       SDTCisFlag<1>,
                                       SDTCisSameAs<2, 0>,
                                       SDTCisFlag<3>]>;
def SDTBinOpRF  : SDTypeProfile<2, 2, [SDTCisInt<0>,
                                       SDTCisFlag<1>,
                                       SDTCisSameAs<2, 0>,
                                       SDTCisSameAs<3, 0>]>;
def SDTBinOpRFF : SDTypeProfile<2, 3, [SDTCisInt<0>,
                                       SDTCisFlag<1>,
                                       SDTCisSameAs<2, 0>,
                                       SDTCisSameAs<3, 0>,
                                       SDTCisFlag<4>]>;
def SDTBinOpF   : SDTypeProfile<1, 2, [SDTCisFlag<0>,
                                       SDTCisInt<1>,
                                       SDTCisSameAs<2, 1>]>;
def SDTBitOpF   : SDTypeProfile<1, 2, [SDTCisFlag<0>,
                                       SDTCisI8<1>,
                                       SDTCisI8<2>]>;
def SDTBitOpR   : SDTypeProfile<1, 2, [SDTCisI8<0>,
                                       SDTCisI8<1>,
                                       SDTCisI8<2>]>;

def SDTZ80Wrapper       : SDTypeProfile<1, 1, [SDTCisPtrTy<0>,
                                               SDTCisSameAs<1, 0>]>;
def SDT_Z80mlt          : SDTypeProfile<1, 1, [SDTCisI16<0>, SDTCisI16<1>]>;
def SDT_Z80sext         : SDTypeProfile<1, 1, [SDTCisInt<0>, SDTCisFlag<1>]>;
def SDT_Z80TCRet        : SDTypeProfile<0, 1, [SDTCisPtrTy<0>]>;
def SDT_Z80Call         : SDTypeProfile<0, -1, [SDTCisPtr<0>]>;
def SDT_Z80BrCond       : SDTypeProfile<0, 3, [SDTCisChain<0>,
                                               SDTCisI8<1>,
                                               SDTCisFlag<2>]>;
def SDT_Z80Select       : SDTypeProfile<1, 4, [SDTCisInt<0>,
                                               SDTCisSameAs<1, 0>,
                                               SDTCisSameAs<2, 0>,
                                               SDTCisI8<3>,
                                               SDTCisI8<4>]>;
def SDT_Z80Pop          : SDTypeProfile<1, 0, [SDTCisPtr<0>]>;
def SDT_Z80Push         : SDTypeProfile<0, 1, [SDTCisPtr<0>]>;

//===----------------------------------------------------------------------===//
// Z80 specific DAG Nodes.
//===----------------------------------------------------------------------===//

def Z80Wrapper       : SDNode<"Z80ISD::Wrapper", SDTZ80Wrapper>;
def Z80rlc_flag      : SDNode<"Z80ISD::RLC",     SDTUnOpRF>;
def Z80rrc_flag      : SDNode<"Z80ISD::RRC",     SDTUnOpRF>;
def Z80rl_flag       : SDNode<"Z80ISD::RL",      SDTUnOpRFF>;
def Z80rr_flag       : SDNode<"Z80ISD::RR",      SDTUnOpRFF>;
def Z80sla_flag      : SDNode<"Z80ISD::SLA",     SDTUnOpRF>;
def Z80sra_flag      : SDNode<"Z80ISD::SRA",     SDTUnOpRF>;
def Z80sli_flag      : SDNode<"Z80ISD::SLI",     SDTUnOpRF>;
def Z80srl_flag      : SDNode<"Z80ISD::SRL",     SDTUnOpRF>;
def Z80bit_flag      : SDNode<"Z80ISD::BIT",     SDTBitOpF>;
def Z80res_flag      : SDNode<"Z80ISD::RES",     SDTBitOpR>;
def Z80set_flag      : SDNode<"Z80ISD::SET",     SDTBitOpR>;
def Z80inc_flag      : SDNode<"Z80ISD::INC",     SDTUnOpRF>;
def Z80dec_flag      : SDNode<"Z80ISD::DEC",     SDTUnOpRF>;
def Z80add_flag      : SDNode<"Z80ISD::ADD",     SDTBinOpRF, [SDNPCommutative]>;
def Z80adc_flag      : SDNode<"Z80ISD::ADC",     SDTBinOpRFF>;
def Z80sub_flag      : SDNode<"Z80ISD::SUB",     SDTBinOpRF>;
def Z80sbc_flag      : SDNode<"Z80ISD::SBC",     SDTBinOpRFF>;
def Z80and_flag      : SDNode<"Z80ISD::AND",     SDTBinOpRF, [SDNPCommutative]>;
def Z80xor_flag      : SDNode<"Z80ISD::XOR",     SDTBinOpRF, [SDNPCommutative]>;
def Z80or_flag       : SDNode<"Z80ISD::OR",      SDTBinOpRF, [SDNPCommutative]>;
def Z80cp_flag       : SDNode<"Z80ISD::CP",      SDTBinOpF>;
def Z80tst_flag      : SDNode<"Z80ISD::TST",     SDTBinOpF,  [SDNPCommutative]>;
def Z80mlt           : SDNode<"Z80ISD::MLT",     SDT_Z80mlt>;
def Z80sext          : SDNode<"Z80ISD::SEXT",    SDT_Z80sext>;
def Z80retflag       : SDNode<"Z80ISD::RET_FLAG", SDTNone,
                              [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def Z80retnflag      : SDNode<"Z80ISD::RETN_FLAG", SDTNone,
                              [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def Z80retiflag      : SDNode<"Z80ISD::RETI_FLAG", SDTNone,
                              [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def Z80tcret         : SDNode<"Z80ISD::TC_RETURN", SDT_Z80TCRet,
                              [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def Z80call          : SDNode<"Z80ISD::CALL", SDT_Z80Call,
                              [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                               SDNPVariadic]>;
def Z80brcond        : SDNode<"Z80ISD::BRCOND", SDT_Z80BrCond, [SDNPHasChain]>;
def Z80select        : SDNode<"Z80ISD::SELECT", SDT_Z80Select>;
def Z80pop           : SDNode<"Z80ISD::POP", SDT_Z80Pop,
                              [SDNPHasChain, SDNPMayLoad]>;
def Z80push          : SDNode<"Z80ISD::PUSH", SDT_Z80Push,
                              [SDNPHasChain, SDNPMayStore]>;

//===----------------------------------------------------------------------===//
// Z80 Instruction Predicate Definitions.
//===----------------------------------------------------------------------===//

def In16BitMode  : Predicate<"Subtarget->is16Bit()">,
                   AssemblerPredicate<(all_of Mode16Bit), "16-bit mode">;
def In24BitMode  : Predicate<"Subtarget->is24Bit()">,
                   AssemblerPredicate<(all_of Mode24Bit), "24-bit mode">;
def HaveUndocOps : Predicate<"Subtarget->hasUndocOps()">,
                   AssemblerPredicate<(all_of FeatureUndoc),
                                      "undocumented ops">;
def HaveZ180Ops  : Predicate<"Subtarget->hasZ180Ops()">,
                   AssemblerPredicate<(all_of FeatureZ180), "Z180 ops">;
def HaveEZ80Ops  : Predicate<"Subtarget->hasEZ80Ops()">,
                   AssemblerPredicate<(all_of FeatureEZ80), "eZ80 ops">;
def HaveIdxHalf  : Predicate<"Subtarget->hasIndexHalfRegs()">,
                   AssemblerPredicate<(all_of FeatureIdxHalf),
                                      "index half regs">;
def HaveSliOp    : Predicate<"Subtarget->hasSliOp()">,
                   AssemblerPredicate<(all_of FeatureSli), "SLI op">;

//===----------------------------------------------------------------------===//
// Z80 Instruction Format Definitions.
//===----------------------------------------------------------------------===//

include "Z80InstrFormats.td"

//===----------------------------------------------------------------------===//
// Pattern fragments.
//===----------------------------------------------------------------------===//

// Z80 specific condition code. These correspond to CondCode in
// Z80InstrInfo.h. They must be kept in synch.
def Z80_COND_NZ : PatLeaf<(i8 0)>;
def Z80_COND_Z  : PatLeaf<(i8 1)>;
def Z80_COND_NC : PatLeaf<(i8 2)>;
def Z80_COND_C  : PatLeaf<(i8 3)>;
def Z80_COND_PO : PatLeaf<(i8 4)>;
def Z80_COND_PE : PatLeaf<(i8 5)>;
def Z80_COND_P  : PatLeaf<(i8 6)>;
def Z80_COND_M  : PatLeaf<(i8 7)>;

//===----------------------------------------------------------------------===//
// Z80 Operand Definitions.
//===----------------------------------------------------------------------===//

def gptr_rc : PointerLikeRegClass<1>;
def optr_rc : PointerLikeRegClass<2>;
def aptr_rc : PointerLikeRegClass<3>;
def iptr_rc : PointerLikeRegClass<4>;

let OperandType = "OPERAND_IMMEDIATE" in {
  def bitimm : Operand<i8>, PatLeaf<(i8 imm), [{
    return isUInt<3>(N->getZExtValue());
  }]>;
  def rsttarget : Operand<i8>, PatLeaf<(i8 imm), [{
    return N->getZExtValue() == (N->getZExtValue() & 070);
  }]>;
  def intmode : Operand<i8>, PatLeaf<(i8 imm), [{
    return N->getZExtValue() <= 2;
  }]>;
  def i24imm : Operand<i24>;
}

let OperandType = "OPERAND_MEMORY" in {
  let PrintMethod = "printIndirect" in {
    def mem : Operand<iPTR> {
      let MIOperandInfo = (ops imm);
    }
    def gptr: Operand<iPTR> {
      let MIOperandInfo = (ops gptr_rc);
    }
    def optr: Operand<iPTR> {
      let MIOperandInfo = (ops optr_rc);
    }
    def aptr: Operand<iPTR> {
      let MIOperandInfo = (ops aptr_rc);
    }
    def port : Operand<i8>;
  }
  let PrintMethod = "printIndirectOffset" in {
    def off : Operand<iPTR> {
      let MIOperandInfo = (ops iptr_rc, i8imm);
    }
  }
}

let PrintMethod = "printOffset" in {
  def off16 : Operand<i16> {
    let MIOperandInfo = (ops I16, i8imm);
  }
  def off24 : Operand<i24> {
    let MIOperandInfo = (ops I24, i8imm);
  }
}

def jmptarget : Operand<OtherVT>;
let OperandType = "OPERAND_PCREL" in
def jmptargetoff : Operand<OtherVT>;

def cc : Operand<i8> {
  let PrintMethod = "printCondCode";
}

//===----------------------------------------------------------------------===//
// Z80 Complex Pattern Definitions.
//===----------------------------------------------------------------------===//

def mempat : ComplexPattern<iPTR, 1, "SelectMem",
                            [imm, globaladdr, externalsym]>;
def offpat : ComplexPattern<iPTR, 2, "SelectOff",
                            [add, sub, frameindex]>;

def gi_mempat :
  GIComplexOperandMatcher<p0, "selectMem">,
  GIComplexPatternEquiv<mempat>;
def gi_offpat :
  GIComplexOperandMatcher<p0, "selectOff">,
  GIComplexPatternEquiv<offpat>;

//===----------------------------------------------------------------------===//
// Instruction list.
//===----------------------------------------------------------------------===//

let hasPostISelHook = true in {
  let Defs = [SPS], Uses = [SPS] in {
  def ADJCALLSTACKDOWN16 : P<(outs),
                             (ins i16imm:$amt1, i16imm:$amt2, i16imm:$amt3)>,
                           Requires<[In16BitMode]>;
  def ADJCALLSTACKUP16   : P<(outs), (ins i16imm:$amt1, i16imm:$amt2)>,
                           Requires<[In16BitMode]>;
  }
  let Defs = [SPL], Uses = [SPL] in {
  def ADJCALLSTACKDOWN24 : P<(outs),
                             (ins i24imm:$amt1, i24imm:$amt2, i24imm:$amt3)>,
                           Requires<[In24BitMode]>;
  def ADJCALLSTACKUP24   : P<(outs), (ins i24imm:$amt1, i24imm:$amt2)>,
                           Requires<[In24BitMode]>;
  }
}
let hasSideEffects = false, usesCustomInserter = true in {
  let Uses = [F] in {
    def SetCC : P<(outs R8:$dst), (ins i8imm:$cc)>;
    def Select8  : P<(outs  R8:$dst), (ins  R8:$true,  R8:$false, i8imm:$cc),
                     [(set  R8:$dst, (Z80select  R8:$true,  R8:$false, imm:$cc,
                                      F))]>;
    def Select16 : P<(outs R16:$dst), (ins R16:$true, R16:$false, i8imm:$cc),
                     [(set R16:$dst, (Z80select R16:$true, R16:$false, imm:$cc,
                                      F))]>;
    def Select24 : P<(outs R24:$dst), (ins R24:$true, R24:$false, i8imm:$cc),
                     [(set R24:$dst, (Z80select R24:$true, R24:$false, imm:$cc,
                                      F))]>,
                     Requires<[In24BitMode]>;
    let Defs = [A,   F] in
    def SExt8  : P<(outs), (ins), [(set A,   (Z80sext F))]>;
    let Defs = [HL,  F] in
    def SExt16 : P<(outs), (ins), [(set HL,  (Z80sext F))]>;
    let Defs = [UHL, F] in
    def SExt24 : P<(outs), (ins), [(set UHL, (Z80sext F))]>;
  }
}

let hasSideEffects = false in
def NOP   : I<NoPre, 0x00, "nop">;
def HALT  : I<NoPre, 0x76, "halt">;
def DI    : I<NoPre, 0xF3, "di">;
def EI    : I<NoPre, 0xFB, "ei">;
def IM    : I<EDPre, 0x46, "im", "\t$mode", "", (outs), (ins intmode:$mode)>;
def SLP   : I<EDPre, 0x76, "slp">, Requires<[HaveZ180Ops]>;
def STMIX : I<EDPre, 0x7D, "stmix">, Requires<[HaveEZ80Ops]>;
def RSMIX : I<EDPre, 0x7E, "rsmix">, Requires<[HaveEZ80Ops]>;

//===----------------------------------------------------------------------===//
//  Control Flow Instructions.
//===----------------------------------------------------------------------===//

// All calls clobber the non-callee saved registers.  SP is marked as a use to
// prevent stack-pointer assignments that appear immediately before calls from
// potentially appearing dead.  Uses for argument registers are added manually.
let isCall = true in {
  let Uses = [SPS] in {
    def CALL16   : I16i<NoPre, 0xCD, "call", "\t$tgt", "",
                        (outs), (ins i16imm:$tgt), [(Z80call mempat:$tgt)]>,
                   Requires<[In16BitMode]>;
    def CALL16r  : P   <(outs), (ins    A16:$tgt), [(Z80call    A16:$tgt)]>,
                   Requires<[In16BitMode]>;
    def RST16    : I16 <NoPre, 0xC7, "rst", "\t$tgt", "",
                        (outs), (ins rsttarget:$tgt)>;
  }
  let Uses = [F, SPS] in
    def CALL16CC : I16i<NoPre, 0xC4, "call", "\t$cc, $tgt", "",
                        (outs), (ins i16imm:$tgt, cc:$cc)>;
  let Uses = [SPL] in {
    def CALL24   : I24i<NoPre, 0xCD, "call", "\t$tgt", "",
                        (outs), (ins i24imm:$tgt), [(Z80call mempat:$tgt)]>;
    def CALL24r  : P   <(outs), (ins    A24:$tgt), [(Z80call    A24:$tgt)]>;
    def RST24    : I24 <NoPre, 0xC7, "rst", "\t$tgt", "",
                        (outs), (ins rsttarget:$tgt)>;
  }
  let Uses = [F, SPL] in
    def CALL24CC : I24i<NoPre, 0xC4, "call", "\t$cc, $tgt", "",
                        (outs), (ins i24imm:$tgt, cc:$cc)>;
}

let isTerminator = true, isReturn = true, isBarrier = true in {
  let Defs = [SPS], Uses = [SPS] in {
    def RET16   : I16<NoPre, 0xC9, "ret",  "", "", (outs), (ins),
                      [(Z80retflag)]>;
    def RETN16  : I16<EDPre, 0x45, "retn", "", "", (outs), (ins),
                      [(Z80retnflag)]>;
    def RETI16  : I16<EDPre, 0x4D, "reti">;
  }
  let Defs = [SPS], Uses = [F, SPS] in
    def RET16CC : I16<NoPre, 0xC0, "ret", "\t$cc", "", (outs), (ins cc:$cc)>;
  let Defs = [SPL], Uses = [SPL] in {
    def RET24   : I24<NoPre, 0xC9, "ret",  "", "", (outs), (ins),
                      [(Z80retflag)]>;
    def RETN24  : I24<EDPre, 0x45, "retn", "", "", (outs), (ins),
                      [(Z80retnflag)]>;
    def RETI24  : I24<EDPre, 0x4D, "reti">;
  }
  let Defs = [SPL], Uses = [F, SPL] in
    def RET24CC : I24<NoPre, 0xC0, "ret", "\t$cc", "", (outs), (ins cc:$cc)>;
  def EI_RETI : P<(outs), (ins), [(Z80retiflag)]>;
}
let isCall = true, isTerminator = true, isReturn = true, isBarrier = true in {
  let Uses = [SPS] in {
    def TCRETURN16   : P<(outs), (ins i16imm:$tgt), [(Z80tcret mempat:$tgt)]>,
                       Requires<[In16BitMode]>;
    def TCRETURN16r  : P<(outs), (ins    A16:$tgt), [(Z80tcret    A16:$tgt)]>,
                       Requires<[In16BitMode]>;
  }
  let Uses = [F, SPS] in
    def TCRETURN16CC : P<(outs), (ins i16imm:$tgt, cc:$cc)>,
                       Requires<[In16BitMode]>;
  let Uses = [SPL] in {
    def TCRETURN24   : P<(outs), (ins i24imm:$tgt), [(Z80tcret mempat:$tgt)]>,
                       Requires<[In24BitMode]>;
    def TCRETURN24r  : P<(outs), (ins    A24:$tgt), [(Z80tcret    A24:$tgt)]>,
                       Requires<[In24BitMode]>;
  }
  let Uses = [F, SPL] in
    def TCRETURN24CC : P<(outs), (ins i24imm:$tgt, cc:$cc)>,
                       Requires<[In24BitMode]>;
}

let isBranch = true, isTerminator = true in {
  let isBarrier = true in {
    def JQ : Pseudo<"jq", "\t$tgt", "", (outs), (ins jmptarget:$tgt),
                    [(br bb:$tgt)]>;
    def JR   : Io  <  NoPre, 0x18, "jr", "\t$tgt", "",
                    (outs), (ins jmptargetoff:$tgt)>;
    def JP16 : I16i<  NoPre, 0xC3, "jp", "\t$tgt", "",
                    (outs), (ins jmptarget:$tgt)>;
    def JP24 : I24i<  NoPre, 0xC3, "jp", "\t$tgt", "",
                    (outs), (ins jmptarget:$tgt)>;
    let isIndirectBranch = true in {
      def JP16r : I16<Idx0Pre, 0xE9, "jp", "\t($tgt)", "",
                      (outs), (ins A16:$tgt), [(brind A16:$tgt)]>;
      def JP24r : I24<Idx0Pre, 0xE9, "jp", "\t($tgt)", "",
                      (outs), (ins A24:$tgt), [(brind A24:$tgt)]>;
    }
  }
  let Defs = [B], Uses = [B] in
  def DJNZ : Io  <  NoPre, 0x10, "djnz", "\t$tgt", "",
                  (outs), (ins jmptargetoff:$tgt)>;
  let Uses = [F] in {
    def JQCC : Pseudo<"jq", "\t$cc, $tgt", "",
                      (outs), (ins jmptarget:$tgt, cc:$cc),
                      [(Z80brcond bb:$tgt, imm:$cc, F)]>;
    def JRCC   : Io  <  NoPre, 0x18, "jr", "\t$cc, $tgt", "",
                      (outs), (ins jmptargetoff:$tgt, cc:$cc)>;
    def JP16CC : I16i<  NoPre, 0xC3, "jp", "\t$cc, $tgt", "",
                      (outs), (ins jmptarget:$tgt, cc:$cc)>;
    def JP24CC : I24i<  NoPre, 0xC3, "jp", "\t$cc, $tgt", "",
                      (outs), (ins jmptarget:$tgt, cc:$cc)>;
  }
}

//===----------------------------------------------------------------------===//
//  Load Instructions.
//===----------------------------------------------------------------------===//

def LD8gg  : I<NoPre, 0x40, "ld", "\t$dst, $src", "",
               (outs G8:$dst), (ins G8:$src)>;
def LD8xx  : I<DDPre, 0x40, "ld", "\t$dst, $src", "",
               (outs X8:$dst), (ins X8:$src)>, Requires<[HaveIdxHalf]>;
def LD8yy  : I<FDPre, 0x40, "ld", "\t$dst, $src", "",
               (outs Y8:$dst), (ins Y8:$src)>, Requires<[HaveIdxHalf]>;
let Defs = [I], Uses = [A] in
def LD8ia  : I<EDPre, 0x47, "ld", "\ti, a">;
let Defs = [R], Uses = [A] in
def LD8ra  : I<EDPre, 0x4F, "ld", "\tr, a">;
let Defs = [A, F], Uses = [I] in
def LD8ai  : I<EDPre, 0x57, "ld", "\ta, i">;
let Defs = [A, F], Uses = [R] in
def LD8ar  : I<EDPre, 0x5F, "ld", "\ta, r">;
let Defs = [MB], Uses = [A] in
def LD8mba : I<EDPre, 0x6D, "ld", "\tmb, a">, Requires<[In24BitMode]>;
let Defs = [A], Uses = [MB] in
def LD8amb : I<EDPre, 0x6E, "ld", "\ta, mb">, Requires<[HaveEZ80Ops]>;
let Defs = [I], Uses = [HL] in
def LD16ia : I<EDPre, 0xC7, "ld", "\ti, hl">, Requires<[HaveEZ80Ops]>;
let Defs = [HL], Uses = [I] in
def LD16ai : I16<EDPre, 0xD7, "ld", "\thl, i">, Requires<[HaveEZ80Ops]>;
let Defs = [UHL], Uses = [I, MB] in
def LD24ai : I24<EDPre, 0xD7, "ld", "\thl, i">;

let isMoveImm = true, isReMaterializable = true in {
  let Defs = [F] in {
    def LD8r0   : Pseudo<"ld", "\t$dst, 0",  "",
                         (outs  R8:$dst), (ins), [(set  R8:$dst,  0)]>;
    def LD24r0  : Pseudo<"ld", "\t$dst, 0",  "",
                         (outs R24:$dst), (ins), [(set R24:$dst,  0)]>;
    def LD24r_1 : Pseudo<"ld", "\t$dst, -1", "",
                         (outs R24:$dst), (ins), [(set R24:$dst,
                                                       !sub(!shl(1, 24), 1))]>;
  }
  def LD8ri  : Ii  <Idx0Pre, 0x06, "ld", "\t$dst, $src", "",
                    (outs  R8:$dst), (ins  i8imm:$src),
                    [(set  R8:$dst, imm:$src)]>;
  def LD16ri : I16i<Idx0Pre, 0x01, "ld", "\t$dst, $src", "",
                    (outs R16:$dst), (ins i16imm:$src),
                    [(set R16:$dst, imm:$src)]>;
  def LD24ri : I24i<Idx0Pre, 0x01, "ld", "\t$dst, $src", "",
                    (outs R24:$dst), (ins i24imm:$src),
                    [(set R24:$dst, imm:$src)]>;
}

let mayLoad = true, canFoldAsLoad = true, isReMaterializable = true in {
  let Defs = [A] in
  def LD8am  : I8i   <NoPre,   0x3A, "ld", "\ta, $src",    "",
                      (outs), (ins mem:$src), [(set A, (load mempat:$src))]>;
  def LD16rm : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs R16:$dst), (ins mem:$src),
                      [(set R16:$dst, (load mempat:$src))]>;
  def LD24rm : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs R24:$dst), (ins mem:$src),
                      [(set R24:$dst, (load mempat:$src))]>;
  def LD16am : I16i  <Idx0Pre, 0x2A, "ld", "\t$dst, $src", "",
                      (outs A16:$dst), (ins mem:$src)>;
  def LD24am : I24i  <Idx0Pre, 0x2A, "ld", "\t$dst, $src", "",
                      (outs A24:$dst), (ins mem:$src)>;
  def LD16gm : I16i  <  EDPre, 0x4B, "ld", "\t$dst, $src", "",
                      (outs G16:$dst), (ins mem:$src)>;
  def LD24gm : I24i  <  EDPre, 0x4B, "ld", "\t$dst, $src", "",
                      (outs G24:$dst), (ins mem:$src)>;
  let Defs = [SPS] in
  def LD16sm : I16i  <  EDPre, 0x7B, "ld", "\tsp, $src", "",
                      (outs), (ins mem:$src)>;
  let Defs = [SPL] in
  def LD24sm : I24i  <  EDPre, 0x7B, "ld", "\tsp, $src", "",
                      (outs), (ins mem:$src)>;

  def LD8rp  : Pseudo<               "ld", "\t$dst, $src", "",
                     (outs R8:$dst), (ins aptr:$src)>;
  let Defs = [A] in
  def LD8ap  : I     <  NoPre, 0x02, "ld", "\ta, $src", "",
                      (outs), (ins optr:$src), [(set A, (load iPTR:$src))]>;
  def LD8gp  : I     <NoPre,   0x46, "ld", "\t$dst, $src", "",
                      (outs G8:$dst), (ins aptr:$src),
                      [(set G8:$dst, (load iPTR:$src))]>;
  def LD16rp : I16   <EDPre,   0x07, "ld", "\t$dst, $src", "",
                      (outs R16:$dst), (ins aptr:$src),
                      [(set R16:$dst, (load iPTR:$src))]>,
               Requires<[HaveEZ80Ops]>;
  def LD88rp : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs R16:$dst), (ins aptr:$src),
                      [(set R16:$dst, (load iPTR:$src))]>;
  def LD24rp : I24   <EDPre,   0x07, "ld", "\t$dst, $src", "",
                      (outs R24:$dst), (ins aptr:$src),
                      [(set R24:$dst, (load iPTR:$src))]>;

  def LD8ro  : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs R8:$dst), (ins off:$src)>;
  def LD8go  : Io    <Idx1Pre, 0x46, "ld", "\t$dst, $src", "",
                      (outs   G8:$dst), (ins off:$src),
                      [(set G8:$dst, (load offpat:$src))]>;
  def LD16ro : I16o  <Idx1Pre, 0x07, "ld", "\t$dst, $src", "",
                      (outs R16:$dst), (ins off:$src),
                      [(set R16:$dst, (load offpat:$src))]>,
               Requires<[HaveEZ80Ops]>;
  def LD88ro : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs R16:$dst), (ins off:$src),
                      [(set R16:$dst, (load offpat:$src))]>;
  def LD24ro : I24o  <Idx1Pre, 0x07, "ld", "\t$dst, $src", "",
                      (outs R24:$dst), (ins off:$src),
                      [(set R24:$dst, (load offpat:$src))]>;
}
def : Pat<(i16 (extloadi8  mempat:$src)), (LD16rm mem:$src)>;
def : Pat<(i16 (extloadi8    iPTR:$src)),
          (INSERT_SUBREG (IMPLICIT_DEF), (LD8rp aptr:$src), sub_low)>;
def : Pat<(i16 (extloadi8  offpat:$src)),
          (INSERT_SUBREG (IMPLICIT_DEF), (LD8ro off:$src), sub_low)>;
def : Pat<(i24 (extloadi8  mempat:$src)), (LD24rm mem:$src)>;
def : Pat<(i24 (extloadi8    iPTR:$src)),
          (INSERT_SUBREG (IMPLICIT_DEF), (LD8rp aptr:$src), sub_low)>;
def : Pat<(i24 (extloadi8  offpat:$src)),
          (INSERT_SUBREG (IMPLICIT_DEF), (LD8ro off:$src), sub_low)>;
def : Pat<(i24 (extloadi16 mempat:$src)), (LD24rm  mem:$src)>;
def : Pat<(i24 (extloadi16   iPTR:$src)), (LD24rp aptr:$src)>;
def : Pat<(i24 (extloadi16 offpat:$src)), (LD24ro  off:$src)>;

let mayStore = true in {
  let Uses = [A] in
  def LD8ma  : I8i   <  NoPre, 0x32, "ld", "\t$dst, a",    "",
                      (outs), (ins mem:$dst), [(store A, mempat:$dst)]>;
  def LD16mr : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs), (ins mem:$dst, R16:$src),
                      [(store R16:$src, mempat:$dst)]>;
  def LD24mr : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs), (ins mem:$dst, R24:$src),
                      [(store R24:$src, mempat:$dst)]>;
  def LD16ma : I16i  <Idx1Pre, 0x22, "ld", "\t$dst, $src", "",
                      (outs), (ins mem:$dst, A16:$src)>;
  def LD24ma : I24i  <Idx1Pre, 0x22, "ld", "\t$dst, $src", "",
                      (outs), (ins mem:$dst, A24:$src)>;
  def LD16mg : I16i  <  EDPre, 0x43, "ld", "\t$dst, $src", "",
                      (outs), (ins mem:$dst, G16:$src)>;
  def LD24mg : I24i  <  EDPre, 0x43, "ld", "\t$dst, $src", "",
                      (outs), (ins mem:$dst, G24:$src)>;
  let Uses = [SPS] in
  def LD16ms : I16i  <  EDPre, 0x73, "ld", "\t$dst, sp", "",
                      (outs), (ins mem:$dst)>;
  let Uses = [SPL] in
  def LD24ms : I24i  <  EDPre, 0x73, "ld", "\t$dst, sp", "",
                      (outs), (ins mem:$dst)>;

  def LD8pr  : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs), (ins aptr:$dst, R8:$src)>;
  let Uses = [A] in
  def LD8pa  : I     <  NoPre, 0x02, "ld", "\t$dst, a", "",
                      (outs), (ins optr:$dst), [(store A, iPTR:$dst)]>;
  def LD8pg  : I     <  NoPre, 0x70, "ld", "\t$dst, $src", "",
                      (outs), (ins aptr:$dst, G8:$src),
                      [(store G8 :$src, iPTR:$dst)]>;
  def LD16pr : I16   <  EDPre, 0x0F, "ld", "\t$dst, $src", "",
                      (outs), (ins aptr:$dst, R16:$src),
                      [(store R16:$src, iPTR:$dst)]>,
               Requires<[In16BitMode, HaveEZ80Ops]>;
  def LD88pr : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs), (ins aptr:$dst, R16:$src),
                      [(store R16:$src, iPTR:$dst)]>;
  def LD24pr : I24   <  EDPre, 0x0F, "ld", "\t$dst, $src", "",
                      (outs), (ins aptr:$dst, R24:$src),
                      [(store R24:$src, iPTR:$dst)]>;

  def LD8or  : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs), (ins off:$dst, R8:$src)>;
  def LD8og  : Io    <Idx0Pre, 0x70, "ld", "\t$dst, $src", "",
                      (outs), (ins off:$dst, G8:$src),
                      [(store G8:$src, offpat:$dst)]>;
  def LD16or : I16o  <Idx0Pre, 0x0F, "ld", "\t$dst, $src", "",
                      (outs), (ins off:$dst, R16:$src)>,
               Requires<[HaveEZ80Ops]>;
}
// LD16/24or encodes R16/24:$src, but there aren't enough index registers for
// regalloc to use it, since one of the index registers is reserved as the frame
// pointer and the other is used in offpat:$dst, so just pattern match on
// G16/24:$src.
def : Pat<(store G16:$src, offpat:$dst), (LD16or off:$dst, R16:$src)>,
      Requires<[In16BitMode, HaveEZ80Ops]>;
let mayStore = true in {
  def LD88or : Pseudo<               "ld", "\t$dst, $src", "",
                      (outs), (ins off:$dst, R16:$src),
                      [(store R16:$src, offpat:$dst)]>;
  def LD24or : I24o  <Idx0Pre, 0x0F, "ld", "\t$dst, $src", "",
                      (outs), (ins off:$dst, R24:$src)>;
}
// Same as above.
def : Pat<(store G24:$src, offpat:$dst), (LD24or off:$dst, R24:$src)>;

let mayStore = true in {
  def LD8pi  : Ii    <  NoPre, 0x36, "ld", "\t$dst, $src", "",
                      (outs), (ins aptr:$dst, i8imm:$src),
                      [(store (i8 imm:$src),   iPTR:$dst)]>;
  def LD8oi  : Ioi   <Idx0Pre, 0x36, "ld", "\t$dst, $src", "",
                      (outs), (ins  off:$dst, i8imm:$src),
                      [(store (i8 imm:$src), offpat:$dst)]>;
}

let Defs = [SPS] in {
  def LD16si : I16i<  NoPre, 0x31, "ld", "\tsp, $src", "",
                    (outs), (ins i16imm:$src), [(set SPS, imm:$src)]>;
  def LD16sa : I16 <Idx0Pre, 0xF9, "ld", "\tsp, $src", "",
                    (outs), (ins A16:$src)>;
}
let Defs = [SPL] in {
  def LD24si : I16i<  NoPre, 0x31, "ld", "\tsp, $src", "",
                    (outs), (ins i24imm:$src), [(set SPL, imm:$src)]>;
  def LD24sa : I24<Idx0Pre, 0xF9, "ld", "\tsp, $src", "",
                   (outs), (ins A24:$src)>;
}

let Defs = [AF], Uses = [AF] in
def EXAF : I<NoPre, 0x08, "ex", "\taf, af'">;
let Defs = [HL, DE, BC], Uses = [HL, DE, BC] in
def EXX16  : I16<NoPre, 0xD9, "exx">;
let Defs = [UHL, UDE, UBC], Uses = [UHL, UDE, UBC] in
def EXX24  : I24<NoPre, 0xD9, "exx">;

let Defs = [DE, HL], Uses = [DE, HL] in
def EX16DE : I16<NoPre, 0xEB, "ex", "\tde, hl">;
let Defs = [UDE, UHL], Uses = [UDE, UHL] in
def EX24DE : I24<NoPre, 0xEB, "ex", "\tde, hl">;

let Constraints = "$imp = $dst" in {
let Uses = [SPS] in
def EX16sa : I16<Idx0Pre, 0xE3, "ex", "\t(sp), $dst", "",
                 (outs A16:$dst), (ins A16:$imp)>;
let Uses = [SPL] in
def EX24sa : I24<Idx0Pre, 0xE3, "ex", "\t(sp), $dst", "",
                 (outs A24:$dst), (ins A24:$imp)>;
}

let Defs = [SPS], Uses = [SPS] in {
  let mayLoad = true in
    def POP16r  : I16 <Idx0Pre, 0xC1, "pop", "\t$dst", "",
                       (outs R16:$dst), (ins), [(set R16:$dst, Z80pop)]>,
                  Requires<[In16BitMode]>;
  let mayStore = true in {
    def PUSH16r : I16 <Idx0Pre, 0xC5, "push", "\t$src", "",
                       (outs), (ins R16:$src), [(Z80push R16:$src)]>,
                  Requires<[In16BitMode]>;
    def PEA16o  : I16o<EDPre, 0x65, "pea", "\t$src", "",
                       (outs), (ins off16:$src), [(Z80push offpat:$src)]>,
                  Requires<[In16BitMode, HaveEZ80Ops]>;
  }
}
let Defs = [AF, SPS], Uses = [SPS], mayLoad = true in
def POP16AF  : I16<NoPre, 0xF1, "pop", "\taf", "",
                   (outs), (ins), [(set AF, Z80pop)]>,
               Requires<[In16BitMode]>;
let Defs = [SPS], Uses = [AF, SPS], mayStore = true in
def PUSH16AF : I16<NoPre, 0xF5, "push", "\taf", "",
                   (outs), (ins), [(Z80push AF)]>,
               Requires<[In16BitMode]>;
let Defs = [SPL], Uses = [SPL] in {
  let mayLoad = true in
    def POP24r  : I24 <Idx0Pre, 0xC1, "pop", "\t$dst", "",
                       (outs R24:$dst), (ins), [(set R24:$dst, Z80pop)]>;
  let mayStore = true in {
    def PUSH24r : I24 <Idx0Pre, 0xC5, "push", "\t$src", "",
                       (outs), (ins R24:$src), [(Z80push R24:$src)]>;
    def PEA24o  : I24o<EDPre,   0x65, "pea", "\t$src", "",
                       (outs), (ins off24:$src), [(Z80push offpat:$src)]>;
  }
}
let Defs = [AF, SPL], Uses = [SPL], mayLoad = true in
def POP24AF  : I24<NoPre, 0xF1, "pop", "\taf", "",
                   (outs), (ins), [(set AF, Z80pop)]>;
let Defs = [SPL], Uses = [AF, SPL], mayStore = true in
def PUSH24AF : I24<NoPre, 0xF5, "push", "\taf", "",
                   (outs), (ins), [(Z80push AF)]>;

let Defs = [F] in {
  let isReMaterializable = true in {
    def RCF : P;
    def SCF : I<NoPre, 0x37, "scf">;
  }
  let Uses = [F] in
  def CCF : I<NoPre, 0x3F, "ccf">;
}

//===----------------------------------------------------------------------===//
//  Arithmetic Instructions.
//===----------------------------------------------------------------------===//

let Defs = [A, F], Uses = [A] in {
  def CPL  : I<NoPre, 0x2F, "cpl", "", "", (outs), (ins), [(set A, ( not A))]>;
  def NEG  : I<EDPre, 0x44, "neg", "", "", (outs), (ins), [(set A, (ineg A))]>;
  def RLCA : I<NoPre, 0x07, "rlca">;
  def RRCA : I<NoPre, 0x0F, "rrca">;
}

let Defs = [A, F], Uses = [A, F] in {
  def RLA : I<NoPre, 0x17, "rla">;
  def RRA : I<NoPre, 0x1F, "rra">;
  def DAA : I<NoPre, 0x27, "daa">;
}

let Defs = [A, F], Uses = [HL, A] in {
  def RRD16 : I16<EDPre, 0x67, "rrd">;
  def RLD16 : I16<EDPre, 0x67, "rld">;
}

let Defs = [A, F], Uses = [UHL, A] in {
  def RRD24 : I24<EDPre, 0x67, "rrd">;
  def RLD24 : I24<EDPre, 0x67, "rld">;
}

let Defs = [F] in
multiclass UnOp8RF<Prefix prefix, bits<8> opcode, string mnemonic,
                   list<Predicate> Preds = []> {
  defvar rc8 = !if(!eq(prefix, CBPre), G8, R8);
  def 8r   : I <prefix, opcode, mnemonic, "\t$dst", "$imp = $dst",
                (outs rc8:$dst), (ins rc8:$imp),
                [(set rc8:$dst, F,
                        (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                            rc8:$imp))]>,
             Requires<Preds>;
  let mayLoad = true, mayStore = true in {
    def 8p   : I <prefix, opcode, mnemonic, "\t$adr", "",
                  (outs), (ins aptr:$adr),
                  [(store (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              (i8 (load iPTR:$adr))), iPTR:$adr),
                   (implicit F)]>,
               Requires<Preds>;
    def 8o   : Io<prefix, opcode, mnemonic, "\t$adr", "", (outs), (ins  off:$adr),
                  [(store (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              (i8 (load offpat:$adr))), offpat:$adr),
                   (implicit F)]>,
               Requires<Preds>;
    if !eq(prefix, Idx0CBPre) then
    def 8go  : Io<Idx1CBPre, opcode, mnemonic, "\t$adr, $dst", "",
                  (outs rc8:$dst), (ins off:$adr)>,
               Requires<!listconcat(Preds, [HaveUndocOps])>;
  }
}
let Defs = [F], Uses = [F] in
multiclass UnOp8RFF<Prefix prefix, bits<8> opcode, string mnemonic,
                    list<Predicate> Preds = []> {
  defvar rc8 = !if(!eq(prefix, CBPre), G8, R8);
  def 8r   : I <prefix, opcode, mnemonic, "\t$dst", "$imp = $dst",
                (outs rc8:$dst), (ins rc8:$imp),
                [(set rc8:$dst, F,
                        (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                            rc8:$imp, F))]>,
             Requires<Preds>;
  let mayLoad = true, mayStore = true in {
    def 8p   : I <prefix, opcode, mnemonic, "\t$adr", "",
                  (outs), (ins aptr:$adr),
                  [(store (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              (i8 (load iPTR:$adr)), F), iPTR:$adr),
                   (implicit F)]>,
               Requires<Preds>;
    def 8o   : Io<prefix, opcode, mnemonic, "\t$adr", "", (outs), (ins  off:$adr),
                  [(store (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              (i8 (load offpat:$adr)), F), offpat:$adr),
                   (implicit F)]>,
               Requires<Preds>;
    if !eq(prefix, Idx0CBPre) then
    def 8go  : Io<Idx1CBPre, opcode, mnemonic, "\t$adr, $dst", "",
                  (outs rc8:$dst), (ins off:$adr)>,
               Requires<!listconcat(Preds, [HaveUndocOps])>;
  }
}
multiclass BinOp8RF<bits<3> opcode, string mnemonic, bit compare = false> {
  let isCompare = compare, Defs = [A, F], Uses = [A] in {
    def 8ar  : I <Idx0Pre, {0b10, opcode, 0b000}, mnemonic, "\ta, $src", "",
                  (outs), (ins    R8:$src),
                  [(set A, F,
                        (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                            A, R8:$src))]>;
    def 8ai  : Ii<  NoPre, {0b11, opcode, 0b110}, mnemonic, "\ta, $src", "",
                  (outs), (ins i8imm:$src),
                  [(set A, F,
                        (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                            A, imm:$src))]>;
    let mayLoad = true in {
      def  8ap : I <Idx0Pre, {0b10, opcode, 0b110}, mnemonic, "\ta, $src", "",
                    (outs), (ins  aptr:$src),
                    [(set A, F,
                          (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              A, (i8 (load   iPTR:$src))))]>;
      def  8ao : Io<Idx0Pre, {0b10, opcode, 0b110}, mnemonic, "\ta, $src", "",
                    (outs), (ins   off:$src),
                    [(set A, F,
                          (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              A, (i8 (load offpat:$src))))]>;
    }
  }
  def : Pat<(!cast<SDNode>(mnemonic) A,  R8:$src),
            (!cast<Instruction>(!strconcat(NAME, "8ar"))   R8:$src)>;
  def : Pat<(!cast<SDNode>(mnemonic) A, imm:$src),
            (!cast<Instruction>(!strconcat(NAME, "8ai"))  imm:$src)>;
  def : Pat<(!cast<SDNode>(mnemonic) A, (load iPTR:$src)),
            (!cast<Instruction>(!strconcat(NAME, "8ap")) aptr:$src)>;
  def : Pat<(!cast<SDNode>(mnemonic) A, (load offpat:$src)),
            (!cast<Instruction>(!strconcat(NAME, "8ao"))  off:$src)>;
}
multiclass BinOp8RFF<bits<3> opcode, string mnemonic, SDNode node, bit compare = false> {
  let isCompare = compare, Defs = [A, F], Uses = [A, F] in {
    def 8ar  : I <Idx0Pre, {0b10, opcode, 0b000}, mnemonic, "\ta, $src", "",
                  (outs), (ins    R8:$src),
                  [(set A, F,
                        (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                            A, R8:$src, F))]>;
    def 8ai  : Ii<  NoPre, {0b11, opcode, 0b110}, mnemonic, "\ta, $src", "",
                  (outs), (ins i8imm:$src),
                  [(set A, F,
                        (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                            A, imm:$src, F))]>;
    let mayLoad = true in {
      def 8ap  : I <Idx0Pre, {0b10, opcode, 0b110}, mnemonic, "\ta, $src", "",
                    (outs), (ins  aptr:$src),
                    [(set A, F,
                          (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              A, (i8 (load iPTR:$src)), F))]>;
      def 8ao  : Io<Idx0Pre, {0b10, opcode, 0b110}, mnemonic, "\ta, $src", "",
                    (outs), (ins   off:$src),
                    [(set A, F,
                          (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              A, (i8 (load offpat:$src)), F))]>;
    }
  }
  def : Pat<(node A,  R8:$src),
            (!cast<Instruction>(!strconcat(NAME, "8ar"))   R8:$src)>;
  def : Pat<(node A, imm:$src),
            (!cast<Instruction>(!strconcat(NAME, "8ai"))  imm:$src)>;
  def : Pat<(node A, (load iPTR:$src)),
            (!cast<Instruction>(!strconcat(NAME, "8ap")) aptr:$src)>;
  def : Pat<(node A, (load offpat:$src)),
            (!cast<Instruction>(!strconcat(NAME, "8ao"))  off:$src)>;
}
multiclass BinOp8F<Prefix prefix, bits<3> opcode, string mnemonic,
                   bit compare = false> {
  let isCompare = compare, Defs = [F], Uses = [A] in {
    def 8ar  : I <prefix, {0b10, opcode, 0b000}, mnemonic, "\ta, $src", "",
                  (outs), (ins    R8:$src),
                  [(set F,
                        (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                            A, R8:$src))]>;
    def 8ai  : Ii<prefix, {0b11, opcode, 0b110}, mnemonic, "\ta, $src", "",
                  (outs), (ins i8imm:$src),
                  [(set F,
                        (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                            A, imm:$src))]>;
    let mayLoad = true in {
      def 8ap  : I <prefix, {0b10, opcode, 0b110}, mnemonic, "\ta, $src", "",
                    (outs), (ins  aptr:$src),
                    [(set F,
                          (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              A, (i8 (load iPTR:$src))))]>;
      if !eq(prefix, Idx0Pre) then
      def 8ao  : Io<prefix, {0b10, opcode, 0b110}, mnemonic, "\ta, $src", "",
                    (outs), (ins   off:$src),
                    [(set F,
                          (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              A, (i8 (load offpat:$src))))]>;
    }
  }
}
let Defs = [F] in
multiclass BitOp8F<bits<2> opcode, string mnemonic> {
  def 8bg  : I <    CBPre, {opcode, 0b000, 0b000},
                mnemonic, "\t$bit, $src", "",
                (outs), (ins bitimm:$bit, G8:$src),
                [(set F, (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                             bitimm:$bit, G8:$src))]>;
  let mayLoad = true in {
    def 8bp  : I <    CBPre, {opcode, 0b000, 0b110},
                  mnemonic, "\t$bit, $adr", "",
                  (outs), (ins bitimm:$bit, aptr:$adr),
                  [(set F, (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                               bitimm:$bit, (i8 (load iPTR:$adr))))]>;
    def 8bo  : Io<Idx1CBPre, {opcode, 0b000, 0b110},
                  mnemonic, "\t$bit, $adr", "",
                  (outs), (ins bitimm:$bit,  off:$adr),
                  [(set F, (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                               bitimm:$bit, (i8 (load offpat:$adr))))]>;
  }
}
multiclass BitOp8R<bits<2> opcode, string mnemonic> {
  def 8bg  : I <   CBPre, {opcode, 0b000, 0b000},
                mnemonic, "\t$bit, $dst", "$imp = $dst",
                (outs G8:$dst), (ins bitimm:$bit, G8:$imp),
                [(set G8:$dst,
                      (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                          bitimm:$bit, G8:$imp))]>;
  let mayLoad = true, mayStore = true in {
    def 8bp  : I <    CBPre, {opcode, 0b000, 0b110},
                  mnemonic, "\t$bit, $adr", "",
                  (outs), (ins bitimm:$bit, aptr:$adr),
                  [(store (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              bitimm:$bit, (i8 (load   iPTR:$adr))),
                              iPTR:$adr),
                   (implicit F)]>;
    def 8bo  : Io<Idx1CBPre, {opcode, 0b000, 0b110},
                  mnemonic, "\t$bit, $adr", "",
                  (outs), (ins bitimm:$bit,  off:$adr),
                  [(store (!cast<SDNode>(!strconcat("Z80", mnemonic, "_flag"))
                              bitimm:$bit, (i8 (load offpat:$adr))),
                              offpat:$adr),
                   (implicit F)]>;
    def 8gbo : Io<Idx2CBPre, {opcode, 0b000, 0b000},
                  mnemonic, "\t$bit, $adr, $dst", "",
                  (outs G8:$dst), (ins bitimm:$bit,  off:$adr)>,
               Requires<[HaveUndocOps]>;
  }
}

defm RLC : UnOp8RF  <Idx0CBPre, 0, "rlc">;
defm RRC : UnOp8RF  <Idx0CBPre, 1, "rrc">;
defm RL  : UnOp8RFF <Idx0CBPre, 2, "rl">;
defm RR  : UnOp8RFF <Idx0CBPre, 3, "rr">;
defm SLA : UnOp8RF  <Idx0CBPre, 4, "sla">;
defm SRA : UnOp8RF  <Idx0CBPre, 5, "sra">;
defm SLI : UnOp8RF  <Idx0CBPre, 6, "sli", [HaveSliOp]>;
defm SRL : UnOp8RF  <Idx0CBPre, 7, "srl">;
defm BIT : BitOp8F  <           1, "bit">;
defm RES : BitOp8R  <           2, "res">;
defm SET : BitOp8R  <           3, "set">;
defm INC : UnOp8RF  <  Idx0Pre, 4, "inc">;
defm DEC : UnOp8RF  <  Idx0Pre, 5, "dec">;
defm ADD : BinOp8RF <           0, "add">;
defm ADC : BinOp8RFF<           1, "adc", adde>;
defm SUB : BinOp8RF <           2, "sub", true>;
defm SBC : BinOp8RFF<           3, "sbc", sube>;
defm AND : BinOp8RF <           4, "and">;
defm XOR : BinOp8RF <           5, "xor">;
defm OR  : BinOp8RF <           6, "or",  true>;
defm CP  : BinOp8F  <  Idx0Pre, 7, "cp",  true>;
defm TST : BinOp8F  <    EDPre, 4, "tst", true>, Requires<[HaveZ180Ops]>;

def : MnemonicAlias<"sll", "sli">, Requires<[HaveSliOp]>;
def : MnemonicAlias<"sl1", "sli">, Requires<[HaveSliOp]>;

def : Pat<(fshl G8:$reg, G8:$reg, (i8  1)), (RLC8r G8:$reg)>;
def : Pat<(fshl G8:$reg, G8:$reg, (i8  7)), (RRC8r G8:$reg)>;
def : Pat<(shl G8:$reg, (i8  1)), (SLA8r G8:$reg)>;
def : Pat<(sra G8:$reg, (i8  1)), (SRA8r G8:$reg)>;
def : Pat<(or (shl G8:$reg, (i8  1)), (i8  1)), (SLI8r G8:$reg)>,
      Requires<[HaveSliOp]>;
def : Pat<(srl G8:$reg, (i8  1)), (SRL8r G8:$reg)>;
def : Pat<(add R8:$reg, (i8  1)), (INC8r R8:$reg)>;
def : Pat<(add R8:$reg, (i8 -1)), (DEC8r R8:$reg)>;

def INC16r : I16<Idx0Pre, 0x03, "inc", "\t$dst", "$imp = $dst",
                 (outs R16:$dst), (ins R16:$imp),
                 [(set R16:$dst, (Z80inc_flag R16:$imp))]>;
def DEC16r : I16<Idx0Pre, 0x0B, "dec", "\t$dst", "$imp = $dst",
                 (outs R16:$dst), (ins R16:$imp),
                 [(set R16:$dst, (Z80dec_flag R16:$imp))]>;
def INC24r : I24<Idx0Pre, 0x03, "inc", "\t$dst", "$imp = $dst",
                 (outs R24:$dst), (ins R24:$imp),
                 [(set R24:$dst, (Z80inc_flag R24:$imp))]>;
def DEC24r : I24<Idx0Pre, 0x0B, "dec", "\t$dst", "$imp = $dst",
                 (outs R24:$dst), (ins R24:$imp),
                 [(set R24:$dst, (Z80dec_flag R24:$imp))]>;
let Defs = [SPS], Uses = [SPS] in {
def INC16s : I16<NoPre, 0x33, "inc", "\tsp", "",
                 (outs), (ins), [(set SPS, (Z80inc_flag SPS))]>;
def DEC16s : I16<NoPre, 0x3B, "dec", "\tsp", "",
                 (outs), (ins), [(set SPS, (Z80dec_flag SPS))]>;
}
let Defs = [SPL], Uses = [SPL] in {
def INC24s : I24<NoPre, 0x33, "inc", "\tsp", "",
                 (outs), (ins), [(set SPL, (Z80inc_flag SPL))]>;
def DEC24s : I24<NoPre, 0x3B, "dec", "\tsp", "",
                 (outs), (ins), [(set SPL, (Z80dec_flag SPL))]>;
}
def : Pat<(add R16:$imp,  1), (INC16r R16:$imp)>;
def : Pat<(add R16:$imp, -1), (DEC16r R16:$imp)>;
def : Pat<(add R24:$imp,  1), (INC24r R24:$imp)>;
def : Pat<(add R24:$imp, -1), (DEC24r R24:$imp)>;

let Defs = [F] in {
  let AddedComplexity = 1 in {
    def ADD16aa : I16<Idx0Pre, 0x29, "add", "\t$dst, $imp", "$imp = $dst",
                      (outs A16:$dst), (ins A16:$imp),
                      [(set A16:$dst, F, (Z80add_flag A16:$imp, A16:$imp))]>;
    def ADD24aa : I24<Idx0Pre, 0x29, "add", "\t$dst, $imp", "$imp = $dst",
                      (outs A24:$dst), (ins A24:$imp),
                      [(set A24:$dst, F, (Z80add_flag A24:$imp, A24:$imp))]>;
  }
  def ADD16ao : I16<Idx0Pre, 0x09, "add", "\t$dst, $src", "$imp = $dst",
                    (outs A16:$dst), (ins A16:$imp, O16:$src),
                    [(set A16:$dst, F, (Z80add_flag A16:$imp, O16:$src))]>;
  def ADD24ao : I24<Idx0Pre, 0x09, "add", "\t$dst, $src", "$imp = $dst",
                    (outs A24:$dst), (ins A24:$imp, O24:$src),
                    [(set A24:$dst, F, (Z80add_flag A24:$imp, O24:$src))]>;
  let Uses = [SPS] in
  def ADD16as : I16<Idx0Pre, 0x39, "add", "\t$dst, sp",   "$imp = $dst",
                   (outs A16:$dst), (ins A16:$imp),
                   [(set A16:$dst, F, (Z80add_flag A16:$imp, SPS))]>;
  let Uses = [SPL] in
  def ADD24as : I24<Idx0Pre, 0x39, "add", "\t$dst, sp",   "$imp = $dst",
                   (outs A24:$dst), (ins A24:$imp),
                   [(set A24:$dst, F, (Z80add_flag A24:$imp, SPL))]>;
}
let AddedComplexity = 1 in {
  def : Pat<(add  A16:$imp, A16:$imp), (ADD16aa A16:$imp)>;
  def : Pat<(add  A24:$imp, A24:$imp), (ADD24aa A24:$imp)>;
  def : Pat<(shl  A16:$reg,   (i8 1)), (ADD16aa A16:$reg)>;
  def : Pat<(shl  A24:$reg,   (i8 1)), (ADD24aa A24:$reg)>;
  def : Pat<(addc A16:$imp, A16:$imp), (ADD16aa A16:$imp)>;
  def : Pat<(addc A24:$imp, A24:$imp), (ADD24aa A24:$imp)>;
}
def : Pat<(add  A16:$imp, O16:$src), (ADD16ao A16:$imp, O16:$src)>;
def : Pat<(add  A24:$imp, O24:$src), (ADD24ao A24:$imp, O24:$src)>;
def : Pat<(addc A16:$imp, O16:$src), (ADD16ao A16:$imp, O16:$src)>;
def : Pat<(addc A24:$imp, O24:$src), (ADD24ao A24:$imp, O24:$src)>;

let Defs = [HL, F] in {
  let Uses = [HL, F] in {
    def SBC16aa : I16<EDPre, 0x42, "sbc", "\thl, hl", "", (outs), (ins),
                      [(set  HL, F, (Z80sbc_flag  HL, HL,       F))]>;
    def ADC16aa : I16<EDPre, 0x4A, "adc", "\thl, hl", "", (outs), (ins),
                      [(set  HL, F, (Z80adc_flag  HL, HL,       F))]>;
    def SBC16ao : I16<EDPre, 0x42, "sbc", "\thl, $src", "",
                      (outs), (ins O16:$src),
                      [(set  HL, F, (Z80sbc_flag  HL, O16:$src, F))]>;
    def ADC16ao : I16<EDPre, 0x4A, "adc", "\thl, $src", "",
                      (outs), (ins O16:$src),
                      [(set  HL, F, (Z80adc_flag  HL, O16:$src, F))]>;
  }
  let Uses = [HL, SPS, F] in {
    def SBC16as : I16<EDPre, 0x72, "sbc", "\thl, sp", "", (outs), (ins),
                      [(set  HL, F, (Z80sbc_flag HL, SPS, F))]>;
    def ADC16as : I16<EDPre, 0x7A, "adc", "\thl, sp", "", (outs), (ins),
                      [(set  HL, F, (Z80adc_flag HL, SPS, F))]>;
  }
}
let Defs = [UHL, F] in {
  let Uses = [UHL, F] in {
    def SBC24aa : I24<EDPre, 0x42, "sbc", "\thl, hl", "", (outs), (ins),
                      [(set UHL, F, (Z80sbc_flag UHL, UHL, F))]>;
    def ADC24aa : I24<EDPre, 0x4A, "adc", "\thl, hl", "", (outs), (ins),
                      [(set UHL, F, (Z80adc_flag UHL, UHL, F))]>;
    def SBC24ao : I24<EDPre, 0x42, "sbc", "\thl, $src", "",
                      (outs), (ins O24:$src),
                      [(set UHL, F, (Z80sbc_flag UHL, O24:$src, F))]>;
    def ADC24ao : I24<EDPre, 0x4A, "adc", "\thl, $src", "",
                      (outs), (ins O24:$src),
                      [(set UHL, F, (Z80adc_flag UHL, O24:$src, F))]>;
  }
  let Uses = [UHL, SPL, F] in {
    def SBC24as : I24<EDPre, 0x72, "sbc", "\thl, sp", "", (outs), (ins),
                      [(set UHL, F, (Z80sbc_flag UHL, SPL, F))]>;
    def ADC24as : I24<EDPre, 0x7A, "adc", "\thl, sp", "", (outs), (ins),
                      [(set UHL, F, (Z80adc_flag UHL, SPL, F))]>;
  }
}
def : Pat<(sube  HL, O16:$src), (SBC16ao O16:$src)>;
def : Pat<(adde  HL, O16:$src), (ADC16ao O16:$src)>;
def : Pat<(sube UHL, O24:$src), (SBC24ao O24:$src)>;
def : Pat<(adde UHL, O24:$src), (ADC24ao O24:$src)>;

let Defs = [HL, F], Uses = [HL] in {
  def SUB16ao : P<(outs), (ins O16:$src),
                  [(set  HL, F, (Z80sub_flag  HL, O16:$src))]>;
}
let Defs = [UHL, F], Uses = [UHL] in {
  def SUB24ao : P<(outs), (ins O24:$src),
                  [(set UHL, F, (Z80sub_flag UHL, O24:$src))]>,
                Requires<[HaveEZ80Ops]>;
}
let Defs = [F] in {
  let Uses = [HL] in {
    def CP16a0 : P<(outs), (ins), [(set F, (Z80cp_flag HL, 0))]>;
    def CP16ao : P<(outs), (ins O16:$src),
                   [(set F, (Z80cp_flag HL, O16:$src))]>;
  }
  let Uses = [UHL] in {
    def CP24a0 : P<(outs), (ins), [(set F, (Z80cp_flag UHL, 0))]>,
                 Requires<[HaveEZ80Ops]>;
    def CP24ao : P<(outs), (ins O24:$src),
                   [(set F, (Z80cp_flag UHL, O24:$src))]>,
                 Requires<[HaveEZ80Ops]>;
  }
}
def : Pat<(sub   HL, O16:$src), (SUB16ao O16:$src)>;
def : Pat<(sub  UHL, O24:$src), (SUB24ao O24:$src)>;
def : Pat<(subc  HL, O16:$src), (SUB16ao O16:$src)>;
def : Pat<(subc UHL, O24:$src), (SUB24ao O24:$src)>;

def LEA16ro : I16o<EDPre, 0x02, "lea", "\t$dst, $src", "",
                   (outs R16:$dst), (ins off16:$src),
                   [(set R16:$dst, offpat:$src)]>, Requires<[HaveEZ80Ops]>;
def LEA24ro : I24o<EDPre, 0x02, "lea", "\t$dst, $src", "",
                   (outs R24:$dst), (ins off24:$src),
                   [(set R24:$dst, offpat:$src)]>;

def MLT16r : I<EDPre, 0x4C, "mlt", "\t$dst", "$imp = $dst",
               (outs G16:$dst), (ins G16:$imp),
               [(set G16:$dst, (Z80mlt G16:$imp))]>, Requires<[HaveZ180Ops]>;
let Defs = [SPS], Uses = [SPS] in
def MLT16s : I16<EDPre, 0x7C, "mlt", "\tsp", "", (outs), (ins)>,
             Requires<[HaveZ180Ops]>;
let Defs = [SPL], Uses = [SPL] in
def MLT24s : I24<EDPre, 0x7C, "mlt", "\tsp", "", (outs), (ins)>;
def : Pat<(i8 (mul R8:$op1, R8:$op2)),
          (EXTRACT_SUBREG (MLT16r (REG_SEQUENCE G16, R8:$op1, sub_high,
                                                     R8:$op2, sub_low)),
                          sub_low)>;
def : Pat<(i16 (mul (zext R8:$op1), (zext R8:$op2))),
          (MLT16r (REG_SEQUENCE G16, R8:$op1, sub_high, R8:$op2, sub_low))>;

//===----------------------------------------------------------------------===//
// Port Instructions.
//===----------------------------------------------------------------------===//

let mayLoad = true in {
  let Defs = [A] in
  def  IN8am : Ii<NoPre, 0xDB,  "in",  "\ta, $port", "",
                  (outs), (ins port:$port)>;
  let Defs = [F] in {
    def  IN8gm : Ii<EDPre, 0x00,  "in0", "\t$dst, $port", "",
                    (outs G8:$dst), (ins port:$port)>, Requires<[HaveZ180Ops]>;
    let Uses = [BC] in
    def  IN8gp : I <EDPre, 0x40,  "in",  "\t$dst, (bc)", "",
                    (outs G8:$dst), (ins)>;
  }

  let Defs = [F], Uses = [C] in
  def TST8mi : Ii<EDPre, 0x74, "tstio", "\t$src", "", (outs), (ins i8imm:$src)>,
                  Requires<[HaveZ180Ops]>;
}

def : InstAlias<"in\t$dst, (c)", (IN8gp G8:$dst)>;

let mayStore = true in {
  let Uses = [A] in
  def OUT8ma : Ii<NoPre, 0xD3, "out",  "\t$port, a", "",
                  (outs), (ins port:$port)>;
  def OUT8mg : Ii<EDPre, 0x01, "out0", "\t$port, $src", "",
                  (outs), (ins port:$port, G8:$src)>, Requires<[HaveZ180Ops]>;
  let Uses = [BC] in
  def OUT8pg : I <EDPre, 0x41, "out",  "\t(bc), $src", "",
                  (outs), (ins G8:$src)>;
}

def : InstAlias<"out\t(c), $src", (OUT8pg G8:$src)>;

//===----------------------------------------------------------------------===//
// Block Instructions.
//===----------------------------------------------------------------------===//

multiclass Block16<bits<8> opcode, string mnemonic, string suffix = "",
                   string mnemonicr = mnemonic> {
  def I16  : I16<EDPre, !add(opcode, 0x00),
                 !strconcat(mnemonic,  "i", suffix     )>;
  def D16  : I16<EDPre, !add(opcode, 0x08),
                 !strconcat(mnemonic,  "d", suffix     )>;
  def IR16 : I16<EDPre, !add(opcode, 0x10),
                 !strconcat(mnemonicr, "i", suffix, "r")>;
  def DR16 : I16<EDPre, !add(opcode, 0x18),
                 !strconcat(mnemonicr, "d", suffix, "r")>;
}
multiclass Block24<bits<8> opcode, string mnemonic, string suffix = "",
                   string mnemonicr = mnemonic> {
  def I24  : I24<EDPre, !add(opcode, 0x00),
                 !strconcat(mnemonic,  "i", suffix     )>;
  def D24  : I24<EDPre, !add(opcode, 0x08),
                 !strconcat(mnemonic,  "d", suffix     )>;
  def IR24 : I24<EDPre, !add(opcode, 0x10),
                 !strconcat(mnemonicr, "i", suffix, "r")>;
  def DR24 : I24<EDPre, !add(opcode, 0x18),
                 !strconcat(mnemonicr, "d", suffix, "r")>;
}

let mayLoad = true, mayStore = true in {
  let Defs = [HL, DE, BC, F], Uses = [HL, DE, BC] in {
    defm  LD  : Block16<0xA0,  "ld">;
    def INIRX16 : I16<EDPre, 0xC2, "inirx">, Requires<[HaveEZ80Ops]>;
    def OTIRX16 : I16<EDPre, 0xC3, "otirx">, Requires<[HaveEZ80Ops]>;
    def INDRX16 : I16<EDPre, 0xCA, "indrx">, Requires<[HaveEZ80Ops]>;
    def OTDRX16 : I16<EDPre, 0xCB, "otdrx">, Requires<[HaveEZ80Ops]>;
  }
  let Defs = [HL, BC, F], Uses = [HL, BC] in {
    defm  IN  : Block16<0xA2,  "in">;
    defm OUT  : Block16<0xA3, "out",  "", "ot">;
  }
  let Defs = [UHL, UDE, UBC, F], Uses = [UHL, UDE, UBC] in {
    defm  LD : Block24<0xA0, "ld">;
    def INIRX24 : I24<EDPre, 0xC2, "inirx">;
    def OTIRX24 : I24<EDPre, 0xC3, "otirx">;
    def INDRX24 : I24<EDPre, 0xCA, "indrx">;
    def OTDRX24 : I24<EDPre, 0xCB, "otdrx">;
  }
  let Defs = [UHL, BC, F], Uses = [UHL, BC] in {
    defm  IN : Block24<0xA2, "in">;
    defm OUT : Block24<0xA3, "out",  "", "ot">;
  }

  let Defs = [HL, C, B, F], Uses = [HL, C, B] in {
    defm  INM : Block16<0x82,  "in", "m">, Requires<[HaveEZ80Ops]>;
    defm  OTM : Block16<0x83,  "ot", "m">, Requires<[HaveEZ80Ops]>;
    defm  IN2 : Block16<0x84,  "in", "2">, Requires<[HaveEZ80Ops]>;
    defm OUT2 : Block16<0xA4, "out", "2", "ot">, Requires<[HaveEZ80Ops]>;
  }
  let Defs = [UHL, C, B, F], Uses = [UHL, C, B] in {
    defm  INM : Block24<0x82,  "in", "m">;
    defm  OTM : Block24<0x83,  "ot", "m">;
    defm  IN2 : Block24<0x84,  "in", "2">;
    defm OUT2 : Block24<0xA4, "out", "2", "ot">;
  }

  let usesCustomInserter = true, Defs = [F], Uses = [F] in {
    def LDR16 : P<(outs), (ins R16: $de, R16: $hl, R16: $bc)>;
    def LDR24 : P<(outs), (ins R24:$ude, R24:$uhl, R24:$ubc)>,
                Requires<[HaveEZ80Ops]>;
  }
}

let mayLoad = true in {
  let Defs = [HL, BC, F], Uses = [HL, BC, A] in
  defm CP : Block16<0xA1, "cp">;
  let Defs = [UHL, UBC, F], Uses = [UHL, UBC, A] in
  defm CP : Block24<0xA1, "cp">;
}

//===----------------------------------------------------------------------===//
// Non-Instruction Patterns.
//===----------------------------------------------------------------------===//

// addresses
def : Pat<(i16 (Z80Wrapper tglobaladdr  :$src)), (LD16ri tglobaladdr  :$src)>;
def : Pat<(i24 (Z80Wrapper tglobaladdr  :$src)), (LD24ri tglobaladdr  :$src)>;
def : Pat<(i16 (Z80Wrapper texternalsym :$src)), (LD16ri texternalsym :$src)>;
def : Pat<(i24 (Z80Wrapper texternalsym :$src)), (LD24ri texternalsym :$src)>;
def : Pat<(i16 (Z80Wrapper tblockaddress:$src)), (LD16ri tblockaddress:$src)>;
def : Pat<(i24 (Z80Wrapper tblockaddress:$src)), (LD24ri tblockaddress:$src)>;

// calls
def : Pat<(Z80call (tglobaladdr :$dst)), (CALL16 tglobaladdr :$dst)>,
      Requires<[In16BitMode]>;
def : Pat<(Z80call (tglobaladdr :$dst)), (CALL24 tglobaladdr :$dst)>,
      Requires<[In24BitMode]>;
def : Pat<(Z80call (texternalsym:$dst)), (CALL16 texternalsym:$dst)>,
      Requires<[In16BitMode]>;
def : Pat<(Z80call (texternalsym:$dst)), (CALL24 texternalsym:$dst)>,
      Requires<[In24BitMode]>;

def : Pat<(Z80tcret (tglobaladdr :$dst)), (TCRETURN16 tglobaladdr :$dst)>,
      Requires<[In16BitMode]>;
def : Pat<(Z80tcret (tglobaladdr :$dst)), (TCRETURN24 tglobaladdr :$dst)>,
      Requires<[In24BitMode]>;
def : Pat<(Z80tcret (texternalsym:$dst)), (TCRETURN16 texternalsym:$dst)>,
      Requires<[In16BitMode]>;
def : Pat<(Z80tcret (texternalsym:$dst)), (TCRETURN24 texternalsym:$dst)>,
      Requires<[In24BitMode]>;

//===----------------------------------------------------------------------===//
// Subsystems.
//===----------------------------------------------------------------------===//

// anyext
def : Pat<(i16 (anyext R8 :$src)),
          (INSERT_SUBREG (IMPLICIT_DEF), R8 :$src, sub_low)>;
def : Pat<(i24 (anyext R8 :$src)),
          (INSERT_SUBREG (IMPLICIT_DEF), R8 :$src, sub_low)>;
def : Pat<(i24 (anyext R16:$src)),
          (INSERT_SUBREG (IMPLICIT_DEF), R16:$src, sub_short)>;

// zext
def : Pat<(i16 (zext R8 :$src)),
          (REG_SEQUENCE R16, (LD8r0), sub_high, R8:$src, sub_low)>;
def : Pat<(i24 (zext R8 :$src)),
          (INSERT_SUBREG (i24 (LD24r0)), R8 :$src, sub_low)>;
def : Pat<(i24 (zext R16:$src)),
          (INSERT_SUBREG (i24 (LD24r0)), R16:$src, sub_short)>;

// trunc
def : Pat<(i8  (trunc R16:$src)), (EXTRACT_SUBREG R16:$src, sub_low)>;
def : Pat<(i8  (trunc R24:$src)), (EXTRACT_SUBREG R24:$src, sub_low)>;
def : Pat<(i16 (trunc R24:$src)), (EXTRACT_SUBREG R24:$src, sub_short)>;
