/** @file sched_asm.S
 *
 *  @brief scheduling functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <x86/seg.h>

.text

.global set_fs
.type set_fs, %function
set_fs:
    mov 0x4(%esp), %fs
    ret

.global set_gs
.type set_gs, %function
set_gs:
    mov 0x4(%esp), %gs
    ret

.macro PERCPU_GETSET name offset
.global get_\name
.type get_\name, %function
get_\name:
    xor %eax, %eax
    mov %fs:\offset(%eax), %eax
    ret

.global set_\name
.type set_\name, %function
set_\name:
    xor %ecx, %ecx
    mov 0x4(%esp), %eax
    mov %eax, %fs:\offset(%ecx)
    ret
.endm

PERCPU_GETSET current 0x0
PERCPU_GETSET idle 0x4
PERCPU_GETSET kthread 0x8
PERCPU_GETSET mapped_phys_page 0xc
PERCPU_GETSET mapped_phys_page_pte 0x10

.global return_to_user
.type return_to_user, %function
return_to_user:
    push %esp
    call check_pending_signals
    add $0x4, %esp
    pop %gs /* restore from stack_frame_t structure */
    pop %fs
    pop %es
    pop %ds
    popa
    iret

.global switch_stack_cleanup
.type switch_stack_cleanup, %function
switch_stack_cleanup:
    mov 0x8(%esp), %eax
    mov 0x4(%esp), %esp
    push %eax
    call cleanup_dead_thread
    add $0x4, %esp
    pop %ebp
    pop %ebx
    pop %esi
    pop %edi
    popf
    ret
