#include <x86/seg.h>
#include <x86/interrupt_defines.h>
#include <x86/timer_defines.h>

.text

.global timer_test_handler
.type timer_test_handler, %function
timer_test_handler:
    push %eax
    mov timer_count, %eax
    dec %eax
    mov %eax, timer_count
    mov $INT_ACK_CURRENT, %al
    mov $INT_CTL_PORT, %dx
    outb %al, %dx
    pop %eax
    iret
