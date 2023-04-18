/** @file asm_instr.c
 *
 *  @brief assembly stubs.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

.text

.global invlpg
.type invlpg, %function
invlpg:
    mov 0x4(%esp), %eax
    invlpg (%eax)
    ret

.global hlt
.type hlt, %function
hlt:
    hlt
    ret /* keep it for other platforms that do not respond to hlt */
