/**
 * @file mutex_type.h
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief mutex structure
 * @date 2023-02-22
 *
 */
#ifndef MUTEX_TYPE_H
#define MUTEX_TYPE_H

#include <stdint.h>
#include <stdlib.h>

/* decleration of thread control block */
struct tcb_s;

/**
 * @brief mutex structure
 */
typedef struct mutex_s {
    uint32_t locked;      /* status of the mutex */
    uint32_t w_lock;      /* lock for the waiter list below */
    struct tcb_s* w_list; /* waiter list */
} mutex_t;

/**
 * @brief static mutex initialization
 */
#define MUTEX_INIT \
    { .locked = 0, .w_lock = 0, .w_list = NULL }

/**
 * @brief unlock the mutex and call vanish without using stack
 */
void mutex_unlock_vanish(mutex_t* mp);

#endif
