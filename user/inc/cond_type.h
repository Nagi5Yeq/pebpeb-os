/**
 * @file cond_type.h
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief cv structure
 * @date 2023-02-22
 *
 */
#ifndef COND_TYPE_H
#define COND_TYPE_H

#include <mutex.h>

/**
 * @brief a node in condition vairable wait queue
 */
typedef struct cond_node_s {
    int tid;
    int signaled; /* is this thread signaled by othres */
    struct cond_node_s* next;
    struct cond_node_s* prev;
} cond_node_t;

/**
 * @brief condition vairable
 */
typedef struct cond_s {
    mutex_t lock;
    cond_node_t* queue; /* waiters on this cv */
} cond_t;

#endif
