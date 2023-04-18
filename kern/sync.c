/** @file sync.c
 *
 *  @brief locks.
 *
 *  @author Hanjie Wu (hanjiew)
 *  @bug No functional bugs
 */

#include <x86/asm.h>

#include <interrupt.h>
#include <sched.h>
#include <sync.h>

void mutex_lock(mutex_t* m) {
    thread_t* current = get_current();
    int old_if = spl_lock(&m->guard);
    if (m->locked == 0) {
        m->locked = 1;
        spl_unlock(&m->guard, old_if);
        return;
    }
    queue_insert_tail(&m->waiters, &current->sched_link);
    current->status = THREAD_BLOCKED;
    int old_if2 = spl_lock(&ready_lock);
    thread_t* t = select_next();
    spl_unlock(&ready_lock, old_if2);
    /* release m->gaurd last, otherwise other thread may switch to us before
     * yielding
     */
    yield_to_spl_unlock(t, &m->guard, old_if);
}

void mutex_unlock(mutex_t* m) {
    int old_if = spl_lock(&m->guard);
    if (m->waiters == NULL) {
        m->locked = 0;
        spl_unlock(&m->guard, old_if);
        return;
    }
    /* transfer lock ownership to t */
    thread_t* t =
        queue_data(queue_remove_head(&m->waiters), thread_t, sched_link);
    int old_if2 = spl_lock(&ready_lock);
    insert_ready_head(t);
    spl_unlock(&ready_lock, old_if2);
    spl_unlock(&m->guard, old_if);
}

void cv_wait(cv_t* cv, mutex_t* m) {
    thread_t* current = get_current();
    int old_if = spl_lock(&cv->guard);
    queue_insert_tail(&cv->waiters, &current->sched_link);
    current->status = THREAD_BLOCKED;
    mutex_unlock(m);
    int old_if2 = spl_lock(&ready_lock);
    thread_t* t = select_next();
    spl_unlock(&ready_lock, old_if2);
    yield_to_spl_unlock(t, &cv->guard, old_if);
    /* some threads may jump in so a condition check in a while loop is required
     * outside
     */
    mutex_lock(m);
}

void cv_signal(cv_t* cv) {
    int old_if = spl_lock(&cv->guard);
    if (cv->waiters == NULL) {
        spl_unlock(&cv->guard, old_if);
        return;
    }
    thread_t* t =
        queue_data(queue_remove_head(&cv->waiters), thread_t, sched_link);
    int old_if2 = spl_lock(&ready_lock);
    insert_ready_head(t);
    spl_unlock(&ready_lock, old_if2);
    spl_unlock(&cv->guard, old_if);
}
