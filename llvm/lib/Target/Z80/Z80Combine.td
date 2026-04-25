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

// identity combines (excluding FP related fneg_fneg_fold)
def z80_identity_combines : GICombineGroup<[select_same_val, right_identity_zero,
                                            binop_same_val, binop_left_to_zero,
                                            binop_right_to_zero, p2i_to_i2p,
                                            i2p_to_p2i, anyext_trunc_fold,
                                            right_identity_one,
                                            add_sub_reg, buildvector_identity_fold,
                                            trunc_buildvector_fold,
                                            trunc_lshr_buildvector_fold,
                                            bitcast_bitcast_fold]>;

// const combines (excluding FP related constant_fp_op, combine_minmax_nan)
def z80_const_combines : GICombineGroup<[const_ptradd_to_i2p,
                                         overlapping_and, mulo_by_2, mulo_by_0,
                                         adde_to_addo]>;

def Z80PreLegalizerCombiner
    : GICombiner<"Z80PreLegalizerCombinerImpl", [
        and_ext, and_or_disjoint_mask, combine_ext_or_trunc, combine_identity,
        combine_trunc_shift, constant_fold, z80_const_combines, div_rem_to_divrem,
        ext_ext_fold, flip_cmp_cond,
        flip_setcc_cond, form_bitfield_extract, funnel_shift,
        funnel_shift_combines, hoist_logic_op_with_same_opcode_hands,
        z80_identity_combines, intdiv_combines, known_bits_simplifications,
        load_or_combine, lower_is_power_of_two, merge_unmerge, mulh_combines,
        narrow_count_zext, narrow_icmp, narrow_load, narrow_op, not_cmp_fold,
        opt_brcond_by_inverting_cond, or_to_add, phi_combines,
        ptr_add_const_immed, ptr_add_global_immed, ptr_add_immed_chain,
        ptr_add_with_zero, reassocs, reassoc_fold_consts,
        select_combines, sext_to_zext,
        shift_immed_chain, shift_of_shifted_logic_chain, shl_ashr_to_sext_inreg,
        simplify_add_to_sub, simplify_const, simplify_icmp_bool,
        simplify_known_const, split_brcond, sub_const_to_add_neg,
        trivial_combines, trunc_ext_fold,
        undef_combines, unmerge_cst, unmerge_dead_to_trunc, unmerge_merge,
        unmerge_zext_to_zext, width_reduction_combines,
        xor_of_and_with_same_reg,
    ]>;

def Z80PostLegalizerCombiner
    : GICombiner<"Z80PostLegalizerCombinerImpl", [
        combine_identity, narrow_op, select_same_val, simplify_known_const,
        trunc_ext_fold, unmerge_zext_to_zext,
    ]>;
