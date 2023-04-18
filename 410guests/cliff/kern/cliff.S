/** @file cliff.S
 *  @brief Exception delivery will try to push information onto the stack.
 *  If pushing fails, then the guest should die.  We install a (fake) IDT
 *  entry for the divide-by-zero exception, then we divide by zero.
 * 
 *  @author rpearl
 */

#include <x86/idt.h>
#include <hvcall_int.h>

.globl kernel_main
kernel_main:
    pushl     $1                   # Privileged (it's an exception handler)
    pushl     $badness             # Valid handler address in case hv is careful
    pushl     $IDT_DE
    movl      $HV_SETIDT_OP, %eax
    int       $HV_INT              # ok, "handler" is installed

    movl      $0xFFF00000, %esp    # bad stack pointer

    xorl      %ecx, %ecx
    idiv      %ecx                 # game over, hv should slay us

badness:
    jmp       badness
