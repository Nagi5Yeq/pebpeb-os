// @author de0u
// @author jrduvall
//
// "Creatures of the night, brought to light"

#include <hvcall_int.h>

.globl augchar
.globl kbd_intr
.globl timer_intr
.globl tick_count

.data
.align 4
ticks:
        .long 0
.text

augchar:
	movl $HV_ENABLE_OP, %eax
	int $HV_INT
pause:
	pause
	jmp pause
kbd_intr:
	addl $20, %esp        // restore %esp
	movl -16(%esp), %eax  // augchar from red zone (interrupts are off)
	ret

timer_intr:
	addl $20, %esp        // restore %esp
	incl ticks
	jmp augchar           // resume

tick_count:
	movl ticks, %eax
	ret
