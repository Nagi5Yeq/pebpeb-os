/** @file syscall_hvcall.c
 *
 *  @brief miscellaneous syscalls.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <simics.h>
#include <stdio.h>
#include <string.h>

#include <common_kern.h>
#include <elf/elf_410.h>
#include <hvcall.h>
#include <hvcall_int.h>
#include <malloc_internal.h>
#include <ureg.h>

#include <x86/asm.h>
#include <x86/cr.h>
#include <x86/eflags.h>
#include <x86/seg.h>

#include <asm_instr.h>
#include <assert.h>
#include <loader.h>
#include <malloc.h>
#include <mm.h>
#include <paging.h>
#include <sched.h>
#include <sync.h>
#include <timer.h>
#include <usermem.h>

// 0: not pv, 1: processed
int handle_pv_syscall(stack_frame_t* f) {
    if (get_current()->process->pv == NULL) {
        return 0;
    }
    if (f->eip >= USER_MEM_START) {
        // inject interrupt
        return 1;
    }
    // kernel called syscall
    return 1;
}

void sys_hvcall_real(stack_frame_t* f) {
    if (get_current()->process->pv == NULL) {
        f->eax = (reg_t)-1;
        return;
    }
    if (f->eip >= USER_MEM_START) {
        // inject interrupt
        return;
    }
    switch (f->eax) {
        case HV_MAGIC_OP:
            f->eax = (reg_t)HV_MAGIC;
            break;
        default:
            f->eax = (reg_t)-1;
            break;
    }
}
