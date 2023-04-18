/** @file sync.h
 *
 *  @brief locks.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#ifndef _SYNC_H_
#define _SYNC_H_

#include <common.h>

/** declaration of thread control block */
struct thread_s;
/** declaration of thread control block */
typedef struct thread_s thread_t;

/**
 * @brief save eflags and disable interrupt
 * @return eflags
 */
int save_clear_if();
/**
 * @brief restore eflags
 * @param old_if eflags
 */
void restore_if(int old_if);

/** spinlock structure, only needed for SMP */
typedef struct spl_s {
    int locked;
} spl_t;

/** initial value for spinlock */
#define SPL_INIT (spl_t){.locked = 0};

/**
 * @brief lock the spinlock and disable interrupts
 * @return original eflags
 */
int spl_lock(spl_t* spl);
/**
 * @brief unlock the spinlock and restore eflags
 * @param old_if eflags
 */
void spl_unlock(spl_t* spl, int old_if);

/**
 * @brief atomically yield to another thread and unlock the spinlock, eflags
 * will be restored when current thread become running again
 * @param t thread to yield to
 * @param spl spinlock
 * @param old_if eflags
 */
void yield_to_spl_unlock(thread_t* t, spl_t* spl, int old_if);

/** mutex structure */
typedef struct mutex_s {
    spl_t guard;
    int locked;
    queue_t* waiters;
} mutex_t;

/** initial value for mutex */
#define MUTEX_INIT \
    (mutex_t){.guard = {.locked = 0}, .locked = 0, .waiters = NULL};

/**
 * @brief lock the mutex, will be blocked if lock is held by other thread
 * @param m mutex
 */
void mutex_lock(mutex_t* m);
/**
 * @brief unlock the mutex, will wake up one blocked thread if there is one
 * @param m mutex
 */
void mutex_unlock(mutex_t* m);

/** cv structure */
typedef struct cv_s {
    spl_t guard;
    queue_t* waiters;
} cv_t;

/** initial value for cv */
#define CV_INIT \
    (cv_t) { .guard = {.locked = 0}, .waiters = NULL }

/**
 * @brief unlock the mutex m and wait for other threads to call cv_signal on cv
 * @param cv cv
 * @param m mutex
 */
void cv_wait(cv_t* cv, mutex_t* m);
/**
 * @brief signal a cv, will wake up one blocked thread if there is one
 * @param cv cv
 */
void cv_signal(cv_t* cv);

#endif
