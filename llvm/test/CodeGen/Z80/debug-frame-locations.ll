; RUN: llc -mtriple=ez80 -O0 -z80-gas-style -verify-machineinstrs < %s | FileCheck %s --check-prefixes=ADL,DWARF
; RUN: llc -mtriple=z80 -O0 -z80-gas-style -verify-machineinstrs < %s | FileCheck %s --check-prefixes=Z80,DWARF
;
; Native instructions and DWARF must agree on the frame register and offset.
; ADL-LABEL: _ix_frame:
; ADL: ld (ix - 3), hl
; ADL-LABEL: _iy_frame:
; ADL: ld (iy - 3), hl
; Z80-LABEL: _ix_frame:
; Z80: ld (ix - 7), l
; Z80-LABEL: _iy_frame:
; Z80: ld (iy - 3), l
; DWARF: db 1{{ *}}; DW_AT_frame_base
; DWARF-NEXT: db 86
; DWARF: db 2{{ *}}; DW_AT_location
; DWARF-NEXT: db 145
; ADL-NEXT: db 125
; Z80-NEXT: db 121
; DWARF: db 1{{ *}}; DW_AT_frame_base
; DWARF-NEXT: db 87
; DWARF: db 2{{ *}}; DW_AT_location
; DWARF-NEXT: db 145
; DWARF-NEXT: db 125

; Stack locations must use the same IX/IY base and displacement as code.
source_filename = "debug-frames.c"

declare void @llvm.dbg.declare(metadata, metadata, metadata)
declare void @sink(ptr)

define i24 @ix_frame(i24 %x) #0 !dbg !5 {
entry:
  %local = alloca i24, align 1
  call void @llvm.dbg.declare(metadata ptr %local, metadata !8, metadata !DIExpression()), !dbg !9
  store volatile i24 %x, ptr %local, align 1, !dbg !9
  call void @sink(ptr %local), !dbg !10
  %result = load volatile i24, ptr %local, align 1, !dbg !10
  ret i24 %result, !dbg !10
}

define i24 @iy_frame(i24 %x) #1 !dbg !11 {
entry:
  %local = alloca i24, align 1
  call void @llvm.dbg.declare(metadata ptr %local, metadata !12, metadata !DIExpression()), !dbg !13
  store volatile i24 %x, ptr %local, align 1, !dbg !13
  %result = load volatile i24, ptr %local, align 1, !dbg !13
  ret i24 %result, !dbg !13
}

attributes #0 = { noinline optnone "frame-pointer"="all" }
attributes #1 = { noinline "frame-pointer"="none" }
!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}
!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "CE", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "debug-frames.c", directory: "/")
!2 = !{i32 2, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !DIBasicType(name: "unsigned", size: 24, encoding: DW_ATE_unsigned)
!5 = distinct !DISubprogram(name: "ix_frame", scope: !1, file: !1, line: 1, type: !6, scopeLine: 1, spFlags: DISPFlagDefinition, unit: !0)
!6 = !DISubroutineType(types: !7)
!7 = !{!4, !4}
!8 = !DILocalVariable(name: "local_ix", scope: !5, file: !1, line: 2, type: !4)
!9 = !DILocation(line: 2, column: 1, scope: !5)
!10 = !DILocation(line: 3, column: 1, scope: !5)
!11 = distinct !DISubprogram(name: "iy_frame", scope: !1, file: !1, line: 5, type: !6, scopeLine: 5, spFlags: DISPFlagDefinition, unit: !0)
!12 = !DILocalVariable(name: "local_iy", scope: !11, file: !1, line: 6, type: !4)
!13 = !DILocation(line: 6, column: 1, scope: !11)
