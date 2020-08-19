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

def Z80PreLegalizerCombinerHelper
    : GICombinerHelper<"Z80GenPreLegalizerCombinerHelper", [
        combine_identity, flip_condition, funnel_shift, lower_is_power_of_two,
        narrow_compare, narrow_op, opt_brcond_by_inverting_cond, or_to_add,
        ptr_add_const_immed, ptr_add_global_immed, ptr_add_immed_chain,
        sext_to_zext, sink_const, split_conditions, trivial_combines,
        undef_combines,
    ]> {
  let DisableRuleOption = "z80prelegalizercombiner-disable-rule";
}

def Z80PostLegalizerCombinerHelper
    : GICombinerHelper<"Z80GenPostLegalizerCombinerHelper", [
        narrow_op
    ]> {
  let DisableRuleOption = "z80postlegalizercombiner-disable-rule";
}
