;;; at:filter { grep -- '+++' }

#include "asmdef.h"

.text
ENTRY main
    LOG_ON
    nop
main.step:
    rcall func
    rcall func2
    nop
    EXIT0
ENDFUNC


func:
    nop
func.step:
    ret

func2:
    nop
func2.step:
    nop
    rjmp func
