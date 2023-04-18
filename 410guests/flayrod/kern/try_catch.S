#include "hvcall_int.h"
#include "x86/idt.h"

#define BAD_PTR 0x37373737

.global tryit
tryit:
  // First hv_setidt(IDT_PF, askew, privileged=1)
  pushl   $1
  leal    askew, %eax
  pushl   %eax
  pushl   $IDT_PF
  movl    $HV_SETIDT_OP, %eax
  int     $HV_INT
  // Now dereference a bad pointer in %eax
  movl    $BAD_PTR, %eax
  movl    %eax, (%eax)
  // Pretend that if we get back here we should just return
  addl    $12, %esp
  ret

// Page-fault handler (for when something has gone out askew)
.global askew
askew:
  cmp     (%esp), %eax
  je      success
fail:
  // Hypervisor reported an incorrect value for the bad pointer
  // so we exit with a bad return code.
  pushl   $0xBAD
  jmp     runaway
success:
  // Hypervisor reported the correct value for the bad pointer
  // so we exit with a good return code.
  pushl   $0
  jmp     runaway

runaway:
  movl    $HV_EXIT_OP, %eax
  int     $HV_INT
  // Should not get here - so let's crash hard
  ud2
