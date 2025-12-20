//=== Z80CallingConv.cpp - Z80 Custom Calling Convention Impl   -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of custom routines for the Z80
// Calling Convention that aren't done by tablegen.
//
//===----------------------------------------------------------------------===//

#include "Z80CallingConv.h"
#include "Z80Subtarget.h"
using namespace llvm;

// Provides entry points of CC_Z80 and RetCC_Z80.
#include "Z80GenCallingConv.inc"
