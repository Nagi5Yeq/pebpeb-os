/**
 * @file thread.c
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief Implementation of main thread functions
 * @date 2023-02-22
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <mutex.h>
#include <simics.h>
#include <syscall.h>
#include <thr_internals.h>
#include <thread.h>
#include <ureg.h>

/* return value for thread swexn */
#define THREAD_EXIT_SWEXN -2

/**
 * @brief stack size requested
 */
static uintptr_t thr_stack_size;
/**
 * @brief stack size aligned
 */
static uintptr_t thr_stack_alloc_size;
/**
 * @brief highest address of thread stacks
 */
static uintptr_t thr_stack_begin;
/**
 * @brief lowest address of thread stacks
 */
static uintptr_t thr_stack_end;

/**
 * @brief tcb for main thread
 */
tcb_t main_tcb;

/**
 * @brief global lock for tcb operations
 */
mutex_t tcb_lock;

/**
 * @brief list of unused tcbs
 */
tcb_t* free_tcbs = NULL;

/**
 * @brief get esp register
 */
uintptr_t get_esp();
/**
 * @brief swexn handler for multithread programs
 */
static void thr_swexn_handler(void* arg, ureg_t* reg);
/**
 * @brief wrapper function for thread_fork syscall
 * @param tcb tcb for new thread
 * @param f user thread entry point
 * @param args user arg for f
 */
int thread_fork(tcb_t* tcb, void* (*f)(void*), void* args);

/**
 * @brief Get the tcb of the thread this function is called form
 *
 * @return tcb_t*   pointer to current thread's TCB
 */
tcb_t* get_self_tcb() {
    uintptr_t esp = get_esp();

    /* check esp in main thread area */
    if (esp > main_tcb.stack_lo) {
        return &main_tcb;
    }

    /* calculate stack area from esp */
    uintptr_t stack_end = esp + (thr_stack_begin - esp) % thr_stack_alloc_size;
    /* pointer to tcb is at highest address in current thread stack */
    return *(tcb_t**)(stack_end - sizeof(tcb_t*));
}

/**
 * @brief Initialize thread library. Must be called once before using any thread
 *        functions.
 *
 * Thread stack structure:
 * +-------------------------+-----+---------------+
 * | thread stack                  |address of tcb |
 * +-------------------------+-----+---------------+
 *
 *
 * @param size  the amount of stack space available for each thread
 *
 * @return int  0 on success, -1 on faliure
 */
int thr_init(uintptr_t size) {
    /* set globals */
    thr_stack_size = size;
    thr_stack_alloc_size = ((size + PAGE_SIZE - 1) & PAGE_ALIGN_MASK);
    thr_stack_begin = ((main_tcb.stack_lo) & PAGE_ALIGN_MASK);
    thr_stack_end = thr_stack_begin;

    /* initialize global lock - will never fail */
    mutex_init(&tcb_lock);

    if (swexn(ex_stack_end, thr_swexn_handler, NULL, NULL) < 0)
        return -1;

    return 0;
}

/**
 * @brief Create a new thread with the entry point 'func', and arguments 'args'
 *
 * @param func  thread entry point
 * @param args  thread entry function arguments
 *
 * @return -1 on failure, 0 on success
 */
int thr_create(void* (*func)(void*), void* args) {
    /* holds return values for error checking and the TID to be returned */
    int result;

    /* holds pointer tcb for new thread */
    tcb_t* thread_tcb;

    mutex_lock(&tcb_lock);

    /* find tcb from free list */
    if (free_tcbs != NULL) {
        thread_tcb = free_tcbs;
        /* reuse stack area */
        result = new_pages((void*)thread_tcb->stack_lo, thr_stack_alloc_size);

        if (result != 0) {
            mutex_unlock(&tcb_lock);
            return -1;
        }

        /* remove from list (use tcb->right as link list) */
        free_tcbs = free_tcbs->right;
    }

    else {
        /* allocate new tcb */
        thread_tcb = malloc(sizeof(tcb_t));

        if (thread_tcb == NULL) {
            mutex_unlock(&tcb_lock);
            return -1;
        }

        thr_stack_end -= thr_stack_alloc_size;
        result = new_pages((void*)thr_stack_end, thr_stack_alloc_size);

        /* if new_pages unsuccessful */
        if (result != 0) {
            thr_stack_end += thr_stack_alloc_size;
            free(thread_tcb);
            mutex_unlock(&tcb_lock);
            return -1;
        }

        /* set tcb stack boundaries */
        thread_tcb->stack_lo = thr_stack_end;
        thread_tcb->stack_hi =
            thr_stack_end + thr_stack_alloc_size - sizeof(tcb_t*);
    }

    /* put pointer to tcb at the top of stack for get_self_tcb() use */
    *(tcb_t**)(thread_tcb->stack_lo + thr_stack_alloc_size - sizeof(tcb_t*)) =
        thread_tcb;

    thread_tcb->waiter = 0;
    thread_tcb->exited = 0;
    thread_tcb->is_main = 0;

    cond_init(&thread_tcb->wait_cv);

    thread_tcb->mutex_next = NULL;
    thread_tcb->mutex_resume = 0;

    /* create new thread */
    result = thread_fork(thread_tcb, func, args);

    /* if thread_fork failed */
    if (result < 0) {
        cond_destroy(&thread_tcb->wait_cv);
        remove_pages((void*)thread_tcb->stack_lo);

        thread_tcb->right = free_tcbs;
        free_tcbs = thread_tcb;

        mutex_unlock(&tcb_lock);

        return -1;
    }

    /* thread successfully created, return tid */

    thread_tcb->tid = result;
    rb_insert_tcb(thread_tcb);

    mutex_unlock(&tcb_lock);

    return result;
}

/**
 * @brief wait thread tid to exit and get exit status
 *
 * @param tid thread to wait
 * @param statusp output pointer for exit status
 * @return int      0 on success, -1 on error
 */
int thr_join(int tid, void** statusp) {
    mutex_lock(&tcb_lock);

    tcb_t* tcb = rb_find_tcb(tid);

    if (tcb == NULL) {
        mutex_unlock(&tcb_lock);
        return -1;
    }

    /* other thread is waiting for the thread */
    if (tcb->waiter != 0) {
        mutex_unlock(&tcb_lock);
        return -1;
    }

    /* make sure others will not wait on this thread */
    tcb->waiter = 1;

    /* wait until thread exits */
    if (tcb->exited != 1) {
        cond_wait(&tcb->wait_cv, &tcb_lock);
    }

    /* return exited thread status if user wants it */
    if (statusp != NULL) {
        *statusp = tcb->exit_value;
    }

    /* cleanup and return */
    rb_delete_tcb(tcb);
    cond_destroy(&tcb->wait_cv);

    if (tcb->is_main == 0) {
        remove_pages((void*)tcb->stack_lo);
        tcb->right = free_tcbs;
        free_tcbs = tcb;
    }

    mutex_unlock(&tcb_lock);
    return 0;
}

/**
 * @brief exit from thread with given status
 *
 * @param status
 */
void thr_exit(void* status) {
    tcb_t* tcb = get_self_tcb();
    mutex_lock(&tcb_lock);

    tcb->exit_value = (void*)status;
    tcb->exited = 1;

    cond_signal(&tcb->wait_cv);
    /* call vanish after unlock and do not use stack becuase the stack may have
     * been freed by other threads */
    mutex_unlock_vanish(&tcb_lock);
}

/**
 * @brief return the ID of the current thread
 *
 * @return int  - ID of the current thread
 */
int thr_getid(void) {
    return get_self_tcb()->tid;
}

/**
 * @brief Defers execution of involing thread
 *
 * @param tid
 * @return int
 */
int thr_yield(int tid) {
    return yield(tid);
}

/**
 * @brief thread entrypoint wrapper
 * @param f user func
 * @param arg user argument
 */
void thr_begin(void* (*f)(void*), void* arg) {
    mutex_lock(&tcb_lock);
    mutex_unlock(&tcb_lock);
    thr_exit(f(arg));
}

/**
 * @brief swexn handler
 *
 * @param arg
 * @param reg
 */
static void thr_swexn_handler(void* arg, ureg_t* reg) {
    task_vanish(THREAD_EXIT_SWEXN);
}
