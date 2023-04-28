/** @file sync_asm.S
 *
 *  @brief locks.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

.text

#define EFL_IF 0x00000200

.global save_clear_if
.type save_clear_if, %function
save_clear_if:
    pushf
    pop %eax
    cli
    ret

.global restore_if
.type restore_if, %function
restore_if:
    mov 0x4(%esp), %eax
    push %eax
    popf
    ret

.global spl_lock
.type spl_lock, %function
spl_lock:
    push %ebx
    pushf
    pop %ebx
    cli
    mov 0x8(%esp), %ecx
    mov $1, %edx
.spl_lock_try:
    mov (%ecx), %eax
    test %eax, %eax
    jnz .spl_lock_try
    lock cmpxchg %edx, (%ecx)
    test %eax, %eax
    jnz .spl_lock_try
    mov %ebx, %eax
    pop %ebx
    ret

.global spl_unlock
.type spl_unlock, %function
spl_unlock:
    mov 0x8(%esp), %eax
    mov 0x4(%esp), %ecx
    xor %edx, %edx
    mov %edx, (%ecx)
    push %eax
    popf
    ret

.global yield_to_spl_unlock
.type yield_to_spl_unlock, %function
yield_to_spl_unlock:
    mov 0xc(%esp), %edx
    mov 0x8(%esp), %ecx
    mov 0x4(%esp), %eax
    push %edx /* old_if, will be restored when switching back */
    push %edi
    push %esi
    push %ebx
    push %ebp
    mov %eax, %ebp
    mov %ecx, %ebx
    call get_current
    cmp %eax, %ebp
    je .yield_to_spl_unlock_no_switch
    push %esp
    push %ebp
    call save_and_setup_env
    mov %eax, %esp
.yield_to_spl_unlock_no_switch:
    xor %eax, %eax
    mov %eax, (%ebx)
    pop %ebp
    pop %ebx
    pop %esi
    pop %edi
    popf
    ret
