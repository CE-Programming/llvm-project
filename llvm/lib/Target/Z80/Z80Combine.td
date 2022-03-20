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

def Z80PreLegalizerCombinerHelper
    : GICombinerHelper<"Z80GenPreLegalizerCombinerHelper", [
        and_ext, and_or_disjoint_mask, combine_ext_or_trunc, combine_identity,
        combine_trunc_shift, constant_fold, const_combines, div_rem_to_divrem,
        ext_ext_fold, fabs_fabs_fold, fabs_fneg_fold, flip_cmp_cond,
        flip_setcc_cond, form_bitfield_extract, funnel_shift,
        funnel_shift_combines, hoist_logic_op_with_same_opcode_hands,
        identity_combines, intdiv_combines, known_bits_simplifications,
        load_or_combine, lower_is_power_of_two, merge_unmerge, mulh_combines,
        narrow_count_zext, narrow_icmp, narrow_load, narrow_op, not_cmp_fold,
        opt_brcond_by_inverting_cond, or_to_add, phi_combines,
        ptr_add_const_immed, ptr_add_global_immed, ptr_add_immed_chain,
        ptr_add_with_zero, reassocs, reassoc_fold_consts,
        redundant_neg_operands, select_combines, sext_to_zext,
        shift_immed_chain, shift_of_shifted_logic_chain, shl_ashr_to_sext_inreg,
        simplify_add_to_sub, simplify_const, simplify_icmp_bool,
        simplify_known_const, split_brcond, sub_const_to_add_neg,
        trivial_combines, truncstore_merge, trunc_ext_fold, trunc_shl,
        undef_combines, unmerge_cst, unmerge_dead_to_trunc, unmerge_merge,
        unmerge_zext_to_zext, width_reduction_combines,
        xor_of_and_with_same_reg,
    ]> {
  let DisableRuleOption = "z80prelegalizercombiner-disable-rule";
}

def Z80PostLegalizerCombinerHelper
    : GICombinerHelper<"Z80GenPostLegalizerCombinerHelper", [
        combine_identity, narrow_op, simplify_known_const, trunc_ext_fold,
        unmerge_zext_to_zext,
    ]> {
  let DisableRuleOption = "z80postlegalizercombiner-disable-rule";
}
