;;; at:filter { grep -- '+++' }

#include "asmdef.h"

.text
ENTRY main
    LOG_ON
    nop
    ldi r24, 0
    rcall func
    ldi r24, 1
    rcall func
    rcall func2
    EXIT0
ENDFUNC


func:
    nop
    tst r24
    breq 0f
func.proep:
    do_prologue_saves 7, 10
    nop
    do_epilogue_restores 7, 10

0:  ret

func2:
    rcall func
    rjmp func
