/**
 * @file thr_internals.h
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief tcb structure and management functions and excpetion stack
 * @date 2023-02-22
 *
 */

#ifndef THR_INTERNALS_H
#define THR_INTERNALS_H

#include <stdint.h>

#include <cond.h>
#include <mutex.h>
#include <syscall.h>

/**
 * @brief stack for excpetion handler
 */
extern char ex_stack[];

/**
 * @brief pointer to the highest address of exception stack
 */
extern void* ex_stack_end;

/**
 * @brief thread control block structure
 */
typedef struct tcb_s {
    int tid;            /* thread id */
    uintptr_t stack_lo; /* low limit of stack */
    uintptr_t stack_hi; /* high limit of stack */
    int is_main;        /* is main thread */

    int color;            /* rbtree color */
    struct tcb_s* parent; /* rbtree parent */
    struct tcb_s* left;   /* rbtree left node */
    struct tcb_s* right;  /* rbtree right node */

    struct tcb_s* mutex_next; /* queue for block on mutex */
    struct tcb_s* mutex_prev; /* queue for block on mutex */
    int mutex_resume; /* flag to show is other thread is going to resume this
                         thread */

    int waiter;       /* tid of other thread that called thr_join on it */
    int exited;       /* is exited */
    void* exit_value; /* return value */
    cond_t wait_cv;   /* cv to wait for thread exit */
} tcb_t;

/**
 * @brief tcb for main thread
 */
extern tcb_t main_tcb;

/**
 * @brief find tcb by thread id
 */
tcb_t* rb_find_tcb(int tid);
/**
 * @brief insert tcb into rbtree
 */
void rb_insert_tcb(tcb_t* tcb);
/**
 * @brief delete tcb from rbtree
 */
void rb_delete_tcb(tcb_t* tcb);

#define PAGE_ALIGN_MASK ((uintptr_t) ~((uintptr_t)(PAGE_SIZE - 1)))

#endif
