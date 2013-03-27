#mach: crisv32
#output: Basic clock cycles, total @: *\n
#output: Memory source stall cycles: 82\n
#output: Memory read-after-write stall cycles: 0\n
#output: Movem source stall cycles: 6\n
#output: Movem destination stall cycles: 880\n
#output: Movem address stall cycles: 4\n
#output: Multiplication source stall cycles: 18\n
#output: Jump source stall cycles: 6\n
#output: Branch misprediction stall cycles: 0\n
#output: Jump target stall cycles: 0\n
#sim: --cris-cycles=basic

 .include "testutils.inc"

; Macros for testing correctness of movem destination stall
; cycles for various insn types.  Beware: macro parameters can
; be comma or space-delimited.  There are problems (i.e. bugs)
; with using space-delimited operands and operands with
; non-alphanumeric characters, like "[]-." so use comma for
; them.  Lots of trouble passing empty parameters and parameters
; with comma.  Ugh.  FIXME: Report bugs, fix bugs, fix other
; shortcomings, fix that darn old macro-parameter-in-string.

; Helper macro.  Unfortunately I find no cleaner way to unify
; one and two-operand cases, the main problem being the comma
; operand delimiter clashing with macro operand delimiter.
 .macro t_S_x_y S insn x y=none
 movem [r7],r6
 .ifc \y,none
  .ifc \S,none
   \insn \x
  .else
   \insn\S \x
  .endif
 .else
  .ifc \S,none
   \insn \x,\y
  .else
   \insn\S \x,\y
  .endif
 .endif
 nop
 nop
 nop
 .endm

; An insn-type that has a single register operand.  The register
; may or may not be a source register for the insn.
 .macro t_r insn
 t_S_x_y none,\insn,r3
 t_S_x_y none,\insn,r8
 .endm

; An insn-type that jumps to the destination of the register.
 .macro t_r_j insn
 move.d 0f,r7
 move.d 1f,r8
 move.d r8,r9
 nop
 nop
 nop
 .section ".rodata"
 .p2align 5
0:
 .dword 1f
 .dword 1f
 .dword 1f
 .dword 1f
 .dword 1f
 .dword 1f
 .dword 1f
 .previous
 t_r \insn
1:
 .endm

; An insn-type that has a size-modifier and two register
; operands.
 .macro t_xr_r S insn
 t_S_x_y \S \insn r3 r8
 t_S_x_y \S \insn r8 r3
 move.d r3,r9
 t_S_x_y \S \insn r4 r3
 t_S_x_y \S \insn r8 r9
 .endm

; An insn-type that has two register operands.
 .macro t_r_r insn
 t_xr_r none \insn
 .endm

; An t_r_rx insn with a byte or word-size modifier.
 .macro t_wbr_r insn
 t_xr_r .b,\insn
 t_xr_r .w,\insn
 .endm

; Ditto with a dword-size modifier.
 .macro t_dwbr_r insn
 t_xr_r .d,\insn
 t_wbr_r \insn
 .endm

; An insn-type that has a size-modifier, a constant and a
; register operand.
 .macro t_xc_r S insn
 t_S_x_y \S \insn 24 r3
 move.d r3,r9
 t_S_x_y \S \insn 24 r8
 .endm

; An insn-type that has a constant and a register operand.
 .macro t_c_r insn
 t_xc_r none \insn
 .endm

; An t_c_r insn with a byte or word-size modifier.
 .macro t_wbc_r insn
 t_xc_r .b,\insn
 t_xc_r .w,\insn
 .endm

; Ditto with a dword-size modifier.
 .macro t_dwbc_r insn
 t_xc_r .d,\insn
 t_wbc_r \insn
 .endm

; An insn-type that has size-modifier, a memory operand and a
; register operand.
 .macro t_xm_r S insn
 move.d 9b,r8
 t_S_x_y \S,\insn,[r4],r3
 move.d r3,r9
 t_S_x_y \S,\insn,[r8],r5
 move.d r5,r9
 t_S_x_y \S,\insn,[r3],r9
 t_S_x_y \S,\insn,[r8],r9
 .endm

; Ditto, to memory.
 .macro t_xr_m S insn
 move.d 9b,r8
 t_S_x_y \S,\insn,r3,[r4]
 t_S_x_y \S,\insn,r8,[r3]
 t_S_x_y \S,\insn,r3,[r8]
 t_S_x_y \S,\insn,r9,[r8]
 .endm

; An insn-type that has a memory operand and a register operand.
 .macro t_m_r insn
 t_xm_r none \insn
 .endm

; An t_m_r insn with a byte or word-size modifier.
 .macro t_wbm_r insn
 t_xm_r .b,\insn
 t_xm_r .w,\insn
 .endm

; Ditto with a dword-size modifier.
 .macro t_dwbm_r insn
 t_xm_r .d,\insn
 t_wbm_r \insn
 .endm

; Insn types of the regular type (r, c, m, size d w b).
 .macro t_dwb insn
 t_dwbr_r \insn
 t_dwbc_r \insn
 t_dwbm_r \insn
 .endm

; Similar, sizes w b.
 .macro t_wb insn
 t_wbr_r \insn
 t_wbc_r \insn
 t_wbm_r \insn
 .endm

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

 startnostack

; Initialize registers so they don't contain unknowns.

 move.d 9f,r7
 move.d r7,r8
 moveq 0,r9

; Movem source area.  Register contents must be valid
; addresses, aligned on a cache boundary.
 .section ".rodata"
 .p2align 5
9:
 .dword 9b
 .dword 9b
 .dword 9b
 .dword 9b
 .dword 9b
 .dword 9b
 .dword 9b
 .dword 9b
 .dword 9b
 .dword 9b
 .previous

; The actual tests.  The numbers in the comments specify the
; number of movem destination stall cycles.  Some of them may be
; filed as memory source address stalls, multiplication source
; stalls or jump source stalls, duly marked so.

 t_r_r abs		; 3+3

 t_dwb add		; (3+3+3)*3+3*3+(3+3+3)*3 (6 mem src)

 t_r_r addc		; (3+3+3)
 t_c_r addc		; 3
 t_m_r addc		; (3+3+3) (2 mem src)

 t_dwb move		; (3+3)+(3+3+3)*2+3*2+(3+3+3)*3 (6 mem src)
 t_xr_m .b move		; 3+3+3 (2 mem src)
 t_xr_m .w move		; 3+3+3 (2 mem src)
 t_xr_m .d move		; 3+3+3 (2 mem src)

 t_S_x_y none addi r3.b r8	; 3
 t_S_x_y none addi r8.w r3	; 3
 t_S_x_y none addi r4.d r3	; 3
 t_S_x_y none addi r8.w r9

 ; Addo has three-operand syntax, so we have to expand (a useful
 ; subset of) "t_dwb".
 t_S_x_y none addi r3.b "r8,acr"	; 3
 t_S_x_y none addi r8.w "r3,acr"	; 3
 t_S_x_y none addi r4.d "r3,acr"	; 3
 t_S_x_y none addi r8.w "r9,acr"

 t_S_x_y .b addo 42 "r8,acr"
 t_S_x_y .w addo 4200 "r3,acr"		; 3
 t_S_x_y .d addo 420000 "r3,acr"	; 3

 move.d 9b,r8
 t_S_x_y .d,addo,[r4],"r3,acr"		; 3 (1 mem src)
 t_S_x_y .b,addo,[r3],"r8,acr"		; 3 (1 mem src)
 t_S_x_y .w,addo,[r8],"r3,acr"		; 3
 t_S_x_y .w,addo,[r8],"r9,acr"

 ; Similar for addoq.
 t_S_x_y none addoq 42 "r8,acr"
 t_S_x_y none addoq 42 "r3,acr"		; 3

 t_c_r addq				; 3

 t_wb adds		; (3+3+3)*2+3*2+(3+3+3)*2 (4 mem src)
 t_wb addu		; (3+3+3)*2+3*2+(3+3+3)*2 (4 mem src)

 t_dwb and		; (3+3+3)*3+3*3+(3+3+3)*3 (6 mem src)
 t_c_r andq		; 3

 t_dwbr_r asr		; (3+3+3)*3
 t_c_r asrq		; 3

 t_dwbr_r bound		; (3+3+3)*3
 t_dwbc_r bound		; 3*3

 t_r_r btst		; (3+3+3)
 t_c_r btstq		; 3

 t_dwb cmp		; (3+3+3)*3+3*3+(3+3+3)*3 (6 mem src)
 t_c_r cmpq		; 3

 t_wbc_r cmps		; 3*2
 t_wbc_r cmpu		; 3*2
 t_wbm_r cmps		; (3+3+3)*2 (4 mem src)
 t_wbm_r cmpu		; (3+3+3)*2 (4 mem src)

 t_r_r dstep		; (3+3+3)

 ; FIXME: idxd, fidxi, ftagd, ftagi when supported.

 t_r_j jsr		; 3 (2 jump src)
 t_r_j jump		; 3 (2 jump src)

 t_c_r lapc.d

; The "quick operand" must be in range [. to .+15*2] so we can't
; use t_c_r.
 t_S_x_y none lapcq .+4 r3
 t_S_x_y none lapcq .+4 r8

 t_dwbr_r lsl		; (3+3+3)*3
 t_c_r lslq		; 3

 t_dwbr_r lsr		; (3+3+3)*3
 t_c_r lsrq		; 3

 t_r_r lz		; 3+3

 t_S_x_y none mcp srp r3	; 3
 t_S_x_y none mcp srp r8

 t_c_r moveq

 t_S_x_y none move srp r8
 t_S_x_y none move srp r3
 t_S_x_y none move r8 srp
 t_S_x_y none move r3 srp	; 3

; FIXME: move supreg,Rd and move Rs,supreg when supported.

 t_wb movs	; (3+3)*2+0+(3+3)*2 (4 mem src)
 t_wb movu	; (3+3)*2+0+(3+3)*2 (4 mem src)

 t_dwbr_r muls	; (3+3+3)*3 (9 mul src)
 t_dwbr_r mulu	; (3+3+3)*3 (9 mul src)

 t_dwbr_r neg	; (3+3)*3

 t_r not	; 3 cycles.

 t_dwb or	; (3+3+3)*3+3*3+(3+3+3)*3 (6 mem src)
 t_c_r orq	; 3

 t_r seq

 t_dwb sub	; (3+3+3)*3+3*3+(3+3+3)*3 (6 mem src)
 t_c_r subq	; 3

 t_wb subs	; (3+3+3)*2+3*2+(3+3+3)*2 (4 mem src)
 t_wb subu	; (3+3+3)*2+3*2+(3+3+3)*2 (4 mem src)

 t_r swapw	; 3 cycles.
 t_r swapnwbr	; 3 cycles.

 t_r_j jsrc	; 3 (2 jump src)

 t_r_r xor	; (3+3+3)

 move.d 9b,r7
 nop
 nop
 nop
 t_xm_r none movem	; (3+3) (2 mem src, 1+1 movem addr)
 ; As implied by the comment, all movem destination penalty
 ; cycles (but one) are accounted for as memory source address
 ; and movem source penalties.  There are also two movem address
 ; cache-line straddle penalties.
 t_xr_m none movem	; (3+3+2+2) (2 mem, 6 movem src, +2 movem addr)

 break 15
