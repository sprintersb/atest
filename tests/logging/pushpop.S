;;; at:filter { grep -- '+++' }

#include "asmdef.h"

.text
ENTRY main
    LOG_ON
    nop
    rcall test_pushpop8
    nop
    EXIT0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
.macro set_Z_SP
    in      ZL, SPL_IO_ADDR
#ifdef SPH
    in      ZH, SPH_IO_ADDR
#else
    clr     ZH
#endif /* SPH */
.endm

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Push 8 GPRs that don't overlap with ZL / ZH.
;;; SREG is unchanged, same for all registers, i.e. the macro
;;; implements a function (including the final RET) behaving like:
;;;     push q7
;;;     push q6
;;;     ...
;;;     push q0
;;; Overhead: 15 insns when #PC = 2 (compared to an ideal function).

.macro PUSH_8 q7, q6, q5, q4, q3, q2, q1, q0
    ;; The CALL put PC_SIZE bytes on the stack, and we can already
    ;; push 8 - PC_SIZE regs.
#if PC_SIZE == 2
    push    \q5
#endif
    push    \q4
    push    \q3
    push    \q2
    push    ZL
    push    ZH
    ;; Now we have the SP in place for the caller.

    set_Z_SP

    ;; Transfer/push the return address.
    ldd     \q2, Z + 8  $  push \q2
    ldd     \q2, Z + 7  $  push \q2
#if PC_SIZE == 3
    ldd     \q2, Z + 6  $  push \q2
#endif
    ;; Transfer/push ZL and ZH
    ldd     \q2, Z + 2  $  push \q2
    ldd     \q2, Z + 1  $  push \q2
    ;; Restore q2 from its push above.
    ldd     \q2, Z + 3
    ;; Store q0, q1, (q5,) q6, q7 in the now unused slots.
    std     Z + 1, \q0
    std     Z + 2, \q1
#if PC_SIZE == 3
    std     Z + 6, \q5
#endif
    std     Z + 7, \q6
    std     Z + 8, \q7
    ;; Restore Z
    pop     ZH
    pop     ZL
    ret
.endm

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Pop 8 GPRs that don't overlap with ZL / ZH.
;;; SREG is unchanged, same for all registers except the popped ones, i.e.
;;; the macro implements a function (including the final RET) behaving like:
;;;     pop q0
;;;     pop q1
;;;     ...
;;;     pop q7
;;; Overhead: 10 insns when #PC = 2 (compared to an ideal function).

.macro POP_8 q0, q1, q2, q3, q4, q5, q6, q7
#if PC_SIZE == 2
    ;; Pop the return address.
    pop     \q4
    pop     \q5
    ;; Pop all q's except the remaining 8 - 2 * PC_SIZE ones.
    pop     \q0
    pop     \q1
    pop     \q2
    pop     \q3
    ;; Save Z.
    push    ZL
    push    ZH
    ;; Stack is now: ZH, ZL,  q4, q5,  q6, q7

    set_Z_SP

    ;; Restore q7, q6 and...
    ldd     \q7, Z + 6
    ldd     \q6, Z + 5
    ;; ...put the return address in these slots.
    std     Z + 5, \q4
    std     Z + 6, \q5
    ;; Restore Z and the remaining q's.
    pop     ZH
    pop     ZL
    pop     \q4
    pop     \q5
#elif PC_SIZE == 3
    ;; Pop the return address.
    pop     \q2
    pop     \q3
    pop     \q4
    ;; Pop all q's except the remaining 8 - 2 * PC_SIZE ones.
    pop     \q0
    pop     \q1
    ;; Save Z.
    push    ZL
    push    ZH
    ;; Stack is now: ZH, ZL,  q2, q3, q4,  q5, q6, q7

    set_Z_SP

    ;; Restore q7, q6, q5 and...
    ldd     \q7, Z + 8
    ldd     \q6, Z + 7
    ldd     \q5, Z + 6
    ;; ...put the return address in these slots.
    std     Z + 6, \q2
    std     Z + 7, \q3
    std     Z + 8, \q4
    ;; Restore Z and the remaining q's.
    pop     ZH
    pop     ZL
    pop     \q2
    pop     \q3
    pop     \q4
#else
#error
#endif
    ret
.endm

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
#define A0  18
#define A1  19
#define A2  20
#define A3  21
#define A4  22
#define A5  23
#define A6  24
#define A7  25

/* Caveats:
   1) The follwing functions MUST be CALL-ed and MUST NOT be XJMP-ed to.
   2) As a consequence, the caller MUST NOT return directly after calling
      one of the functions, like in:
         XCALL pop8
         NOP          ; Indispensable for correct operation! DON'T REMOVE!
         RET
      Otherwise, linker relaxation may turn the XCALL+RET into XJMP+RET,
      violating 1).  Relaxation could be turned off, or the application
      could be linked with --no-call-ret-replacement; but that would be
      far too fragile.
   3) Calling such a routine to push/pop registers cannot be moved to a
      function.  Suppose you have a function that does
          XCALL pop8_A
          XCALL pop8_B
      It is NOT possible to replace this with
          XCALL pop8_AB
      where pop8_AB's code is
          XCALL pop8_A
          XCALL pop8_B
          NOP
          RET
*/

;;; !!! Function must be XCALL-ed and MUST NOT be XJMP-ed to !!!
ENTRY push8
    PUSH_8 A7, A6, A5, A4, A3, A2, A1, A0

;;; !!! Function must be XCALL-ed and MUST NOT be XJMP-ed to !!!
ENTRY pop8
    POP_8 A0, A1, A2, A3, A4, A5, A6, A7

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ENTRY test_pushpop8

    ldi A0, 0x12
    ldi A1, 0x34
    ldi A2, 0x56
    ldi A3, 0x78
    ldi A4, 0x90
    ldi A5, 0xab
    ldi A6, 0xcd
    ldi A7, 0xef
    ldi ZL, 0x11
    ldi ZH, 0x22

    XCALL push8

    clr A0
    clr A1
    X_movw  A2, A0
    X_movw  A4, A0
    X_movw  A6, A0

    XCALL pop8

    cpi A0, 0x12  $  brne bad
    cpi A1, 0x34  $  brne bad
    cpi A2, 0x56  $  brne bad
    cpi A3, 0x78  $  brne bad
    cpi A4, 0x90  $  brne bad
    cpi A5, 0xab  $  brne bad
    cpi A6, 0xcd  $  brne bad
    cpi A7, 0xef  $  brne bad
    cpi ZL, 0x11  $  brne bad
    cpi ZH, 0x22  $  brne bad
    ret

bad:
    AVRTEST_ABORT
ENDFUNC
