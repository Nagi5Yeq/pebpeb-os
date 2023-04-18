/**
 * @file cond.c
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief
 * @date 2023-02-22
 *
 */

#include <assert.h>
#include <cond.h>
#include <stdlib.h>
#include <syscall.h>

/**
 * @brief initialize the condition variable pointed by cv
 *
 * @param cv
 * @return 0
 */
int cond_init(cond_t* cv) {
    mutex_init(&cv->lock);
    cv->queue = NULL;
    return 0;
}

/**
 * @brief destroy the condition variable pointed by cv.
 *
 * @param cv
 */
void cond_destroy(cond_t* cv) {
    affirm(cv->queue == NULL);
    mutex_destroy(&cv->lock);
}

/**
 * @brief wait on a cv until signaled and lock mp
 *
 * @param cv
 */
void cond_wait(cond_t* cv, mutex_t* mp) {
    cond_node_t self;
    mutex_lock(&cv->lock);
    self.tid = gettid();
    self.signaled = 0;
    if (cv->queue == NULL) { /* add self node to wait list */
        cv->queue = &self;
        self.next = self.prev = &self;
    } else {
        self.next = cv->queue;
        self.prev = cv->queue->prev;
        cv->queue->prev->next = &self;
        cv->queue->prev = &self;
    }
    mutex_unlock(mp);
    mutex_unlock(&cv->lock);
    while (self.signaled != 1) {
        deschedule(&self.signaled); /* will be set to 1 after other thread
                                       signal the cv */
    }
    mutex_lock(mp);
}

/**
 * @brief signal a cv and wake up one thread
 *
 * @param cv
 */
void cond_signal(cond_t* cv) {
    mutex_lock(&cv->lock);
    if (cv->queue != NULL) { /* remove a waiter from the list */
        cond_node_t* p = cv->queue;
        if (p->next == p) {
            cv->queue = NULL;
        } else {
            cv->queue = p->next;
            p->next->prev = p->prev;
            p->prev->next = p->next;
        }
        int tid = p->tid;
        p->signaled = 1;
        make_runnable(tid);
    }
    mutex_unlock(&cv->lock);
}

/**
 * @brief signal a cv and wake up all threads
 *
 * @param cv
 */
void cond_broadcast(cond_t* cv) {
    mutex_lock(&cv->lock);
    cond_node_t* p = cv->queue;
    if (p != NULL) {
        cond_node_t* end = p;
        do {
            cond_node_t* q = p->next;
            int tid = p->tid;
            p->signaled = 1;
            make_runnable(tid);
            p = q;
        } while (p != end);
        cv->queue = NULL;
    }
    mutex_unlock(&cv->lock);
}
