/** @file warp.S
 *
 *  @brief Just point our stack at the top of the indirectly mapped frame,
 *  trigger a hypercall, and clean up after ourselves.
 *
 *  @param  esp   new stack pointer to adopt
 *  @param    s   guest-virtual address of string to print
 *  @param  len   number of bytes to print
 *
 *  @author rpearl
 *  @author de0u
 */
#include <hvcall_int.h>

.globl warp

warp:
	pushl  %ebp
	movl   %esp, %ebp

	movl    8(%ebp), %esp        /* adopt new stack pointer */

	movl   12(%ebp), %eax        /* string address */
	pushl  %eax

	movl   16(%ebp), %eax        /* string length */
	pushl  %eax

	movl   $HV_PRINT_OP, %eax    /* hypercall */
	int    $HV_INT

	movl   %ebp, %esp            /* back to old stack */
	popl   %ebp

	ret
