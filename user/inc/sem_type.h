/**
 * @file sem.h
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief semaphore structure
 * @date 2023-02-22
 *
 */
#ifndef SEM_TYPE_H
#define SEM_TYPE_H

#include <cond.h>
#include <mutex.h>

/**
 * @brief semaphore structure
 */
typedef struct sem_s {
    int value;
    mutex_t value_lock; /* mutex to protect the value */
    cond_t value_cv;    /* cv to wait for value increment */
} sem_t;

#endif
