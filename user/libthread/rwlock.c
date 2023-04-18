/**
 * @file rwlock.c
 * @author Hanjie Wu (hanjiew)
 * @author Devang Acharya (devanga)
 * @brief RW Lock implementation
 * @date 2023-02-22
 *
 */

#include <assert.h>
#include <rwlock.h>

/**
 * @brief initialize the lock pointed to by rwlock
 *
 * @param rwlock
 * @return int
 */
int rwlock_init(rwlock_t* rwlock) {
    rwlock->status = RWLOCK_STATUS_UNLOCK;
    rwlock->num_reader = 0;
    mutex_init(&rwlock->status_lock);
    cond_init(&rwlock->reader_cv);
    cond_init(&rwlock->writer_cv);
    return 0;
}

/**
 * @brief lock a rwlock
 */
void rwlock_lock(rwlock_t* rwlock, int type) {
    mutex_lock(&rwlock->status_lock);
    if (type == RWLOCK_READ) {
        while (rwlock->status == RWLOCK_STATUS_WRITE) {
            /* wait for the write to release */
            cond_wait(&rwlock->reader_cv, &rwlock->status_lock);
        }
        rwlock->status = RWLOCK_STATUS_READ;
        rwlock->num_reader++;
    } else {
        while (rwlock->status != RWLOCK_STATUS_UNLOCK) {
            affirm(rwlock->status == RWLOCK_STATUS_WRITE);
            /* wait for readers to release */
            cond_wait(&rwlock->writer_cv, &rwlock->status_lock);
        }
        rwlock->status = RWLOCK_STATUS_WRITE;
    }
    mutex_unlock(&rwlock->status_lock);
}

/**
 * @brief unlock a rwlock
 */
void rwlock_unlock(rwlock_t* rwlock) {
    mutex_lock(&rwlock->status_lock);
    if (rwlock->status == RWLOCK_STATUS_READ) {
        rwlock->num_reader--;
        /* the last reader should notify the writers */
        if (rwlock->num_reader == 0) {
            rwlock->status = RWLOCK_STATUS_UNLOCK;
            cond_broadcast(&rwlock->writer_cv);
        }
    } else {
        affirm(rwlock->status == RWLOCK_STATUS_WRITE);
        rwlock->status = RWLOCK_STATUS_UNLOCK;
        /* notify all readers */
        cond_broadcast(&rwlock->reader_cv);
    }
    mutex_unlock(&rwlock->status_lock);
}

/**
 * @brief destroy a rwlock
 */
void rwlock_destroy(rwlock_t* rwlock) {
    affirm(rwlock->status == RWLOCK_STATUS_UNLOCK);
    mutex_destroy(&rwlock->status_lock);
    cond_destroy(&rwlock->reader_cv);
    cond_destroy(&rwlock->writer_cv);
}

/**
 * @brief downgrade a rwlock
 */
void rwlock_downgrade(rwlock_t* rwlock) {
    affirm(rwlock->status == RWLOCK_STATUS_WRITE);
    rwlock->status = RWLOCK_STATUS_READ;
    cond_broadcast(&rwlock->reader_cv);
}
