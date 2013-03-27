;  assemble as "as -o reg-pa64 reg-pa64.s"
; or
;  cc -g -o +DA2.0W
;
; PA-RISC 2.0 register contents test.
;
        .level 2.0W

        .code
        .export main,ENTRY
        .export mainend,CODE
        .export lab1,CODE
        .space $TEXT$
        .subspa $CODE$
one
        .align 8
        .stringz        "?\xF0\x00\x00\x00\x00\x00\x00@\x00\x00\x00\x00\x00\x00"

main
        .proc
        .callinfo NO_CALLS,FRAME=0
        .entry

        ;; Test we have right register numbers
        ;;
        ADD             %r0,%r0,%r1             ;    0 
        LDI             1,%r1                   ;    1
        ;;
        ;; Don't put anything into r2 or r3--they are special registers.
        ;;
        ADD             %r1,%r1,%r4             ;    2
        ADD             %r4,%r4,%r5             ;    4
        ADD             %r5,%r5,%r6             ;    8
        ADD             %r6,%r6,%r7             ;   16
        ADD             %r7,%r7,%r8             ;   32
        ADD             %r8,%r8,%r9             ;   64
        ADD             %r9,%r9,%r10            ;  128
        ADD             %r10,%r10,%r11          ;  256
        ADD             %r11,%r11,%r12          ;  512
        ADD             %r12,%r12,%r13          ; 1024
        ADD             %r13,%r13,%r14          ; 2048
        ADD             %r14,%r14,%r15          ; 4096
        ADD             %r15,%r15,%r16          ; 9192

        ;; Test high bits, to be sure we show them.
        ;;
        LDI             0xde,%r19               ; "de"
        DEPD,Z          %r19,55,56,%r19         ; "de00"
        LDI             0xad,%r18               ; "ad"
        ADD             %r18,%r19,%r19          ; "dead"
        DEPD,Z          %r19,55,56,%r19         ; "dead00"
        LDI             0xbe,%r18               ; "be"
        ADD             %r18,%r19,%r19          ; "deadbe"
        DEPD,Z          %r19,55,56,%r19         ; "deadbe00"
        LDI             0xef,%r18               ; "ef"
        ADD             %r18,%r19,%r19          ; "deadbeef"
        ;
        DEPD,Z          %r19,55,56,%r19         ; "deadbeef00"
        LDI             0xba,%r18               ; "ba"
        ADD             %r18,%r19,%r19          ; "deadbeefba"
        DEPD,Z          %r19,55,56,%r19         ; "deadbeefba00"
        LDI             0xdc,%r18               ; "dc"
        ADD             %r18,%r19,%r19          ; "deadbeefbadc"
        DEPD,Z          %r19,55,56,%r19         ; "deadbeefbadc00"
        LDI             0xad,%r18               ; "ad"
        ADD             %r18,%r19,%r19          ; "deadbeefbadcad"
        DEPD,Z          %r19,55,56,%r19         ; "deadbeefbadcad00"
        LDI             0xee,%r18               ; "ee"
        ADD             %r18,%r19,%r19          ; "deadbeefbadcadee"
        
lab1    ;; Test floating point registers
        ;;
        ;; LDIL            LR'one,%r22             ;
        ;; FLDD            RR'one(%r22),%fr4       ;   1.0
        ;; FLDD            RR'one+8(%r22),%fr5     ;   2.0
        ;; FLDD            RR'one+8(%r22),%fr6     ;   2.0
	B,L		here,%r2
	NOP
here	DEPDI		0x0,63,2,%r2
	LDO		one-here(%r2),%r2
	FLDD		0(%r2),%fr4
	FLDD		8(%r2),%fr5
	FLDD		8(%r2),%fr6

        FMPY,DBL        %fr5,%fr6,%fr7          ;   4.0
        FMPY,DBL        %fr6,%fr7,%fr8          ;   8.0
        FMPY,DBL        %fr7,%fr8,%fr9          ;  32.0
        FMPY,DBL        %fr8,%fr9,%fr10         ; 256.0
        
        ;; The NOP prevents anything from end.o or crt0.o from
        ;; being appended immediately after "mainend".  If that
        ;; happens, then we may have other labels that have the
        ;; same address as "mainend", and thus the debugger
        ;; may symbolize this PC to something other than "mainend".
mainend                
        NOP
        .exit
        .procend

        .space $TEXT$
        .subspa $CODE$
        .subspa $LIT$        ;; <don't use> ,QUAD=0,ALIGN=8,ACCESS=0x2c,SORT=16
        .end

