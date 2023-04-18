/**
 * @file autostack.c
 * @author Hanjie Wu (hanjiew)
 * @brief autostack implementation
 * @date 2023-02-22
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <simics.h>
#include <syscall.h>
#include <thr_internals.h>
#include <ureg.h>

/**
 * @brief test if the page fault is caused by page not present
 */
#define IS_PF_NP(error_code) (((error_code)&0x1) == 0)

/**
 * @brief a stack size which is enough for our swexn handlers
 */
#define EX_STACK_SIZE (1 << 14)

char ex_stack[EX_STACK_SIZE];
void* ex_stack_end = &ex_stack[sizeof(ex_stack)];
/**
 * @brief a stack size which is enough for common programs
 */
#define AUTOSYACK_SIZE (1 << 24)
/**
 * @brief size limit of autostack growth
 */
const static unsigned int stack_limit = AUTOSYACK_SIZE;

/**
 * @brief autostack swexn handler
 */
static void swexn_handler(void* arg, ureg_t* reg) {
    /* check cause */
    if (reg->cause == SWEXN_CAUSE_PAGEFAULT && IS_PF_NP(reg->error_code)) {
        /* check stack limit */
        if (reg->cr2 < main_tcb.stack_lo &&
            reg->cr2 > main_tcb.stack_lo - stack_limit) {
            uintptr_t aligned = reg->cr2 & PAGE_ALIGN_MASK;
            if (new_pages((void*)aligned, (int)(main_tcb.stack_lo - aligned)) !=
                0) {
                task_vanish(-2);
            }
            main_tcb.stack_lo = aligned;
            swexn(ex_stack_end, swexn_handler, NULL, reg);
        }
    }
    task_vanish(-2);
}

/**
 * @brief install autostack handler
 */
void install_autostack(void* stack_high, void* stack_low) {
    /* use main_tcb to record stack limits */
    main_tcb.stack_hi = (unsigned int)stack_high;
    main_tcb.stack_lo = (unsigned int)stack_low;
    main_tcb.tid = gettid();
    main_tcb.is_main = 1;
    rb_insert_tcb(&main_tcb);
    swexn(ex_stack_end, swexn_handler, NULL, NULL);
}
