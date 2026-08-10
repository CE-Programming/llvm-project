//=- Z80.td - Define Z80 Combine Rules -----------------------*- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

include "llvm/Target/GlobalISel/Combine.td"

def combine_trunc_shift_matchdata : GIDefMatchData<"Register">;
def combine_trunc_shift : GICombineRule<
  (defs root:$mi, combine_trunc_shift_matchdata:$matchinfo),
  (match (wip_match_opcode G_TRUNC):$mi,
         [{ return matchCombineTruncShift(*${mi}, MRI, ${matchinfo}); }]),
  (apply [{ applyCombineTruncShift(*${mi}, B, Observer, ${matchinfo}); }])>;

def flip_setcc_matchdata : GIDefMatchData<"MachineInstr *">;
def flip_setcc_cond : GICombineRule<
  (defs root:$mi, flip_setcc_matchdata:$matchinfo),
  (match (wip_match_opcode G_XOR):$mi,
         [{ return matchFlipSetCCCond(*${mi}, MRI, ${matchinfo}); }]),
  (apply [{ applyFlipSetCCCond(*${mi}, B, Observer, *${matchinfo}); }])>;

// fp combines do not apply to Z80
def z80_identity_combines : GICombineGroup<
    !listremove(identity_combines.Rules,
                [fneg_fneg_fold, fptrunc_fpext_fold,
                 right_identity_neg_zero_fp, right_identity_neg_one_fp])>;

def z80_const_combines : GICombineGroup<
    !listremove(const_combines.Rules,
                [constant_fold_fp_ops, combine_minmax_nan,
                 expand_const_fpowi])>;

def z80_select_all_ones : GICombineRule<
  (defs root:$root, build_fn_matchinfo:$matchinfo),
  (match (wip_match_opcode G_SELECT):$root,
         [{ return matchSelectAllOnes(*${root}, ${matchinfo}); }]),
  (apply [{ Helper.applyBuildFn(*${root}, ${matchinfo}); }])>;

def z80_intdiv_combines : GICombineGroup<
    !listremove(intdiv_combines.Rules, [sdiv_by_pow2])>;

def z80_copy_prop : GICombineRule<
  (defs root:$d),
  (match (COPY $d, $s):$mi, [{
    if (!Helper.matchCombineCopy(*${mi}))
      return false;
    MachineInstr *SrcDef = MRI.getVRegDef(${s}.getReg());
    return !SrcDef || (SrcDef->getOpcode() != TargetOpcode::G_CONSTANT &&
                       SrcDef->getOpcode() != TargetOpcode::G_IMPLICIT_DEF);
  }]),
  (apply [{ Helper.applyCombineCopy(*${mi}); }])>;

def z80_extending_loads : GICombineRule<
  (defs root:$root, extending_load_matchdata:$matchinfo),
  (match (wip_match_opcode G_LOAD, G_SEXTLOAD, G_ZEXTLOAD):$root, [{
    Register Dst = ${root}->getOperand(0).getReg();
    MachineInstr *PtrDef =
        MRI.getVRegDef(${root}->getOperand(1).getReg());
    return (MRI.hasOneNonDBGUser(Dst) ||
            !MRI.hasAtMostUserInstrs(Dst, 2) ||
            MRI.getType(Dst).getScalarSizeInBits() == 8 || !PtrDef ||
            PtrDef->getOpcode() != TargetOpcode::G_FRAME_INDEX) &&
           Helper.matchCombineExtendingLoads(*${root}, ${matchinfo});
  }]),
  (apply [{
    Helper.applyCombineExtendingLoads(*${root}, ${matchinfo});
  }])>;

def z80_mul_by_3 : GICombineRule<
  (defs root:$dst),
  (match (G_MUL $dst, $x, 3),
         [{ return MRI.getType(${dst}.getReg()) == LLT::scalar(24); }]),
  (apply (G_ADD $twice, $x, $x),
         (G_ADD $dst, $twice, $x))>;

def Z80PreLegalizerCombiner
    : GICombiner<"Z80PreLegalizerCombinerImpl", [
        and_ext, and_or_disjoint_mask, cast_combines, combine_ext_or_trunc,
        combine_identity,
        combine_trunc_shift, constant_fold, z80_const_combines, div_rem_to_divrem,
        flip_cmp_cond,
        flip_setcc_cond, form_bitfield_extract, funnel_shift,
        funnel_shift_combines, hoist_logic_op_with_same_opcode_hands,
        z80_identity_combines, z80_intdiv_combines, known_bits_simplifications,
        load_or_combine, lower_is_power_of_two, merge_of_x_and_undef,
        merge_of_x_and_zero, merge_unmerge, mulh_combines, z80_mul_by_3,
        narrow_count_zext, narrow_icmp, narrow_load, narrow_op, not_cmp_fold,
        opt_brcond_by_inverting_cond, or_to_add, overflow_combines, phi_combines,
        ptr_add_const_immed, ptr_add_global_immed, ptr_add_immed_chain,
        select_combines, sext_to_zext,
        shift_immed_chain, shift_of_shifted_logic_chain, shl_ashr_to_sext_inreg,
        simplify_add_to_sub, simplify_const, simplify_icmp_bool,
        simplify_known_const, split_brcond, sub_const_to_add_neg,
        trivial_combines,
        undef_combines, unmerge_cst, unmerge_dead_to_trunc, unmerge_merge,
        unmerge_zext_to_zext, width_reduction_combines,
        xor_of_and_with_same_reg,
    ]>;

def Z80PostLegalizerCombiner
    : GICombiner<"Z80PostLegalizerCombinerImpl", [
        cast_of_cast_combines, combine_identity, narrow_op, select_same_val,
        simplify_known_const, unmerge_zext_to_zext,
        ptr_add_immed_chain, sink_const, z80_copy_prop, z80_extending_loads,
        z80_select_all_ones,
    ]>;
