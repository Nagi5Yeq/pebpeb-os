/** @file syscall_asm.S
 *
 *  @brief syscall entries.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <simics.h>
#include <x86/seg.h>

#define SEGSEL_KERNEL_FS SEGSEL_SPARE0

.macro SYSCALL name
.global sys_\name
.type sys_\name, %function
sys_\name:
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
    call sys_\name\()_real
    add $0x4, %esp
    jmp return_to_user /* check pending exit before iret */
.endm

SYSCALL fork
SYSCALL exec
SYSCALL wait
SYSCALL yield
SYSCALL deschedule
SYSCALL make_runnable
SYSCALL gettid
SYSCALL new_pages
SYSCALL remove_pages
SYSCALL sleep
SYSCALL getchar
SYSCALL readline
SYSCALL print
SYSCALL set_term_color
SYSCALL set_cursor_pos
SYSCALL get_cursor_pos
SYSCALL thread_fork
SYSCALL get_ticks
SYSCALL misbehave
SYSCALL halt
SYSCALL task_vanish
SYSCALL set_status
SYSCALL vanish
SYSCALL readfile
SYSCALL swexn

SYSCALL enosys
