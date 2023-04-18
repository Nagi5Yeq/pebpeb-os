/** @file usermem_asm.S
 *
 *  @brief userspace r/w functions.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

.text

.global try_read
.type try_read, %function
try_read:
    mov 0x4(%esp), %eax
    mov 0x8(%esp), %esi
    mov (%eax), %al
    mov %al, (%esi)
    xor %eax, %eax
    ret

.global try_write
.type try_write, %function
try_write:
    mov 0x4(%esp), %eax
    mov 0x8(%esp), %edx
    mov %dl, (%eax)
    xor %eax, %eax
    ret

/* unsuccessful r/w will jump to this function and we return 1 */
.global usermem_fail
.type usermem_fail, %function
usermem_fail:
    mov $0xffffffff, %eax
    ret
