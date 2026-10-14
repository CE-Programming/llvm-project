; NOTE: Do not autogenerate
; RUN: llc -mtriple=ez80 -O2 -enable-machine-outliner=never -z80-frame-bias-policy=fixed -z80-frame-bias=36 -o - %s | FileCheck %s --check-prefix=B36
; RUN: llc -mtriple=ez80 -O2 -enable-machine-outliner=never -z80-frame-bias-policy=fixed -z80-frame-bias=-37 -o - %s | FileCheck %s --check-prefix=NEG
; RUN: llc -mtriple=ez80 -O2 -enable-machine-outliner=never -z80-frame-bias-policy=fixed -z80-frame-bias=300 -o - %s | FileCheck %s --check-prefix=LARGE
; RUN: llc -mtriple=ez80 -O2 -enable-machine-outliner=never -z80-frame-bias-policy=fixed -z80-frame-bias=-300 -o - %s | FileCheck %s --check-prefix=LARGENEG
; RUN: llc -mtriple=ez80 -O2 -enable-machine-outliner=never -z80-frame-bias-policy=off -o - %s | FileCheck %s --check-prefix=ZERO
; RUN: llc -mtriple=ez80 -O0 -enable-machine-outliner=never -z80-frame-bias-policy=fixed -z80-frame-bias=36 -o - %s | FileCheck %s --check-prefix=O0
; RUN: llc -mtriple=ez80 -O2 -enable-machine-outliner=never -z80-frame-bias-policy=fixed -z80-frame-bias=36 -stop-after=prologepilog -verify-machineinstrs -o /dev/null %s
; RUN: llc -mtriple=ez80 -O2 -enable-machine-outliner=never -z80-frame-bias-policy=fixed -z80-frame-bias=300 -stop-after=prologepilog -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=MIRPOS
; RUN: llc -mtriple=ez80 -O2 -enable-machine-outliner=never -z80-frame-bias-policy=fixed -z80-frame-bias=-300 -stop-after=prologepilog -verify-machineinstrs -o - %s | FileCheck %s --check-prefix=MIRNEG
;
; A variable-sized allocation makes the current SP unrelated to the fixed
; frame size. Recover the original frame base, including positive/negative
; bias, without pushing temporaries onto that unknown stack. The word return
; stays in HL, so use a separate index scratch at its epilogue. An empty
; fixed frame still requires restoration. Very large bias must use legal
; index-register LEA chains. Verify through frame insertion because the
; existing post-RA OR expansion has a separate implicit-A verifier issue.

; B36-LABEL: _dynamic_word:
; B36: lea ix, ix - 36
; B36: lea iy, ix + 36
; B36-NEXT: ld sp, iy
; B36-NEXT: pop ix
; B36-NEXT: ret
; NEG-LABEL: _dynamic_word:
; NEG: lea ix, ix + 37
; NEG: lea iy, ix - 37
; NEG-NEXT: ld sp, iy
; NEG-NEXT: pop ix
; NEG-NEXT: ret
; LARGE-LABEL: _dynamic_word:
; LARGE: lea iy, ix + 46
; LARGE-NEXT: ld sp, iy
; LARGE-NEXT: pop ix
; LARGE-NEXT: ret
; LARGENEG-LABEL: _dynamic_word:
; LARGENEG: lea iy, ix - 44
; LARGENEG-NEXT: ld sp, iy
; LARGENEG-NEXT: pop ix
; LARGENEG-NEXT: ret
; ZERO-LABEL: _dynamic_word:
; ZERO: ld sp, ix
; ZERO-NEXT: pop ix
; ZERO-NEXT: ret
; O0-LABEL: _dynamic_word:
; O0: lea [[RESTORE:[a-z]+]], ix + 36
; O0-NEXT: ld sp, [[RESTORE]]
; O0-NEXT: pop ix
; O0-NEXT: ret

; MIRPOS-LABEL: name: dynamic_word
; MIRPOS: $uix = frame-destroy LEA24ro $uix, 127
; MIRPOS-NEXT: $uix = frame-destroy LEA24ro $uix, 127
; MIRPOS-NEXT: $uiy = frame-destroy LEA24ro $uix, 46
; MIRPOS-NEXT: frame-destroy LD24sa killed $uiy
; MIRNEG-LABEL: name: dynamic_word
; MIRNEG: $uix = frame-destroy LEA24ro $uix, -128
; MIRNEG-NEXT: $uix = frame-destroy LEA24ro $uix, -128
; MIRNEG-NEXT: $uiy = frame-destroy LEA24ro $uix, -44
; MIRNEG-NEXT: frame-destroy LD24sa killed $uiy

define i24 @dynamic_word(i24 %n, i24 %salt, ptr %out) minsize optsize nounwind {
  %fixed = alloca [160 x i8], align 1
  store volatile i8 85, ptr %fixed, align 1
  %fixedlast = getelementptr [160 x i8], ptr %fixed, i24 0, i24 159
  store volatile i8 -86, ptr %fixedlast, align 1
  %vla = alloca i8, i24 %n, align 1
  %lo = trunc i24 %salt to i8
  %hi = xor i8 %lo, -1
  store volatile i8 %lo, ptr %vla, align 1
  %index = sub i24 %n, 1
  %last = getelementptr i8, ptr %vla, i24 %index
  store volatile i8 %hi, ptr %last, align 1
  %f0 = load volatile i8, ptr %fixed, align 1
  %f1 = load volatile i8, ptr %fixedlast, align 1
  %v0 = load volatile i8, ptr %vla, align 1
  %v1 = load volatile i8, ptr %last, align 1
  store i8 %f0, ptr %out, align 1
  %out1 = getelementptr i8, ptr %out, i24 1
  store i8 %f1, ptr %out1, align 1
  %out2 = getelementptr i8, ptr %out, i24 2
  store i8 %v0, ptr %out2, align 1
  %out3 = getelementptr i8, ptr %out, i24 3
  store i8 %v1, ptr %out3, align 1
  %addr = ptrtoint ptr %vla to i24
  %out4 = getelementptr i8, ptr %out, i24 4
  store i24 %addr, ptr %out4, align 1
  %result = add i24 %n, %salt
  ret i24 %result
}

; B36-LABEL: _dynamic_byte:
; B36: lea ix, ix - 36
; B36: lea hl, ix + 36
; B36-NEXT: ld sp, hl
; B36-NEXT: pop ix
; B36-NEXT: ret
; NEG-LABEL: _dynamic_byte:
; NEG: lea ix, ix + 37
; NEG: lea hl, ix - 37
; NEG-NEXT: ld sp, hl
; NEG-NEXT: pop ix
; NEG-NEXT: ret
; LARGE-LABEL: _dynamic_byte:
; LARGE: lea hl, ix + 46
; LARGE-NEXT: ld sp, hl
; LARGE-NEXT: pop ix
; LARGE-NEXT: ret
; LARGENEG-LABEL: _dynamic_byte:
; LARGENEG: lea hl, ix - 44
; LARGENEG-NEXT: ld sp, hl
; LARGENEG-NEXT: pop ix
; LARGENEG-NEXT: ret
; ZERO-LABEL: _dynamic_byte:
; ZERO: ld sp, ix
; ZERO-NEXT: pop ix
; ZERO-NEXT: ret
; O0-LABEL: _dynamic_byte:
; O0: lea [[RESTORE:[a-z]+]], ix + 36
; O0-NEXT: ld sp, [[RESTORE]]
; O0-NEXT: pop ix
; O0-NEXT: ret

; MIRPOS-LABEL: name: dynamic_byte
; MIRPOS: $uix = frame-destroy LEA24ro $uix, 127
; MIRPOS-NEXT: $uix = frame-destroy LEA24ro $uix, 127
; MIRPOS-NEXT: $uhl = frame-destroy LEA24ro $uix, 46
; MIRPOS-NEXT: frame-destroy LD24sa killed $uhl
; MIRNEG-LABEL: name: dynamic_byte
; MIRNEG: $uix = frame-destroy LEA24ro $uix, -128
; MIRNEG-NEXT: $uix = frame-destroy LEA24ro $uix, -128
; MIRNEG-NEXT: $uhl = frame-destroy LEA24ro $uix, -44
; MIRNEG-NEXT: frame-destroy LD24sa killed $uhl

define i8 @dynamic_byte(i24 %n, i8 %salt, ptr %out) minsize optsize nounwind {
  %fixed = alloca [192 x i8], align 1
  store volatile i8 90, ptr %fixed, align 1
  %fixedlast = getelementptr [192 x i8], ptr %fixed, i24 0, i24 191
  store volatile i8 -91, ptr %fixedlast, align 1
  %vla = alloca i8, i24 %n, align 1
  store volatile i8 %salt, ptr %vla, align 1
  %index = sub i24 %n, 1
  %last = getelementptr i8, ptr %vla, i24 %index
  %other = xor i8 %salt, -1
  store volatile i8 %other, ptr %last, align 1
  %a = load volatile i8, ptr %fixed, align 1
  %b = load volatile i8, ptr %fixedlast, align 1
  store i8 %a, ptr %out, align 1
  %out1 = getelementptr i8, ptr %out, i24 1
  store i8 %b, ptr %out1, align 1
  %v = load volatile i8, ptr %vla, align 1
  %result = add i8 %salt, %v
  ret i8 %result
}

; B36-LABEL: _dynamic_only:
; B36: lea ix, ix - 36
; B36: lea iy, ix + 36
; B36-NEXT: ld sp, iy
; B36-NEXT: pop ix
; B36-NEXT: ret
; NEG-LABEL: _dynamic_only:
; NEG: lea ix, ix + 37
; NEG: lea iy, ix - 37
; NEG-NEXT: ld sp, iy
; NEG-NEXT: pop ix
; NEG-NEXT: ret
; LARGE-LABEL: _dynamic_only:
; LARGE: lea iy, ix + 46
; LARGE-NEXT: ld sp, iy
; LARGE-NEXT: pop ix
; LARGE-NEXT: ret
; LARGENEG-LABEL: _dynamic_only:
; LARGENEG: lea iy, ix - 44
; LARGENEG-NEXT: ld sp, iy
; LARGENEG-NEXT: pop ix
; LARGENEG-NEXT: ret
; ZERO-LABEL: _dynamic_only:
; ZERO: ld sp, ix
; ZERO-NEXT: pop ix
; ZERO-NEXT: ret
; O0-LABEL: _dynamic_only:
; O0: lea [[RESTORE:[a-z]+]], ix + 36
; O0-NEXT: ld sp, [[RESTORE]]
; O0-NEXT: pop ix
; O0-NEXT: ret

; MIRPOS-LABEL: name: dynamic_only
; MIRPOS: $uix = frame-destroy LEA24ro $uix, 127
; MIRPOS-NEXT: $uix = frame-destroy LEA24ro $uix, 127
; MIRPOS-NEXT: $uiy = frame-destroy LEA24ro $uix, 46
; MIRPOS-NEXT: frame-destroy LD24sa killed $uiy
; MIRNEG-LABEL: name: dynamic_only
; MIRNEG: $uix = frame-destroy LEA24ro $uix, -128
; MIRNEG-NEXT: $uix = frame-destroy LEA24ro $uix, -128
; MIRNEG-NEXT: $uiy = frame-destroy LEA24ro $uix, -44
; MIRNEG-NEXT: frame-destroy LD24sa killed $uiy

define i24 @dynamic_only(i24 %n) minsize optsize nounwind {
  %vla = alloca i8, i24 %n, align 1
  store volatile i8 -91, ptr %vla, align 1
  %v = load volatile i8, ptr %vla, align 1
  %result = zext i8 %v to i24
  ret i24 %result
}

; Like the graph menu: the conditional allocation has a constant element
; count but occurs outside entry under stacksave/stackrestore. The function
; still has variable-stack frame state when this branch is skipped on exit.
declare ptr @llvm.stacksave.p0()
declare void @llvm.stackrestore.p0(ptr)

; B36-LABEL: _menu_scope:
; B36: lea iy, ix + 36
; B36-NEXT: ld sp, iy
; B36-NEXT: pop ix
; B36-NEXT: ret
; NEG-LABEL: _menu_scope:
; NEG: lea iy, ix - 37
; NEG-NEXT: ld sp, iy
; NEG-NEXT: pop ix
; NEG-NEXT: ret
; LARGE-LABEL: _menu_scope:
; LARGE: lea iy, ix + 46
; LARGE-NEXT: ld sp, iy
; LARGE-NEXT: pop ix
; LARGE-NEXT: ret
; LARGENEG-LABEL: _menu_scope:
; LARGENEG: lea iy, ix - 44
; LARGENEG-NEXT: ld sp, iy
; LARGENEG-NEXT: pop ix
; LARGENEG-NEXT: ret
; ZERO-LABEL: _menu_scope:
; ZERO: ld sp, ix
; ZERO-NEXT: pop ix
; ZERO-NEXT: ret
; O0-LABEL: _menu_scope:
; O0: lea [[RESTORE:[a-z]+]], ix + 36
; O0-NEXT: ld sp, [[RESTORE]]
; O0-NEXT: pop ix
; O0-NEXT: ret
; MIRPOS-LABEL: name: menu_scope
; MIRPOS: $uix = frame-destroy LEA24ro $uix, 127
; MIRPOS-NEXT: $uix = frame-destroy LEA24ro $uix, 127
; MIRPOS-NEXT: $uiy = frame-destroy LEA24ro $uix, 46
; MIRPOS-NEXT: frame-destroy LD24sa killed $uiy
; MIRNEG-LABEL: name: menu_scope
; MIRNEG: $uix = frame-destroy LEA24ro $uix, -128
; MIRNEG-NEXT: $uix = frame-destroy LEA24ro $uix, -128
; MIRNEG-NEXT: $uiy = frame-destroy LEA24ro $uix, -44
; MIRNEG-NEXT: frame-destroy LD24sa killed $uiy

define i24 @menu_scope(i24 %n, i24 %salt, ptr %out) minsize optsize nounwind {
  %fixed = alloca [160 x i8], align 1
  store volatile i8 85, ptr %fixed, align 1
  %fixedlast = getelementptr [160 x i8], ptr %fixed, i24 0, i24 159
  store volatile i8 -86, ptr %fixedlast, align 1
  %bit = and i24 %n, 1
  %take = icmp ne i24 %bit, 0
  br i1 %take, label %menu, label %done
menu:
  %saved = call ptr @llvm.stacksave.p0()
  %items = alloca [15 x i24], align 1
  store volatile i24 %salt, ptr %items, align 1
  %last = getelementptr [15 x i24], ptr %items, i24 0, i24 14
  store volatile i24 %n, ptr %last, align 1
  %a = load volatile i24, ptr %items, align 1
  %b = load volatile i24, ptr %last, align 1
  %sum = add i24 %a, %b
  call void @llvm.stackrestore.p0(ptr %saved)
  br label %done
done:
  %result = phi i24 [ %salt, %0 ], [ %sum, %menu ]
  %f0 = load volatile i8, ptr %fixed, align 1
  %f1 = load volatile i8, ptr %fixedlast, align 1
  store i8 %f0, ptr %out, align 1
  %out1 = getelementptr i8, ptr %out, i24 1
  store i8 %f1, ptr %out1, align 1
  ret i24 %result
}
