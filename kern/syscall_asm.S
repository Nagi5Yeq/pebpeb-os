/** @file syscall_asm.S
 *
 *  @brief syscall entries.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <simics.h>
#include <x86/seg.h>

#define SEGSEL_KERNEL_FS SEGSEL_SPARE2

.macro SYSCALL name index
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
    push $\index
    call pv_handle_syscall
    add $0x8, %esp
    test %eax, %eax
    je return_to_user
    push %esp
    call sys_\name\()_real
    add $0x4, %esp
    jmp return_to_user /* check pending exit before iret */
.endm

SYSCALL fork 0x41
SYSCALL exec 0x42
SYSCALL wait 0x44
SYSCALL yield 0x45
SYSCALL deschedule 0x46
SYSCALL make_runnable 0x47
SYSCALL gettid 0x48
SYSCALL new_pages 0x49
SYSCALL remove_pages 0x4a
SYSCALL sleep 0x4b
SYSCALL getchar 0x4c
SYSCALL readline 0x4d
SYSCALL print 0x4e
SYSCALL set_term_color 0x4f
SYSCALL set_cursor_pos 0x50
SYSCALL get_cursor_pos 0x51
SYSCALL thread_fork 0x52
SYSCALL get_ticks 0x53
SYSCALL misbehave 0x54
SYSCALL halt 0x55
SYSCALL task_vanish 0x57
SYSCALL new_console 0x58
SYSCALL set_status 0x59
SYSCALL vanish 0x60
SYSCALL readfile 0x62
SYSCALL swexn 0x74

.global sys_hvcall
.type sys_hvcall, %function
sys_hvcall:
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
    call sys_hvcall_real
    add $0x4, %esp
    jmp return_to_user /* check pending exit before iret */

.macro NONEXIST_SYSCALL name index
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
    push $\index
    call pv_handle_syscall
    add $0x8, %esp
    jmp return_to_user /* check pending exit before iret */
.endm

/* some ABIs do not place syscall index in a register so we 
 * need to set different entries for the syscalls
 */
NONEXIST_SYSCALL 67 0x43
NONEXIST_SYSCALL 86 0x56
NONEXIST_SYSCALL 97 0x61
NONEXIST_SYSCALL 99 0x63
NONEXIST_SYSCALL 100 0x64
NONEXIST_SYSCALL 101 0x65
NONEXIST_SYSCALL 102 0x66
NONEXIST_SYSCALL 103 0x67
NONEXIST_SYSCALL 104 0x68
NONEXIST_SYSCALL 105 0x69
NONEXIST_SYSCALL 106 0x6a
NONEXIST_SYSCALL 107 0x6b
NONEXIST_SYSCALL 108 0x6c
NONEXIST_SYSCALL 109 0x6d
NONEXIST_SYSCALL 110 0x6e
NONEXIST_SYSCALL 111 0x6f
NONEXIST_SYSCALL 112 0x70
NONEXIST_SYSCALL 113 0x71
NONEXIST_SYSCALL 114 0x72
NONEXIST_SYSCALL 115 0x73
NONEXIST_SYSCALL 128 0x80
NONEXIST_SYSCALL 129 0x81
NONEXIST_SYSCALL 130 0x82
NONEXIST_SYSCALL 131 0x83
NONEXIST_SYSCALL 132 0x84
NONEXIST_SYSCALL 133 0x85
NONEXIST_SYSCALL 134 0x86

NONEXIST_SYSCALL nonexist 0x0
