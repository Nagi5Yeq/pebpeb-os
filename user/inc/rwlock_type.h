/**
 * @file rwlock_type.h
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief rwlock structure
 * @date 2023-02-22
 *
 */
#ifndef RWLOCK_TYPE_H
#define RWLOCK_TYPE_H

#include <cond.h>
#include <mutex.h>

/**
 * @brief indicates that the rwlock is unlocked
 */
#define RWLOCK_STATUS_UNLOCK 0

/**
 * @brief indicates that the rwlock is locked by at least 1 reader
 */
#define RWLOCK_STATUS_READ 1

/**
 * @brief indicates that the rwlock is locked by a writer
 */
#define RWLOCK_STATUS_WRITE 2

/**
 * @brief rwlock structure
 */
typedef struct rwlock_s {
    mutex_t status_lock; /* mutex to protect other structures */
    int num_reader;      /* number of readers holding this rwlock */
    cond_t reader_cv;    /* cv to wait for read availability */
    cond_t writer_cv;    /* cv to wait for write availability */
    int status;          /* status of this rwlock */
} rwlock_t;

#endif
