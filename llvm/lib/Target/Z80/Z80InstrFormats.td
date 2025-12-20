//===-- Z80InstrFormats.td - Z80 Instruction Formats -------*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Z80 Instruction Format Definitions.
//

class Mode<bits<2> val> {
  bits<2> Value = val;
}
def AnyMode  : Mode<0>;
def CurMode  : Mode<1>;
def Z80Mode  : Mode<2>;
def EZ80Mode : Mode<3>;

class Prefix {
  bits<4> Value = 0;
}

class IndexPrefix<bit index0, bit index1> : Prefix {
  let Value{0-1} = {index0, index1};
}
def NoIdxPre : IndexPrefix<false, false>;
def  Idx0Pre : IndexPrefix< true, false>;
def  Idx1Pre : IndexPrefix<false,  true>;
def Idx01Pre : IndexPrefix< true,  true>;

class OtherPrefix<bit cb, bit ed> : Prefix {
  let Value{2-3} = {cb, ed};
}
def NoPre : OtherPrefix<false, false>;
def CBPre : OtherPrefix< true, false>;
def EDPre : OtherPrefix<false,  true>;

class Pre<IndexPrefix index = NoIdxPre, OtherPrefix other = NoPre> : Prefix {
  let Value{0-3} = {index.Value{0-1}, other.Value{2-3}};
}

class ImmType<bits<2> val> {
  bits<2> Value = val;
}
def NoImm  : ImmType<0>;
def Imm    : ImmType<1>;
def Off    : ImmType<2>;
def OffImm : ImmType<3>;

class Z80Inst<Mode mode, Prefix prefix, bits<8> opcode, ImmType immediate,
              dag outputs = (outs), dag inputs = (ins), list<dag> pattern = [],
              string asm = "", string constraints = "">
  : Instruction {
  let Namespace = "Z80";

  bits<8> Opcode = opcode;
  bit hasImm = immediate.Value{0};
  bit hasOff = immediate.Value{1};

  let OutOperandList = outputs;
  let InOperandList = inputs;
  let Pattern = pattern;
  let AsmString = asm;
  let Constraints = constraints;

  let TSFlags{1-0} = mode.Value;
  let TSFlags{5-2} = prefix.Value;
  let TSFlags{7-6} = immediate.Value;
  let TSFlags{15-8} = opcode;

  let hasSideEffects = false;
  let isCodeGenOnly = true;
}

let isPseudo = true in
class Pseudo<string mnem, string args = "", string con = "",
             dag outs = (outs), dag ins = (ins), list<dag> pattern = []>
  : Z80Inst< AnyMode, NoPre, 0, NoImm,  outs, ins, pattern,
            !strconcat(mnem,            args), con>;

class Inst  <Prefix prefix, bits<8> opcode, ImmType immediate,
             string mnemonic, string arguments = "", string constraints = "",
             dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Z80Inst< AnyMode, prefix, opcode, immediate, outputs, inputs, pattern,
            !strconcat(mnemonic,            arguments), constraints>;
class Inst8 <Prefix prefix, bits<8> opcode, ImmType immediate,
             string mnemonic, string arguments = "", string constraints = "",
             dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Z80Inst< CurMode, prefix, opcode, immediate, outputs, inputs, pattern,
            !strconcat(mnemonic,            arguments), constraints>;
class Inst16<Prefix prefix, bits<8> opcode, ImmType immediate,
             string mnemonic, string arguments = "", string constraints = "",
             dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Z80Inst< Z80Mode, prefix, opcode, immediate, outputs, inputs, pattern,
            !strconcat(mnemonic, "{|.sis}", arguments), constraints>;
class Inst24<Prefix prefix, bits<8> opcode, ImmType immediate,
             string mnemonic, string arguments = "", string constraints = "",
             dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Z80Inst<EZ80Mode, prefix, opcode, immediate, outputs, inputs, pattern,
            !strconcat(mnemonic, "{.lil|}", arguments), constraints>,
    Requires<[HaveEZ80Ops]>;

class I    <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst  <prefix, opcode,  NoImm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class Ii   <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst  <prefix, opcode,    Imm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class Io   <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst  <prefix, opcode, Off   , mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class Ioi  <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst  <prefix, opcode, OffImm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;

class I8i  <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst8 <prefix, opcode,    Imm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class I8oi <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst8 <prefix, opcode, OffImm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;

class I16  <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst16<prefix, opcode,  NoImm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class I16i <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst16<prefix, opcode,    Imm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class I16o <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst16<prefix, opcode, Off   , mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class I16oi<Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst16<prefix, opcode, OffImm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;

class I24  <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst24<prefix, opcode,  NoImm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class I24i <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst24<prefix, opcode,    Imm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class I24o <Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst24<prefix, opcode, Off   , mnemonic, arguments, constraints,
           outputs, inputs, pattern>;
class I24oi<Prefix prefix, bits<8> opcode,
            string mnemonic, string arguments = "", string constraints = "",
            dag outputs = (outs), dag inputs = (ins), list<dag> pattern = []>
  : Inst24<prefix, opcode, OffImm, mnemonic, arguments, constraints,
           outputs, inputs, pattern>;

class P<dag outs = (outs), dag ins = (ins), list<dag> pattern = []>
  : Z80Inst<AnyMode, NoPre, 0, NoImm, outs, ins, pattern> {
  let isPseudo = true;
}
