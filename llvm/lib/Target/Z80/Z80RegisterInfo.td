//===- Z80RegisterInfo.td - Describe the Z80 Register File --*- tablegen -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes the Z80 Register file, defining the registers themselves,
// aliases between the registers, and the register classes built out of the
// registers.
//
//===----------------------------------------------------------------------===//

let Namespace = "Z80" in {
  class Z80Reg<string name, bits<16> enc = -1> : Register<name> {
    let HWEncoding = enc;
  }

  // Subregister indices.
  def sub_low   : SubRegIndex<8>;
  def sub_high  : SubRegIndex<8, 8>;
  def sub_short : SubRegIndex<16>;
  // upper 8 bits of a 24-bit eZ80 register
  def sub_upper : SubRegIndex<8, 16>;
  def sub_word1 : SubRegIndex<16, 16>;
  def sub_word2 : SubRegIndex<16, 32>;
  def sub_word3 : SubRegIndex<16, 48>;
  def sub_low24 : SubRegIndex<24>;
  def sub_mid24 : SubRegIndex<24, 24>;
  def sub_high8 : SubRegIndex<8, 24>;

  class Z80RegPair<Z80Reg high, Z80Reg low,
                   bits<16> enc, bits<16> dwarf = enc>
    : Z80Reg<!cond(!and(!eq(!size(high.AsmName), 1),
                        !eq(!size( low.AsmName), 1))
                   : !strconcat(high.AsmName, low.AsmName),
                   !and(!gt(!size(high.AsmName), 1),
                        !gt(!size( low.AsmName), 1),
                        !eq(!substr(high.AsmName,
                                    0, !sub(!size(high.AsmName), 1)),
                            !substr( low.AsmName,
                                    0, !sub(!size( low.AsmName), 1))))
                   : !substr(high.AsmName, 0, !sub(!size(high.AsmName), 1))),
             enc>
    , DwarfRegNum<[dwarf]> {
    let SubRegs = [high, low];
    let SubRegIndices = [sub_high, sub_low];
    let CoveredBySubRegs = 1;
  }

  class EZ80ExtReg<Z80Reg short, Z80Reg upper>
    : Z80Reg<short.AsmName, short.HWEncoding>
    , DwarfRegAlias<short> {
    // model eZ80 24 bit register as a super register of the 16 bit register's
    // 8 bit subregs plus an upper byte. The upper byte is not directly
    // addressable in the ISA, but modeling it enables MachineVerifier liveness
    // tracking. we use 8 bit subregs instead of the 16 bit register directly
    // to avoid duplicate subreg names (which TableGen doesnt like)
    let SubRegs = !listconcat(short.SubRegs, [upper]);
    let SubRegIndices = !listconcat(short.SubRegIndices, [sub_upper]);
    let CoveredBySubRegs = 1;
  }
}

//===----------------------------------------------------------------------===//
//  Register definitions...
//

// 8-bit registers
def A : Z80Reg<"a", 7>;
def F : Z80Reg<"f">;
def B : Z80Reg<"b", 0>;
def C : Z80Reg<"c", 1>;
def D : Z80Reg<"d", 2>;
def E : Z80Reg<"e", 3>;
def H : Z80Reg<"h", 4>;
def L : Z80Reg<"l", 5>;

// 8-bit index registers
let CostPerUse = [1] in {
  def IXH : Z80Reg<"ixh", 4>;
  def IXL : Z80Reg<"ixl", 5>;
  def IYH : Z80Reg<"iyh", 4>;
  def IYL : Z80Reg<"iyl", 5>;
}

// upper bytes for 24 bit registers (ADL mode). these are modeled only for
// correct aliasing/liveness and are not expected to be used directly

def UBCU : Z80Reg<"ubcu">;
def UDEU : Z80Reg<"udeu">;
def UHLU : Z80Reg<"uhlu">;
def UIXU : Z80Reg<"uixu">;
def UIYU : Z80Reg<"uiyu">;

// 16-bit registers
def AF : Z80RegPair<A, F, 3>;
def BC : Z80RegPair<B, C, 0>;
def DE : Z80RegPair<D, E, 1>;
def HL : Z80RegPair<H, L, 2>;

// 16-bit index registers
let CostPerUse = [1] in {
  def IX : Z80RegPair<IXH, IXL, 2, 4>;
  def IY : Z80RegPair<IYH, IYL, 2, 5>;
}


// 24-bit registers
def UBC : EZ80ExtReg<BC, UBCU>;
def UDE : EZ80ExtReg<DE, UDEU>;
def UHL : EZ80ExtReg<HL, UHLU>;
// 24-bit index registers
let CostPerUse = [1] in {
  def UIX : EZ80ExtReg<IX, UIXU>;
  def UIY : EZ80ExtReg<IY, UIYU>;
}

// misc registers
def SPS : Z80Reg<"sp", 3>, DwarfRegNum<[6]>;
def SPL : Z80Reg<"sp", 3>, DwarfRegNum<[7]>;
def PC : Z80Reg<"pc">, DwarfRegNum<[8]>;
def I  : Z80Reg<"i">;
let SubRegs = [I], SubRegIndices = [sub_low] in
def UI : Z80Reg<"i">;
def R  : Z80Reg<"r">;
def MB : Z80Reg<"mb">;

//===----------------------------------------------------------------------===//
//  Register tuples for wide scalar values
//

// Z80 (16 bit) wide integer tuples
def GPR32_16TuplesHLDE : RegisterTuples<[sub_short, sub_word1], [(add HL), (add DE)]>;
def GPR32_16TuplesHLBC : RegisterTuples<[sub_short, sub_word1], [(add HL), (add BC)]>;
def GPR32_16TuplesHLIY : RegisterTuples<[sub_short, sub_word1], [(add HL), (add IY)]>;
def GPR32_16TuplesDEBC : RegisterTuples<[sub_short, sub_word1], [(add DE), (add BC)]>;
def GPR32_16TuplesDEIY : RegisterTuples<[sub_short, sub_word1], [(add DE), (add IY)]>;
def GPR32_16TuplesBCIY : RegisterTuples<[sub_short, sub_word1], [(add BC), (add IY)]>;
def GPR64_16Tuples : RegisterTuples<[sub_short, sub_word1, sub_word2, sub_word3],
                                    [(add HL), (add DE), (add BC), (add IY)],
                                    ["hldebciy"]>;

// eZ80 (24-bit) wide integer tuples
// these are artificial physical tuples used to model wide scalars for
// GlobalISel. they must not overlap in their component registers
//
// R32_24 is used both directly (libcalls) and indirectly via composed
// subreg indices when extracting the low 32 bits of an R48_24/R64_24 value
// provide multiple allocatable tuples so regalloc/coalescing doesnt get stuck
// with singleton derived register classes
def GPR32_24TuplesUHLE : RegisterTuples<[sub_low24, sub_high8], [(add UHL), (add E)]>;
def GPR32_24TuplesUHLC : RegisterTuples<[sub_low24, sub_high8], [(add UHL), (add C)]>;
def GPR32_24TuplesUHLB : RegisterTuples<[sub_low24, sub_high8], [(add UHL), (add B)]>;
def GPR32_24TuplesUHLD : RegisterTuples<[sub_low24, sub_high8], [(add UHL), (add D)]>;
def GPR32_24TuplesUDEL : RegisterTuples<[sub_low24, sub_high8], [(add UDE), (add L)]>;
def GPR32_24TuplesUDEC : RegisterTuples<[sub_low24, sub_high8], [(add UDE), (add C)]>;
def GPR32_24TuplesUDEB : RegisterTuples<[sub_low24, sub_high8], [(add UDE), (add B)]>;
def GPR32_24TuplesUDEH : RegisterTuples<[sub_low24, sub_high8], [(add UDE), (add H)]>;
def GPR32_24TuplesUBCL : RegisterTuples<[sub_low24, sub_high8], [(add UBC), (add L)]>;
def GPR32_24TuplesUBCE : RegisterTuples<[sub_low24, sub_high8], [(add UBC), (add E)]>;
def GPR32_24TuplesUBCD : RegisterTuples<[sub_low24, sub_high8], [(add UBC), (add D)]>;
def GPR32_24TuplesUBCH : RegisterTuples<[sub_low24, sub_high8], [(add UBC), (add H)]>;
def GPR32_24TuplesUIYE : RegisterTuples<[sub_low24, sub_high8], [(add UIY), (add E)]>;
def GPR32_24TuplesUIYC : RegisterTuples<[sub_low24, sub_high8], [(add UIY), (add C)]>;
def GPR32_24TuplesUIYL : RegisterTuples<[sub_low24, sub_high8], [(add UIY), (add L)]>;
def GPR32_24TuplesUIYB : RegisterTuples<[sub_low24, sub_high8], [(add UIY), (add B)]>;
def GPR32_24TuplesUIYD : RegisterTuples<[sub_low24, sub_high8], [(add UIY), (add D)]>;
def GPR32_24TuplesUIYH : RegisterTuples<[sub_low24, sub_high8], [(add UIY), (add H)]>;

// R48_24 is modeled as (low24, mid24) with low24 in G24 and mid24 in Y24

def GPR48_24Tuples : RegisterTuples<[sub_low24, sub_mid24],
                                   [(add UHL, UHL, UHL, UDE, UDE, UBC, UBC),
                                    (add UDE, UBC, UIY, UBC, UIY, UDE, UIY)]>;

// R64_24 is modeled as (low24, mid24, high16). keep the base tuple set small
// and well defined. additional tuples can be added carefully to avoid TableGen
// subreg composition pitfalls
def GPR64_24TuplesBC : RegisterTuples<[sub_low24, sub_mid24, sub_word3],
                                     [(add UHL), (add UDE), (add BC)],
                                     ["uhludebc"]>;
def GPR64_24TuplesDE : RegisterTuples<[sub_low24, sub_mid24, sub_word3],
                                     [(add UHL), (add UBC), (add DE)],
                                     ["uhlubcde"]>;
def GPR64_24TuplesIY : RegisterTuples<[sub_low24, sub_mid24, sub_word3],
                                     [(add UHL), (add UDE), (add IY)],
                                     ["uhludeiy"]>;
// extra tuple with non-UHL low24 to give regalloc a choice when UHL is live
def GPR64_24TuplesAlt0 : RegisterTuples<[sub_low24, sub_mid24, sub_word3],
                                       [(add UDE), (add UIY), (add BC)],
                                       ["udeuiybc"]>;
def GPR64_24TuplesAlt1 : RegisterTuples<[sub_low24, sub_mid24, sub_word3],
                                       [(add UBC), (add UIY), (add DE)],
                                       ["ubcuiyde"]>;
// extra tuples to allow (low24, mid24) combinations without forcing UHL
def GPR64_24TuplesAlt2 : RegisterTuples<[sub_low24, sub_mid24, sub_word3],
                                       [(add UBC), (add UDE), (add HL)],
                                       ["ubcudehl"]>;
def GPR64_24TuplesAlt3 : RegisterTuples<[sub_low24, sub_mid24, sub_word3],
                                       [(add UIY), (add UBC), (add HL)],
                                       ["uiyubchl"]>;
def GPR64_24TuplesAlt4 : RegisterTuples<[sub_low24, sub_mid24, sub_word3],
                                       [(add UIY), (add UDE), (add HL)],
                                       ["uiyudehl"]>;

//===----------------------------------------------------------------------===//
//  Register Class Definitions...
//

class Z80RC8 <dag regList> : RegisterClass<"Z80", [i8 ], 8, regList>;
class Z80RC16<dag regList> : RegisterClass<"Z80", [i16], 8, regList>;
class Z80RC24<dag regList> : RegisterClass<"Z80", [i24], 8, regList>;
class Z80RC32<dag regList> : RegisterClass<"Z80", [i32], 8, regList> {
  let Size = 32;
}
class Z80RC48<dag regList> : RegisterClass<"Z80", [untyped], 8, regList> {
  let Size = 48;
}
class Z80RC64<dag regList> : RegisterClass<"Z80", [i64], 8, regList> {
  let Size = 64;
}

def G8  : Z80RC8 <(add A, L, E, C, H, D, B)>;
// non accumulator 8 bit excludes A for ALU pseudo src2 operand
// prevents RA from assigning A to src2, which would be clobbered when
// copying src1 to A during expandPostRAPseudo

def N8  : Z80RC8 <(add L, E, C, H, D, B)>;
def O8  : Z80RC8 <(add A, E, C, D, B)>;
def Y8  : Z80RC8 <(add O8, IYL, IYH)>;
def X8  : Z80RC8 <(add O8, IXL, IXH)>;
def I8  : Z80RC8 <(add IYL, IYH, IXL, IXH)>;
def R8  : Z80RC8 <(add G8, I8)>;
let CopyCost = -1 in {
  def F8  : Z80RC8 <(add F)>;
  def Z8  : Z80RC8 <(add I, R, MB)>;
}
// upper bytes for 24 bit registers are modeled only for aliasing/liveness and
// must not be allocatable
let CopyCost = -1, isAllocatable = 0 in
def U8 : Z80RC8<(add UBCU, UDEU, UHLU, UIXU, UIYU)>;

def O16 : Z80RC16<(add DE, BC)> {
  // operand class for 16 bit ALU operations excludes HL
  // higher priority helps v17's greedy allocator avoid spilling
  let AllocationPriority = 8;
}
def G16 : Z80RC16<(add HL, O16)>;
def Y16 : Z80RC16<(add IY, O16)>;
def X16 : Z80RC16<(add IX, O16)>;
def I16 : Z80RC16<(add IY, IX)>;
def A16 : Z80RC16<(add HL, I16)> {
  let AllocationPriority = 16;
  let GlobalPriority = true;
}
def HL16 : Z80RC16<(add HL)>;
def HL24 : Z80RC24<(add UHL)>;
def R16 : Z80RC16<(add G16, I16)>;
let CopyCost = -1 in
def Z16 : Z80RC16<(add SPS, AF, UI)>;

def O24 : Z80RC24<(add UDE, UBC)> {
  let AllocationPriority = 8;
}
def G24 : Z80RC24<(add UHL, O24)>;
def Y24 : Z80RC24<(add UIY, O24)>;
def X24 : Z80RC24<(add UIX, O24)>;
def I24 : Z80RC24<(add UIY, UIX)>;
def A24 : Z80RC24<(add UHL, I24)> {
  let AllocationPriority = 16;
  let GlobalPriority = true;
}
def R24 : Z80RC24<(add G24, I24)>;
let CopyCost = -1 in
def Z24 : Z80RC24<(add SPL, PC)>;

def R32_16 : Z80RC32<(add GPR32_16TuplesHLDE, GPR32_16TuplesHLBC, GPR32_16TuplesHLIY,
                          GPR32_16TuplesDEBC, GPR32_16TuplesDEIY, GPR32_16TuplesBCIY)>;
def R32_24 : Z80RC32<(add GPR32_24TuplesUHLE, GPR32_24TuplesUHLC,
                          GPR32_24TuplesUHLB, GPR32_24TuplesUHLD,
                          GPR32_24TuplesUDEL, GPR32_24TuplesUDEC,
                          GPR32_24TuplesUDEB, GPR32_24TuplesUDEH,
                          GPR32_24TuplesUBCL, GPR32_24TuplesUBCE,
                          GPR32_24TuplesUBCD, GPR32_24TuplesUBCH,
                          GPR32_24TuplesUIYE, GPR32_24TuplesUIYC, GPR32_24TuplesUIYL,
                          GPR32_24TuplesUIYB, GPR32_24TuplesUIYD, GPR32_24TuplesUIYH)>;
def R48_24 : Z80RC48<(add GPR48_24Tuples)>;
def R64_16 : Z80RC64<(add GPR64_16Tuples)>;
def R64_24 : Z80RC64<(add GPR64_24TuplesBC, GPR64_24TuplesDE, GPR64_24TuplesIY,
                          GPR64_24TuplesAlt0, GPR64_24TuplesAlt1,
                          GPR64_24TuplesAlt2, GPR64_24TuplesAlt3,
                          GPR64_24TuplesAlt4)>;
