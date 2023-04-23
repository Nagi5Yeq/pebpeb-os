/** @file interrupt_asm.S
 *
 *  @brief fault and interrupt handlers.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <x86/seg.h>
#include <x86/interrupt_defines.h>

#define SEGSEL_KERNEL_FS SEGSEL_SPARE2

.global default_handler
default_handler:
    iret

.macro FAULT_HANDLER_ERR name cause
.global \name\()_handler
.type \name\()_handler, %function
\name\()_handler:
    pusha /* save a ureg_t structure */
    push %gs
    push %fs
    push %es
    push %ds
    mov %cr2, %eax
    push %eax
    push $\cause
    mov $SEGSEL_KERNEL_DS, %eax
    mov %ax, %ds
    mov %ax, %es
    mov $SEGSEL_KERNEL_FS, %eax
    mov %ax, %fs
    cld
    push %esp
    call handle_fault
    add $0xc, %esp /* skip cr2, cause and argument */
    pop %ds
    pop %es
    pop %fs
    pop %gs
    popa
    add $0x4, %esp /* skip error code */
    iret
.endm

.macro FAULT_HANDLER_NOERR name cause
.global \name\()_handler
.type \name\()_handler, %function
\name\()_handler:
    push $0 /* dummy error code */
    pusha
    push %gs
    push %fs
    push %es
    push %ds
    mov %cr2, %eax
    push %eax
    push $\cause
    mov $SEGSEL_KERNEL_DS, %eax
    mov %ax, %ds
    mov %ax, %es
    mov $SEGSEL_KERNEL_FS, %eax
    mov %ax, %fs
    cld
    push %esp
    call handle_fault
    add $0xc, %esp /* skip cr2, cause and argument */
    pop %ds
    pop %es
    pop %fs
    pop %gs
    popa
    add $0x4, %esp /* skip error code */
    iret
.endm

FAULT_HANDLER_NOERR de 0x0
FAULT_HANDLER_NOERR db 0x1
FAULT_HANDLER_NOERR nmi 0x2
FAULT_HANDLER_NOERR bp 0x3
FAULT_HANDLER_NOERR of 0x4
FAULT_HANDLER_NOERR br 0x5
FAULT_HANDLER_NOERR ud 0x6
FAULT_HANDLER_NOERR nm 0x7
FAULT_HANDLER_ERR df 0x8
FAULT_HANDLER_NOERR cso 0x9
FAULT_HANDLER_ERR ts 0xa
FAULT_HANDLER_ERR np 0xb
FAULT_HANDLER_ERR ss 0xc
FAULT_HANDLER_ERR gp 0xd
FAULT_HANDLER_ERR pf 0xe
FAULT_HANDLER_ERR fault_15 0xf
FAULT_HANDLER_NOERR mf 0x10
FAULT_HANDLER_ERR ac 0x11
FAULT_HANDLER_NOERR mc 0x12
FAULT_HANDLER_NOERR xf 0x13

.global timer_handler
.type timer_handler, %function
timer_handler:
    pusha /* save a stack_frame_t structure */
    push %ds
    push %es
    push %fs
    push %gs
    mov $SEGSEL_KERNEL_DS, %eax
    mov %ax, %ds
    mov %ax, %es
    mov $SEGSEL_KERNEL_FS, %eax
    mov %ax, %fs
    cld
    push %esp
    call timer_handler_real
    add $0x4, %esp
    jmp return_to_user /* check pending exit before iret */


.global kbd_handler
.type kbd_handler, %function
kbd_handler:
    pusha /* save a stack_frame_t structure */
    push %ds
    push %es
    push %fs
    push %gs
    mov $SEGSEL_KERNEL_DS, %eax
    mov %ax, %ds
    mov %ax, %es
    mov $SEGSEL_KERNEL_FS, %eax
    mov %ax, %fs
    cld
    push %esp
    call kbd_handler_real
    add $0x4, %esp
    jmp return_to_user /* check pending exit before iret */
