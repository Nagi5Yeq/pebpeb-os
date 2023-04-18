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
    pushf
    pop %edx
    cli
    mov 0x4(%esp), %esi
    mov $1, %edi
.spl_lock_try:
    xor %eax, %eax
    lock cmpxchg %edi, (%esi)
    test %eax, %eax
    jnz .spl_lock_try
    mov %edx, %eax
    ret

.global spl_unlock
.type spl_unlock, %function
spl_unlock:
    mov 0x8(%esp), %eax
    mov 0x4(%esp), %esi
    xor %edx, %edx
    mov %edx, (%esi)
    push %eax
    popf
    ret

.global yield_to_spl_unlock
.type yield_to_spl_unlock, %function
yield_to_spl_unlock:
    mov 0xc(%esp), %edi
    mov 0x8(%esp), %esi
    mov 0x4(%esp), %eax
    push %edi /* old_if, will be restored when switching back */
    push %ebx
    push %ebp
    mov %esi, %ebx
    push %esp
    push %eax
    call save_and_setup_env
    mov %eax, %esp
    xor %eax, %eax
    mov %eax, (%ebx)
    pop %ebp
    pop %ebx
    popf
    ret
